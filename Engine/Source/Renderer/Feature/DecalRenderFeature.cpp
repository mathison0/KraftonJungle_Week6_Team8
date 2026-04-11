#include "Renderer/Feature/DecalRenderFeature.h"

#include "Component/DecalComponent.h"
#include "Core/Paths.h"
#include "Level/Level.h"
#include "Level/SceneRenderPacket.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderStateManager.h"
#include "Renderer/Shader.h"
#include "Renderer/ShaderMap.h"

namespace
{
    constexpr uint32 GDefaultClusterTileSize = 16;
    constexpr uint32 GDefaultClusterSliceCount = 16;

    uint32 CeilDivideU32(uint32 A, uint32 B)
    {
        return (A + B - 1u) / B;
    }

    std::wstring BuildShaderPath(const wchar_t* FileName)
    {
        std::wstring ShaderDirW = FPaths::ShaderDir().wstring();
        return ShaderDirW + FileName;
    }
}

bool FDecalRenderFeature::Initialize(FRenderer& Renderer)
{
    ID3D11Device* Device = Renderer.GetDevice();
    if (!Device)
    {
        return false;
    }

    // 이미 생성되어 있으면 재사용한다.
    if (DecalMaterial)
    {
        return true;
    }

    const std::wstring DecalVSPath = BuildShaderPath(L"DecalVertexShader.hlsl");
    const std::wstring DecalPSPath = BuildShaderPath(L"DecalPixelShader.hlsl");
    const std::wstring FallbackVSPath = BuildShaderPath(L"VertexShader.hlsl");
    const std::wstring FallbackPSPath = BuildShaderPath(L"TexturePixelShader.hlsl");

    // 데칼 전용 셰이더가 있으면 그걸 쓰고, 아직 없으면 일단 기본 경로로 fallback 한다.
    if (!Renderer.ShaderManager.LoadVertexShader(Device, DecalVSPath.c_str()))
    {
        Renderer.ShaderManager.LoadVertexShader(Device, FallbackVSPath.c_str());
    }
    if (!Renderer.ShaderManager.LoadPixelShader(Device, DecalPSPath.c_str()))
    {
        Renderer.ShaderManager.LoadPixelShader(Device, FallbackPSPath.c_str());
    }

    auto VS = FShaderMap::Get().GetOrCreateVertexShader(
        Device,
        FPaths::FileExists(DecalVSPath.c_str()) ? DecalVSPath.c_str() : FallbackVSPath.c_str());
    auto PS = FShaderMap::Get().GetOrCreatePixelShader(
        Device,
        FPaths::FileExists(DecalPSPath.c_str()) ? DecalPSPath.c_str() : FallbackPSPath.c_str());

    if (!VS || !PS)
    {
        return false;
    }

    auto& RenderStateManager = Renderer.GetRenderStateManager();
    if (!RenderStateManager)
    {
        return false;
    }

    DecalMaterial = std::make_shared<FMaterial>();
    DecalMaterial->SetOriginName("M_Decal");
    DecalMaterial->SetVertexShader(VS);
    DecalMaterial->SetPixelShader(PS);

    FRasterizerStateOption RasterizerOption;
    RasterizerOption.FillMode = D3D11_FILL_SOLID;
    RasterizerOption.CullMode = D3D11_CULL_BACK;
    auto RasterizerState = RenderStateManager->GetOrCreateRasterizerState(RasterizerOption);
    DecalMaterial->SetRasterizerOption(RasterizerOption);
    DecalMaterial->SetRasterizerState(RasterizerState);

    // screen-space decal pass는 depth test는 켜되 depth write는 하지 않는 쪽이 일반적이다.
    FDepthStencilStateOption DepthStencilOption;
    DepthStencilOption.DepthEnable = true;
    DepthStencilOption.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    auto DepthStencilState = RenderStateManager->GetOrCreateDepthStencilState(DepthStencilOption);
    DecalMaterial->SetDepthStencilOption(DepthStencilOption);
    DecalMaterial->SetDepthStencilState(DepthStencilState);

    // 데칼 전용 상수 버퍼 슬롯을 하나 확보해 둔다.
    // 실제 파라미터 이름/오프셋은 사용하는 픽셀 셰이더에 맞게 바꾸면 된다.
    const int32 SlotIndex = DecalMaterial->CreateConstantBuffer(Device, 64);
    if (SlotIndex >= 0)
    {
        DecalMaterial->RegisterParameter("DecalColor", SlotIndex, 0, 16);
        const float DefaultColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        DecalMaterial->GetConstantBuffer(SlotIndex)->SetData(DefaultColor, sizeof(DefaultColor));
    }

    FMaterialManager::Get().Register("M_Decal", DecalMaterial);
    return true;
}

void FDecalRenderFeature::Release()
{
    DecalMaterial.reset();
    ClusterGrid.Reset();
}

bool FDecalRenderFeature::Render(FRenderer& Renderer,
                                 ID3D11RenderTargetView* RenderTargetView,
                                 ID3D11DepthStencilView* DepthStencilView,
                                 const FDecalRenderRequest& Request)
{
    if (!Request.ScenePacket || !Request.SceneView || !RenderTargetView || !DepthStencilView)
    {
        return false;
    }

    if (!Initialize(Renderer))
    {
        return false;
    }

    if (Request.bEnableClusterBuild && !BuildClusterGrid(Request))
    {
        return false;
    }

    return RenderDecalPass(Renderer, RenderTargetView, DepthStencilView, Request);
}

bool FDecalRenderFeature::BuildClusterGrid(const FDecalRenderRequest& Request)
{
    if (!Request.ScenePacket || Request.Viewport.Width <= 0.0f || Request.Viewport.Height <= 0.0f)
    {
        ClusterGrid.Reset();
        return false;
    }

    const uint32 ViewWidth = static_cast<uint32>(Request.Viewport.Width);
    const uint32 ViewHeight = static_cast<uint32>(Request.Viewport.Height);

    const uint32 ClusterCountX = CeilDivideU32(ViewWidth, GDefaultClusterTileSize);
    const uint32 ClusterCountY = CeilDivideU32(ViewHeight, GDefaultClusterTileSize);
    const uint32 ClusterCountZ = GDefaultClusterSliceCount;

    ClusterGrid.Initialize(ClusterCountX, ClusterCountY, ClusterCountZ);
    ClusterGrid.BuildDecalLists(*Request.SceneView, Request.Viewport, Request.ScenePacket->DecalPrimitives);

    if (Request.bEnableClusterObjectCache && Request.Level)
    {
        ClusterGrid.BuildObjectLists(*Request.SceneView, Request.Viewport, Request.Level);
    }

    return true;
}

bool FDecalRenderFeature::RenderDecalPass(FRenderer& Renderer,
                                          ID3D11RenderTargetView* RenderTargetView,
                                          ID3D11DepthStencilView* DepthStencilView,
                                          const FDecalRenderRequest& Request)
{
    ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();
    if (!DeviceContext || !Request.ScenePacket)
    {
        return false;
    }

    // 현재 데칼 pass는 scene color / scene depth를 대상으로 덧그리는 후행 pass다.
    DeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);
    DeviceContext->RSSetViewports(1, &Request.Viewport);

    // 현재 RenderDevice 구조에서는 decal shader가 scene depth를 읽을 수 있어야 한다.
    // scene depth SRV가 아직 준비되지 않았다면 pass 자체는 성공으로 끝내되 실제 효과는 생략한다.
    ID3D11ShaderResourceView* SceneDepthSRV = Renderer.GetRenderDevice().GetSceneDepthSRV();
    if (!SceneDepthSRV)
    {
        return true;
    }

    // NOTE:
    // 이 지점부터 실제 draw를 하려면 FRenderer의 private helper들에 접근할 수 있어야 한다.
    // 지금 헤더에는 friend class FDecalRenderFeature가 없어서 다음 작업을 직접 호출할 수 없다.
    // 1) Renderer.SetConstantBuffers()
    // 2) Renderer.ViewMatrix / ProjectionMatrix 설정
    // 3) Renderer.UpdateFrameConstantBuffer()
    // 4) Renderer.UpdateObjectConstantBuffer()
    // 따라서 현재 구현은 'pass orchestration + cluster build + RTV/DSV/viewport 바인딩'까지 제공하고,
    // 실제 draw submission은 아래 TODO로 분리해 둔다.

    for (const FSceneDecalPrimitive& Primitive : Request.ScenePacket->DecalPrimitives)
    {
        UDecalComponent* DecalComponent = Primitive.Component;
        if (!DecalComponent)
        {
            continue;
        }

        FMaterial* Material = DecalComponent->GetDecalMaterial();
        if (!Material)
        {
            Material = DecalMaterial.get();
        }

        if (!Material)
        {
            continue;
        }

        // TODO:
        // - WorldToDecal / InvViewProj 상수 버퍼 업데이트
        // - SceneDepthSRV 바인딩
        // - decal box mesh 또는 fullscreen triangle draw
        // - 블렌드 상태 설정
        //
        // 현재 단계에서는 데이터 준비만 검증한다.
        (void)Material;
        (void)DecalComponent;
    }

    // pass 종료 시 depth SRV 바인딩 충돌을 피하려면 실제로 PSSetShaderResources를 사용한 뒤 해제해야 한다.
    return true;
}
