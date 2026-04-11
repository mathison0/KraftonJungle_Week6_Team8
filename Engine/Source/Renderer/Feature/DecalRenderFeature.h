#pragma once

#include "CoreMinimal.h"
#include "Level/DecalClusterGrid.h"
#include "Renderer/Vertex.h"
#include <d3d11.h>

class FRenderer;
class ULevel;
class FMaterial;
struct FSceneRenderPacket;
struct FSceneViewRenderRequest;

struct FDecalPassConstants
{
    FMatrix InvViewProj;
    FMatrix DecalWorld;
    FMatrix WorldToDecal;
    FMatrix DecalWorldViewProjection;
    FVector4 ScreenSize;
    FVector4 DecalColor;
};

struct ENGINE_API FDecalRenderRequest
{
    const FSceneRenderPacket *ScenePacket = nullptr;
    const FSceneViewRenderRequest *SceneView = nullptr;
    ULevel *Level = nullptr;
    D3D11_VIEWPORT Viewport = {};
    bool bEnableClusterBuild = true;
    bool bEnableClusterObjectCache = false;
};

class ENGINE_API FDecalRenderFeature
{
  public:
    bool Initialize(FRenderer &Renderer);
    void Release();

    bool Render(FRenderer &Renderer, ID3D11RenderTargetView *RenderTargetView, ID3D11DepthStencilView *DepthStencilView,
                const FDecalRenderRequest &Request);

  private:
    bool CreateRenderResources(FRenderer &Renderer);
    bool CreateDefaultWhiteTexture(ID3D11Device *Device);
    void ReleaseRenderResources();
    bool BuildClusterGrid(const FDecalRenderRequest &Request);
    bool RenderDecalPass(FRenderer &Renderer, ID3D11RenderTargetView *RenderTargetView,
                         ID3D11DepthStencilView *DepthStencilView, const FDecalRenderRequest &Request);

  private:
    FDecalClusterGrid ClusterGrid;
    std::shared_ptr<FMaterial> DecalMaterial;
    ID3D11Buffer *DecalPassConstantBuffer = nullptr;
    ID3D11Buffer *UnitCubeVertexBuffer = nullptr;
    ID3D11Buffer *UnitCubeIndexBuffer = nullptr;
    uint32 UnitCubeIndexCount = 0;
    ID3D11ShaderResourceView *DefaultWhiteTextureSRV = nullptr;
    ID3D11SamplerState *PointSamplerState = nullptr;
    ID3D11SamplerState *LinearSamplerState = nullptr;
};
