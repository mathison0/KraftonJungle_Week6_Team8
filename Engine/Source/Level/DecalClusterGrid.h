#pragma once

#include "CoreMinimal.h"
#include "Math/Vector.h"
#include <d3d11.h>

class ULevel;
class UPrimitiveComponent;
class UDecalComponent;
struct FSceneDecalPrimitive;
struct FSceneViewRenderRequest;

struct ENGINE_API FDecalClusterCell
{
    TArray<UDecalComponent *> Decals;
    TArray<UPrimitiveComponent *> Objects; // optional
};

struct ENGINE_API FDecalClusterRange
{
    int32 MinX = 0;
    int32 MinY = 0;
    int32 MinZ = 0;
    int32 MaxX = -1;
    int32 MaxY = -1;
    int32 MaxZ = -1;

    bool IsValid() const
    {
        return MinX <= MaxX && MinY <= MaxY && MinZ <= MaxZ;
    }
};

class ENGINE_API FDecalClusterGrid
{
  public:
    void Reset();
    void Initialize(uint32 InCountX, uint32 InCountY, uint32 InCountZ);

    void BuildDecalLists(const FSceneViewRenderRequest &SceneView, const D3D11_VIEWPORT &Viewport,
                         const TArray<FSceneDecalPrimitive> &DecalPrimitives);

    void BuildObjectLists(const FSceneViewRenderRequest &SceneView, const D3D11_VIEWPORT &Viewport, ULevel *Level);

    int32 GetCellIndex(uint32 X, uint32 Y, uint32 Z) const;
    FDecalClusterCell *GetCell(uint32 X, uint32 Y, uint32 Z);
    const FDecalClusterCell *GetCell(uint32 X, uint32 Y, uint32 Z) const;

    uint32 GetCountX() const
    {
        return CountX;
    }
    uint32 GetCountY() const
    {
        return CountY;
    }
    uint32 GetCountZ() const
    {
        return CountZ;
    }

  private:
    FDecalClusterRange ComputeClusterRangeForDecal(const FSceneViewRenderRequest &SceneView,
                                                   const D3D11_VIEWPORT &Viewport,
                                                   UDecalComponent *DecalComponent) const;

  private:
    uint32 CountX = 0;
    uint32 CountY = 0;
    uint32 CountZ = 0;
    TArray<FDecalClusterCell> Cells;
};