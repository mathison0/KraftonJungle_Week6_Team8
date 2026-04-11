#include "Level/DecalClusterGrid.h"

#include "Component/DecalComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Level/Level.h"
#include "Level/SceneRenderPacket.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Renderer/Renderer.h"

#include <algorithm>
#include <cmath>

namespace
{
    // 프로젝트마다 FSceneViewRenderRequest 의 행렬 필드명이 다를 수 있어서
    // 이 함수만 맞춰주면 나머지 구현은 그대로 사용할 수 있게 분리했다.
    static const FMatrix& GetViewProjectionMatrix(const FSceneViewRenderRequest& SceneView)
    {
        // 현재 구현은 아래 필드가 있다고 가정한다.
        // 이름이 다르면 여기만 고치면 된다.
        return SceneView.ViewMatrix * SceneView.ProjectionMatrix;
    }

    static FVector4 TransformPosition(const FMatrix& M, const FVector& P)
    {
        const float X = P.X;
        const float Y = P.Y;
        const float Z = P.Z;

        FVector4 Out;
        Out.X = X * M.M[0][0] + Y * M.M[1][0] + Z * M.M[2][0] + M.M[3][0];
        Out.Y = X * M.M[0][1] + Y * M.M[1][1] + Z * M.M[2][1] + M.M[3][1];
        Out.Z = X * M.M[0][2] + Y * M.M[1][2] + Z * M.M[2][2] + M.M[3][2];
        Out.W = X * M.M[0][3] + Y * M.M[1][3] + Z * M.M[2][3] + M.M[3][3];
        return Out;
    }

    static int32 ClampToIntRange(int32 Value, int32 MinValue, int32 MaxValue)
    {
        return std::max(MinValue, std::min(Value, MaxValue));
    }

    static float ClampFloat(float Value, float MinValue, float MaxValue)
    {
        return std::max(MinValue, std::min(Value, MaxValue));
    }
}

void FDecalClusterGrid::Reset()
{
    CountX = 0;
    CountY = 0;
    CountZ = 0;
    Cells.clear();
}

void FDecalClusterGrid::Initialize(uint32 InCountX, uint32 InCountY, uint32 InCountZ)
{
    CountX = InCountX;
    CountY = InCountY;
    CountZ = InCountZ;

    const size_t TotalCellCount = static_cast<size_t>(CountX) * static_cast<size_t>(CountY) * static_cast<size_t>(CountZ);
    Cells.clear();
    Cells.resize(TotalCellCount);
}

void FDecalClusterGrid::BuildDecalLists(const FSceneViewRenderRequest& SceneView,
                                        const D3D11_VIEWPORT& Viewport,
                                        const TArray<FSceneDecalPrimitive>& DecalPrimitives)
{
    if (CountX == 0 || CountY == 0 || CountZ == 0 || Cells.empty())
    {
        return;
    }

    for (FDecalClusterCell& Cell : Cells)
    {
        Cell.Decals.clear();
    }

    for (const FSceneDecalPrimitive& DecalPrimitive : DecalPrimitives)
    {
        UDecalComponent* DecalComponent = DecalPrimitive.Component;
        if (!DecalComponent)
        {
            continue;
        }

        const FDecalClusterRange Range = ComputeClusterRangeForDecal(SceneView, Viewport, DecalComponent);
        if (!Range.IsValid())
        {
            continue;
        }

        for (int32 Z = Range.MinZ; Z <= Range.MaxZ; ++Z)
        {
            for (int32 Y = Range.MinY; Y <= Range.MaxY; ++Y)
            {
                for (int32 X = Range.MinX; X <= Range.MaxX; ++X)
                {
                    if (FDecalClusterCell* Cell = GetCell(static_cast<uint32>(X), static_cast<uint32>(Y), static_cast<uint32>(Z)))
                    {
                        Cell->Decals.push_back(DecalComponent);
                    }
                }
            }
        }
    }
}

void FDecalClusterGrid::BuildObjectLists(const FSceneViewRenderRequest& SceneView,
                                         const D3D11_VIEWPORT& Viewport,
                                         ULevel* Level)
{
    (void)SceneView;
    (void)Viewport;
    (void)Level;

    // optional 경로라서 초기 구현에서는 비워 둔다.
    // 추후 아래 방식으로 확장하면 된다.
    //
    // 1. 각 cluster cell 의 월드 공간 AABB 또는 view-space slab 를 계산
    // 2. ULevel::QueryPrimitivesByAABB(...) 로 후보를 얻음
    // 3. Cell.Objects 에 저장
    //
    // 지금은 stale 데이터가 남지 않도록 clear 만 수행한다.
    for (FDecalClusterCell& Cell : Cells)
    {
        Cell.Objects.clear();
    }
}

int32 FDecalClusterGrid::GetCellIndex(uint32 X, uint32 Y, uint32 Z) const
{
    if (X >= CountX || Y >= CountY || Z >= CountZ)
    {
        return -1;
    }

    const uint32 Index = Z * (CountX * CountY) + Y * CountX + X;
    return static_cast<int32>(Index);
}

FDecalClusterCell* FDecalClusterGrid::GetCell(uint32 X, uint32 Y, uint32 Z)
{
    const int32 Index = GetCellIndex(X, Y, Z);
    if (Index < 0)
    {
        return nullptr;
    }

    return &Cells[static_cast<size_t>(Index)];
}

const FDecalClusterCell* FDecalClusterGrid::GetCell(uint32 X, uint32 Y, uint32 Z) const
{
    const int32 Index = GetCellIndex(X, Y, Z);
    if (Index < 0)
    {
        return nullptr;
    }

    return &Cells[static_cast<size_t>(Index)];
}

FDecalClusterRange FDecalClusterGrid::ComputeClusterRangeForDecal(const FSceneViewRenderRequest& SceneView,
                                                                  const D3D11_VIEWPORT& Viewport,
                                                                  UDecalComponent* DecalComponent) const
{
    FDecalClusterRange Result;

    if (!DecalComponent || CountX == 0 || CountY == 0 || CountZ == 0)
    {
        return Result;
    }

    const FBoxSphereBounds WorldBounds = DecalComponent->GetWorldBounds();
    const FVector Center = WorldBounds.Center;
    const FVector Extent = WorldBounds.BoxExtent;

    const FVector Corners[8] =
    {
        FVector(Center.X - Extent.X, Center.Y - Extent.Y, Center.Z - Extent.Z),
        FVector(Center.X + Extent.X, Center.Y - Extent.Y, Center.Z - Extent.Z),
        FVector(Center.X - Extent.X, Center.Y + Extent.Y, Center.Z - Extent.Z),
        FVector(Center.X + Extent.X, Center.Y + Extent.Y, Center.Z - Extent.Z),
        FVector(Center.X - Extent.X, Center.Y - Extent.Y, Center.Z + Extent.Z),
        FVector(Center.X + Extent.X, Center.Y - Extent.Y, Center.Z + Extent.Z),
        FVector(Center.X - Extent.X, Center.Y + Extent.Y, Center.Z + Extent.Z),
        FVector(Center.X + Extent.X, Center.Y + Extent.Y, Center.Z + Extent.Z)
    };

    const FMatrix& ViewProjection = GetViewProjectionMatrix(SceneView);

    bool bHasValidProjectedCorner = false;

    float MinNdcX = 1.0f;
    float MinNdcY = 1.0f;
    float MinNdcZ = 1.0f;
    float MaxNdcX = -1.0f;
    float MaxNdcY = -1.0f;
    float MaxNdcZ = -1.0f;

    for (const FVector& Corner : Corners)
    {
        const FVector4 Clip = TransformPosition(ViewProjection, Corner);

        // 카메라 뒤 완전 배치된 점은 제외한다.
        // 하나라도 앞쪽에 있는 점이 있으면 나머지 점과 함께 범위를 잡는다.
        if (std::abs(Clip.W) < 1.0e-6f || Clip.W <= 0.0f)
        {
            continue;
        }

        const float InvW = 1.0f / Clip.W;
        const float NdcX = Clip.X * InvW;
        const float NdcY = Clip.Y * InvW;
        const float NdcZ = Clip.Z * InvW;

        MinNdcX = std::min(MinNdcX, NdcX);
        MinNdcY = std::min(MinNdcY, NdcY);
        MinNdcZ = std::min(MinNdcZ, NdcZ);

        MaxNdcX = std::max(MaxNdcX, NdcX);
        MaxNdcY = std::max(MaxNdcY, NdcY);
        MaxNdcZ = std::max(MaxNdcZ, NdcZ);

        bHasValidProjectedCorner = true;
    }

    if (!bHasValidProjectedCorner)
    {
        return Result;
    }

    // 화면 밖으로 일부 나가더라도 cluster에 걸칠 수 있으므로 clamp 한다.
    MinNdcX = ClampFloat(MinNdcX, -1.0f, 1.0f);
    MaxNdcX = ClampFloat(MaxNdcX, -1.0f, 1.0f);
    MinNdcY = ClampFloat(MinNdcY, -1.0f, 1.0f);
    MaxNdcY = ClampFloat(MaxNdcY, -1.0f, 1.0f);

    // D3D 기준 NDC z 는 보통 [0, 1] 을 가정한다.
    MinNdcZ = ClampFloat(MinNdcZ, 0.0f, 1.0f);
    MaxNdcZ = ClampFloat(MaxNdcZ, 0.0f, 1.0f);

    const float ScreenMinX = (MinNdcX * 0.5f + 0.5f) * Viewport.Width;
    const float ScreenMaxX = (MaxNdcX * 0.5f + 0.5f) * Viewport.Width;

    // NDC Y up -> viewport Y down 보정
    const float ScreenMinY = (1.0f - (MaxNdcY * 0.5f + 0.5f)) * Viewport.Height;
    const float ScreenMaxY = (1.0f - (MinNdcY * 0.5f + 0.5f)) * Viewport.Height;

    const float CellWidth = Viewport.Width / static_cast<float>(CountX);
    const float CellHeight = Viewport.Height / static_cast<float>(CountY);
    const float CellDepth = 1.0f / static_cast<float>(CountZ);

    if (CellWidth <= 0.0f || CellHeight <= 0.0f || CellDepth <= 0.0f)
    {
        return Result;
    }

    Result.MinX = ClampToIntRange(static_cast<int32>(std::floor(ScreenMinX / CellWidth)), 0, static_cast<int32>(CountX) - 1);
    Result.MaxX = ClampToIntRange(static_cast<int32>(std::floor(ScreenMaxX / CellWidth)), 0, static_cast<int32>(CountX) - 1);

    Result.MinY = ClampToIntRange(static_cast<int32>(std::floor(ScreenMinY / CellHeight)), 0, static_cast<int32>(CountY) - 1);
    Result.MaxY = ClampToIntRange(static_cast<int32>(std::floor(ScreenMaxY / CellHeight)), 0, static_cast<int32>(CountY) - 1);

    Result.MinZ = ClampToIntRange(static_cast<int32>(std::floor(MinNdcZ / CellDepth)), 0, static_cast<int32>(CountZ) - 1);
    Result.MaxZ = ClampToIntRange(static_cast<int32>(std::floor(MaxNdcZ / CellDepth)), 0, static_cast<int32>(CountZ) - 1);

    return Result;
}
