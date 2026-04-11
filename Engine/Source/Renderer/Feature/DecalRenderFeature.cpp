#include "Renderer/Feature/DecalRenderFeature.h"

#include "Component/DecalComponent.h"
#include "Core/Paths.h"
#include "Debug/EngineLog.h"
#include "Level/Level.h"
#include "Level/SceneRenderPacket.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderStateManager.h"
#include "Renderer/Shader.h"
#include "Renderer/ShaderMap.h"

#include <cfloat>

namespace
{
    constexpr uint32 GDefaultClusterTileSize = 16;
    constexpr uint32 GDefaultClusterSliceCount = 16;
    constexpr UINT GDecalPassConstantBufferSlot = 7;

    uint32 CeilDivideU32(uint32 A, uint32 B)
    {
        return (A + B - 1u) / B;
    }

    std::wstring BuildShaderPath(const wchar_t *FileName)
    {
        std::wstring ShaderDirW = FPaths::ShaderDir().wstring();
        return ShaderDirW + FileName;
    }

    void BuildUnitCubeGeometry(TArray<FVertex> &OutVertices, TArray<uint16> &OutIndices)
    {
        OutVertices.clear();
        OutIndices.clear();

        const FVector Positions[8] = {
            FVector(-0.5f, -0.5f, -0.5f), FVector(0.5f, -0.5f, -0.5f), FVector(0.5f, 0.5f, -0.5f),
            FVector(-0.5f, 0.5f, -0.5f),  FVector(-0.5f, -0.5f, 0.5f),  FVector(0.5f, -0.5f, 0.5f),
            FVector(0.5f, 0.5f, 0.5f),    FVector(-0.5f, 0.5f, 0.5f)};

        OutVertices.reserve(8);
        for (const FVector &Position : Positions)
        {
            FVertex Vertex = {};
            Vertex.Position = Position;
            Vertex.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
            Vertex.Normal = FVector(0.0f, 0.0f, 1.0f);
            Vertex.UV = FVector2(0.0f, 0.0f);
            OutVertices.push_back(Vertex);
        }

        const uint16 Indices[] = {
            0, 2, 1, 0, 3, 2, // back
            4, 5, 6, 4, 6, 7, // front
            0, 1, 5, 0, 5, 4, // bottom
            3, 7, 6, 3, 6, 2, // top
            0, 4, 7, 0, 7, 3, // left
            1, 2, 6, 1, 6, 5  // right
        };
        OutIndices.insert(OutIndices.end(), std::begin(Indices), std::end(Indices));
    }

    ID3D11ShaderResourceView *ResolveDecalTextureSRV(FMaterial *Material, ID3D11ShaderResourceView *DefaultSRV)
    {
        if (Material)
        {
            std::shared_ptr<FMaterialTexture> MaterialTexture = Material->GetMaterialTexture();
            if (MaterialTexture && MaterialTexture->TextureSRV)
            {
                return MaterialTexture->TextureSRV;
            }
        }

        return DefaultSRV;
    }

    ID3D11SamplerState *ResolveDecalTextureSampler(FMaterial *Material, ID3D11SamplerState *FallbackSampler)
    {
        if (Material)
        {
            std::shared_ptr<FMaterialTexture> MaterialTexture = Material->GetMaterialTexture();
            if (MaterialTexture && MaterialTexture->SamplerState)
            {
                return MaterialTexture->SamplerState;
            }
        }

        return FallbackSampler;
    }
}

bool FDecalRenderFeature::Initialize(FRenderer &Renderer)
{
    ID3D11Device *Device = Renderer.GetDevice();
    if (!Device)
    {
        return false;
    }

    if (DecalMaterial && DecalVolumeMaterial)
    {
        return true;
    }

    const std::wstring DecalVSPath = BuildShaderPath(L"DecalVertexShader.hlsl");
    const std::wstring DecalPSPath = BuildShaderPath(L"DecalPixelShader.hlsl");
    const std::wstring VolumeVSPath = BuildShaderPath(L"VertexShader.hlsl");
    const std::wstring VolumePSPath = BuildShaderPath(L"DecalVolumePixelShader.hlsl");
    const std::wstring FallbackVSPath = BuildShaderPath(L"VertexShader.hlsl");
    const std::wstring FallbackPSPath = BuildShaderPath(L"TexturePixelShader.hlsl");

    if (!Renderer.ShaderManager.LoadVertexShader(Device, DecalVSPath.c_str()))
    {
        Renderer.ShaderManager.LoadVertexShader(Device, FallbackVSPath.c_str());
    }
    if (!Renderer.ShaderManager.LoadPixelShader(Device, DecalPSPath.c_str()))
    {
        Renderer.ShaderManager.LoadPixelShader(Device, FallbackPSPath.c_str());
    }

    auto VS = FShaderMap::Get().GetOrCreateVertexShader(
        Device, FPaths::FileExists(DecalVSPath.c_str()) ? DecalVSPath.c_str() : FallbackVSPath.c_str());
    auto PS = FShaderMap::Get().GetOrCreatePixelShader(
        Device, FPaths::FileExists(DecalPSPath.c_str()) ? DecalPSPath.c_str() : FallbackPSPath.c_str());
    if (!Renderer.ShaderManager.LoadVertexShader(Device, VolumeVSPath.c_str()))
    {
        return false;
    }
    if (!Renderer.ShaderManager.LoadPixelShader(Device, VolumePSPath.c_str()))
    {
        return false;
    }

    auto VolumeVS = FShaderMap::Get().GetOrCreateVertexShader(Device, VolumeVSPath.c_str());
    auto VolumePS = FShaderMap::Get().GetOrCreatePixelShader(Device, VolumePSPath.c_str());

    if (!VS || !PS || !VolumeVS || !VolumePS)
    {
        return false;
    }

    auto &RenderStateManager = Renderer.GetRenderStateManager();
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
    RasterizerOption.CullMode = D3D11_CULL_NONE;
    auto RasterizerState = RenderStateManager->GetOrCreateRasterizerState(RasterizerOption);
    DecalMaterial->SetRasterizerOption(RasterizerOption);
    DecalMaterial->SetRasterizerState(RasterizerState);

    FDepthStencilStateOption DepthStencilOption;
    DepthStencilOption.DepthEnable = false;
    DepthStencilOption.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    DepthStencilOption.DepthFunc = D3D11_COMPARISON_ALWAYS;
    auto DepthStencilState = RenderStateManager->GetOrCreateDepthStencilState(DepthStencilOption);
    DecalMaterial->SetDepthStencilOption(DepthStencilOption);
    DecalMaterial->SetDepthStencilState(DepthStencilState);

    FBlendStateOption BlendOption;
    BlendOption.BlendEnable = true;
    BlendOption.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendOption.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendOption.BlendOp = D3D11_BLEND_OP_ADD;
    BlendOption.SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendOption.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    BlendOption.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    auto BlendState = RenderStateManager->GetOrCreateBlendState(BlendOption);
    DecalMaterial->SetBlendOption(BlendOption);
    DecalMaterial->SetBlendState(BlendState);

    DecalVolumeMaterial = std::make_shared<FMaterial>();
    DecalVolumeMaterial->SetOriginName("M_DecalVolume");
    DecalVolumeMaterial->SetVertexShader(VolumeVS);
    DecalVolumeMaterial->SetPixelShader(VolumePS);

    FRasterizerStateOption VolumeRasterizerOption;
    VolumeRasterizerOption.FillMode = D3D11_FILL_SOLID;
    VolumeRasterizerOption.CullMode = D3D11_CULL_BACK;
    auto VolumeRasterizerState = RenderStateManager->GetOrCreateRasterizerState(VolumeRasterizerOption);
    DecalVolumeMaterial->SetRasterizerOption(VolumeRasterizerOption);
    DecalVolumeMaterial->SetRasterizerState(VolumeRasterizerState);

    FDepthStencilStateOption VolumeDepthStencilOption;
    VolumeDepthStencilOption.DepthEnable = true;
    VolumeDepthStencilOption.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    VolumeDepthStencilOption.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    auto VolumeDepthStencilState = RenderStateManager->GetOrCreateDepthStencilState(VolumeDepthStencilOption);
    DecalVolumeMaterial->SetDepthStencilOption(VolumeDepthStencilOption);
    DecalVolumeMaterial->SetDepthStencilState(VolumeDepthStencilState);

    FBlendStateOption VolumeBlendOption;
    VolumeBlendOption.BlendEnable = true;
    VolumeBlendOption.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    VolumeBlendOption.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    VolumeBlendOption.BlendOp = D3D11_BLEND_OP_ADD;
    VolumeBlendOption.SrcBlendAlpha = D3D11_BLEND_ONE;
    VolumeBlendOption.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    VolumeBlendOption.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    auto VolumeBlendState = RenderStateManager->GetOrCreateBlendState(VolumeBlendOption);
    DecalVolumeMaterial->SetBlendOption(VolumeBlendOption);
    DecalVolumeMaterial->SetBlendState(VolumeBlendState);

    const int32 VolumeSlotIndex = DecalVolumeMaterial->CreateConstantBuffer(Device, 16);
    if (VolumeSlotIndex >= 0)
    {
        DecalVolumeMaterial->RegisterParameter("BaseColor", VolumeSlotIndex, 0, 16);
        const float VolumeColor[4] = {1.0f, 0.1f, 0.1f, 0.2f};
        DecalVolumeMaterial->GetConstantBuffer(VolumeSlotIndex)->SetData(VolumeColor, sizeof(VolumeColor));
    }

    if (!CreateRenderResources(Renderer))
    {
        ReleaseRenderResources();
        DecalMaterial.reset();
        DecalVolumeMaterial.reset();
        return false;
    }

    FMaterialManager::Get().Register("M_Decal", DecalMaterial);
    FMaterialManager::Get().Register("M_DecalVolume", DecalVolumeMaterial);
    return true;
}

void FDecalRenderFeature::Release()
{
    ReleaseRenderResources();
    DecalMaterial.reset();
    DecalVolumeMaterial.reset();
    ClusterGrid.Reset();
}

bool FDecalRenderFeature::CreateRenderResources(FRenderer &Renderer)
{
    ID3D11Device *Device = Renderer.GetDevice();
    if (!Device)
    {
        return false;
    }

    if (!DecalPassConstantBuffer)
    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth = (sizeof(FDecalPassConstants) + 15u) & ~15u;
        Desc.Usage = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(Device->CreateBuffer(&Desc, nullptr, &DecalPassConstantBuffer)))
        {
            return false;
        }
    }

    if (!UnitCubeVertexBuffer || !UnitCubeIndexBuffer)
    {
        TArray<FVertex> Vertices;
        TArray<uint16> Indices;
        BuildUnitCubeGeometry(Vertices, Indices);

        D3D11_BUFFER_DESC VBDesc = {};
        VBDesc.ByteWidth = static_cast<UINT>(Vertices.size() * sizeof(FVertex));
        VBDesc.Usage = D3D11_USAGE_IMMUTABLE;
        VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA VBInitData = {};
        VBInitData.pSysMem = Vertices.data();
        if (FAILED(Device->CreateBuffer(&VBDesc, &VBInitData, &UnitCubeVertexBuffer)))
        {
            return false;
        }

        D3D11_BUFFER_DESC IBDesc = {};
        IBDesc.ByteWidth = static_cast<UINT>(Indices.size() * sizeof(uint16));
        IBDesc.Usage = D3D11_USAGE_IMMUTABLE;
        IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA IBInitData = {};
        IBInitData.pSysMem = Indices.data();
        if (FAILED(Device->CreateBuffer(&IBDesc, &IBInitData, &UnitCubeIndexBuffer)))
        {
            return false;
        }

        UnitCubeIndexCount = static_cast<uint32>(Indices.size());
    }

    if (!PointSamplerState)
    {
        D3D11_SAMPLER_DESC SamplerDesc = {};
        SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        SamplerDesc.MinLOD = 0;
        SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(Device->CreateSamplerState(&SamplerDesc, &PointSamplerState)))
        {
            return false;
        }
    }

    if (!LinearSamplerState)
    {
        D3D11_SAMPLER_DESC SamplerDesc = {};
        SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        SamplerDesc.MinLOD = 0;
        SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(Device->CreateSamplerState(&SamplerDesc, &LinearSamplerState)))
        {
            return false;
        }
    }

    if (!DefaultWhiteTextureSRV && !CreateDefaultWhiteTexture(Device))
    {
        return false;
    }

    return true;
}

bool FDecalRenderFeature::CreateDefaultWhiteTexture(ID3D11Device *Device)
{
    if (!Device)
    {
        return false;
    }

    constexpr uint32 WhitePixel = 0xFFFFFFFFu;

    D3D11_TEXTURE2D_DESC TextureDesc = {};
    TextureDesc.Width = 1;
    TextureDesc.Height = 1;
    TextureDesc.MipLevels = 1;
    TextureDesc.ArraySize = 1;
    TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA InitData = {};
    InitData.pSysMem = &WhitePixel;
    InitData.SysMemPitch = sizeof(uint32);

    ID3D11Texture2D *Texture = nullptr;
    if (FAILED(Device->CreateTexture2D(&TextureDesc, &InitData, &Texture)))
    {
        return false;
    }

    const HRESULT Hr = Device->CreateShaderResourceView(Texture, nullptr, &DefaultWhiteTextureSRV);
    Texture->Release();
    return SUCCEEDED(Hr);
}

void FDecalRenderFeature::ReleaseRenderResources()
{
    if (DecalPassConstantBuffer)
    {
        DecalPassConstantBuffer->Release();
        DecalPassConstantBuffer = nullptr;
    }
    if (UnitCubeVertexBuffer)
    {
        UnitCubeVertexBuffer->Release();
        UnitCubeVertexBuffer = nullptr;
    }
    if (UnitCubeIndexBuffer)
    {
        UnitCubeIndexBuffer->Release();
        UnitCubeIndexBuffer = nullptr;
    }
    UnitCubeIndexCount = 0;

    if (DefaultWhiteTextureSRV)
    {
        DefaultWhiteTextureSRV->Release();
        DefaultWhiteTextureSRV = nullptr;
    }
    if (PointSamplerState)
    {
        PointSamplerState->Release();
        PointSamplerState = nullptr;
    }
    if (LinearSamplerState)
    {
        LinearSamplerState->Release();
        LinearSamplerState = nullptr;
    }
}

bool FDecalRenderFeature::Render(FRenderer &Renderer,
                                 ID3D11RenderTargetView *RenderTargetView,
                                 ID3D11DepthStencilView *DepthStencilView,
                                 const FDecalRenderRequest &Request)
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

    bool bResult = true;
    if (Request.bRenderProjectedDecals)
    {
        bResult = RenderDecalPass(Renderer, RenderTargetView, DepthStencilView, Request) && bResult;
    }
    if (Request.bShowDecalVolumes)
    {
        bResult = RenderDecalVolumePass(Renderer, RenderTargetView, DepthStencilView, Request) && bResult;
    }

    return bResult;
}

bool FDecalRenderFeature::BuildClusterGrid(const FDecalRenderRequest &Request)
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

bool FDecalRenderFeature::RenderDecalVolumePass(FRenderer &Renderer,
                                                ID3D11RenderTargetView *RenderTargetView,
                                                ID3D11DepthStencilView *DepthStencilView,
                                                const FDecalRenderRequest &Request)
{
    ID3D11DeviceContext *DeviceContext = Renderer.GetDeviceContext();
    if (!DeviceContext || !Request.SceneView || !DecalVolumeMaterial || !UnitCubeVertexBuffer || !UnitCubeIndexBuffer)
    {
        return false;
    }

    if (!Request.ScenePacket || Request.ScenePacket->DecalPrimitives.empty())
    {
        return true;
    }

    Renderer.ViewMatrix = Request.SceneView->ViewMatrix;
    Renderer.ProjectionMatrix = Request.SceneView->ProjectionMatrix;

    DeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);
    DeviceContext->RSSetViewports(1, &Request.Viewport);

    auto &RenderStateManager = Renderer.GetRenderStateManager();
    if (RenderStateManager)
    {
        RenderStateManager->BindState(DecalVolumeMaterial->GetRasterizerState());
        RenderStateManager->BindState(DecalVolumeMaterial->GetDepthStencilState());
        RenderStateManager->BindState(DecalVolumeMaterial->GetBlendState());
    }

    DecalVolumeMaterial->Bind(DeviceContext);
    Renderer.SetConstantBuffers();
    Renderer.UpdateFrameConstantBuffer();

    const UINT Stride = sizeof(FVertex);
    const UINT Offset = 0;
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DeviceContext->IASetVertexBuffers(0, 1, &UnitCubeVertexBuffer, &Stride, &Offset);
    DeviceContext->IASetIndexBuffer(UnitCubeIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    for (const FSceneDecalPrimitive &Primitive : Request.ScenePacket->DecalPrimitives)
    {
        UDecalComponent *DecalComponent = Primitive.Component;
        if (!DecalComponent)
        {
            continue;
        }

        // Use the exact same decal world transform as the projected decal pass so
        // the debug volume matches the actual decal space, including rotation.
        const FMatrix DecalWorld = DecalComponent->GetDecalToWorldMatrix();
        const FVector DecalWorldPosition = DecalComponent->GetWorldLocation();
        const FMatrix WorldToDecal = DecalWorld.GetInverse();
        const FVector CameraLocalPosition = WorldToDecal.TransformPosition(Request.SceneView->CameraPosition);
        const bool bCameraInsideDecal =
            std::abs(CameraLocalPosition.X) <= 0.5f &&
            std::abs(CameraLocalPosition.Y) <= 0.5f &&
            std::abs(CameraLocalPosition.Z) <= 0.5f;

        static FVector GLastLoggedVolumeWorldPosition = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
        static FVector GLastLoggedVolumeCameraPosition = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
        const bool bWorldPositionChanged = !DecalWorldPosition.Equals(GLastLoggedVolumeWorldPosition, 0.1f);
        const bool bCameraPositionChanged = !Request.SceneView->CameraPosition.Equals(GLastLoggedVolumeCameraPosition, 0.1f);
        if (bWorldPositionChanged || bCameraPositionChanged)
        {
            UE_LOG(
                "[DecalDebug] Volume world=(%.2f, %.2f, %.2f) camera=(%.2f, %.2f, %.2f) localCamera=(%.2f, %.2f, %.2f) inside=%s",
                DecalWorldPosition.X,
                DecalWorldPosition.Y,
                DecalWorldPosition.Z,
                Request.SceneView->CameraPosition.X,
                Request.SceneView->CameraPosition.Y,
                Request.SceneView->CameraPosition.Z,
                CameraLocalPosition.X,
                CameraLocalPosition.Y,
                CameraLocalPosition.Z,
                bCameraInsideDecal ? "yes" : "no");
            GLastLoggedVolumeWorldPosition = DecalWorldPosition;
            GLastLoggedVolumeCameraPosition = Request.SceneView->CameraPosition;
        }

        if (RenderStateManager)
        {
            FRasterizerStateOption RasterizerOption = DecalVolumeMaterial->GetRasterizerOption();
            RasterizerOption.CullMode = bCameraInsideDecal ? D3D11_CULL_FRONT : D3D11_CULL_BACK;
            RenderStateManager->BindState(RenderStateManager->GetOrCreateRasterizerState(RasterizerOption));
        }

        Renderer.UpdateObjectConstantBuffer(DecalWorld);
        DeviceContext->DrawIndexed(UnitCubeIndexCount, 0, 0);
    }

    if (RenderStateManager)
    {
        RenderStateManager->RebindState();
    }

    Renderer.SetConstantBuffers();
    return true;
}

bool FDecalRenderFeature::RenderDecalPass(FRenderer &Renderer,
                                          ID3D11RenderTargetView *RenderTargetView,
                                          ID3D11DepthStencilView *DepthStencilView,
                                          const FDecalRenderRequest &Request)
{
    ID3D11DeviceContext *DeviceContext = Renderer.GetDeviceContext();
    if (!DeviceContext || !Request.ScenePacket || !Request.SceneView || !DecalMaterial || !DecalPassConstantBuffer || !UnitCubeVertexBuffer ||
        !UnitCubeIndexBuffer)
    {
        return false;
    }

    ID3D11ShaderResourceView *SceneDepthSRV = Request.SceneDepthSRV;
    const uint32 ViewWidth = static_cast<uint32>(Request.Viewport.Width);
    const uint32 ViewHeight = static_cast<uint32>(Request.Viewport.Height);
    const uint32 DecalCount = static_cast<uint32>(Request.ScenePacket->DecalPrimitives.size());

    static uint32 GLastLoggedDecalCount = static_cast<uint32>(-1);
    static bool GLastLoggedHasDepthSRV = false;
    static uint32 GLastLoggedViewWidth = static_cast<uint32>(-1);
    static uint32 GLastLoggedViewHeight = static_cast<uint32>(-1);
    const bool bHasDepthSRV = (SceneDepthSRV != nullptr);
    if (GLastLoggedDecalCount != DecalCount || GLastLoggedHasDepthSRV != bHasDepthSRV ||
        GLastLoggedViewWidth != ViewWidth || GLastLoggedViewHeight != ViewHeight)
    {
        UE_LOG("[DecalDebug] Decal pass request: decals=%u depthSRV=%s viewport=%ux%u",
               DecalCount,
               bHasDepthSRV ? "yes" : "no",
               ViewWidth,
               ViewHeight);
        GLastLoggedDecalCount = DecalCount;
        GLastLoggedHasDepthSRV = bHasDepthSRV;
        GLastLoggedViewWidth = ViewWidth;
        GLastLoggedViewHeight = ViewHeight;
    }

    if (!SceneDepthSRV || Request.ScenePacket->DecalPrimitives.empty())
    {
        return true;
    }

    Renderer.ViewMatrix = Request.SceneView->ViewMatrix;
    Renderer.ProjectionMatrix = Request.SceneView->ProjectionMatrix;

    // The current depth texture is sampled as an SRV in the pixel shader, so keep the writable DSV unbound here.
    DeviceContext->OMSetRenderTargets(1, &RenderTargetView, nullptr);
    DeviceContext->RSSetViewports(1, &Request.Viewport);

    auto &RenderStateManager = Renderer.GetRenderStateManager();
    if (RenderStateManager)
    {
        RenderStateManager->BindState(DecalMaterial->GetDepthStencilState());
        RenderStateManager->BindState(DecalMaterial->GetBlendState());
    }

    const UINT Stride = sizeof(FVertex);
    const UINT Offset = 0;
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DeviceContext->IASetVertexBuffers(0, 1, &UnitCubeVertexBuffer, &Stride, &Offset);
    DeviceContext->IASetIndexBuffer(UnitCubeIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    const FMatrix ViewProjection = Request.SceneView->ViewMatrix * Request.SceneView->ProjectionMatrix;
    const FMatrix InvViewProj = ViewProjection.GetInverse();

    uint32 DrawnDecalCount = 0;
    for (const FSceneDecalPrimitive &Primitive : Request.ScenePacket->DecalPrimitives)
    {
        UDecalComponent *DecalComponent = Primitive.Component;
        if (!DecalComponent)
        {
            continue;
        }

        FMaterial *AssignedMaterial = DecalComponent->GetDecalMaterial();
        const FMatrix DecalWorld = DecalComponent->GetDecalToWorldMatrix();
        const FMatrix WorldToDecal = DecalWorld.GetInverse();
        const FVector CameraLocalPosition = WorldToDecal.TransformPosition(Request.SceneView->CameraPosition);
        const bool bCameraInsideDecal =
            std::abs(CameraLocalPosition.X) <= 0.5f &&
            std::abs(CameraLocalPosition.Y) <= 0.5f &&
            std::abs(CameraLocalPosition.Z) <= 0.5f;

        if (RenderStateManager)
        {
            FRasterizerStateOption RasterizerOption = DecalMaterial->GetRasterizerOption();
            RasterizerOption.CullMode = bCameraInsideDecal ? D3D11_CULL_FRONT : D3D11_CULL_BACK;
            RenderStateManager->BindState(RenderStateManager->GetOrCreateRasterizerState(RasterizerOption));
        }

        FDecalPassConstants Constants = {};
        Constants.InvViewProj = InvViewProj.GetTransposed();
        Constants.DecalWorld = DecalWorld.GetTransposed();
        Constants.WorldToDecal = WorldToDecal.GetTransposed();
        Constants.ViewProjection = ViewProjection.GetTransposed();
        Constants.ScreenSize = FVector4(
            Request.Viewport.Width,
            Request.Viewport.Height,
            Request.Viewport.Width > 0.0f ? 1.0f / Request.Viewport.Width : 0.0f,
            Request.Viewport.Height > 0.0f ? 1.0f / Request.Viewport.Height : 0.0f);
        Constants.DecalColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

        D3D11_MAPPED_SUBRESOURCE Mapped = {};
        if (FAILED(DeviceContext->Map(DecalPassConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
        {
            continue;
        }
        memcpy(Mapped.pData, &Constants, sizeof(Constants));
        DeviceContext->Unmap(DecalPassConstantBuffer, 0);

        DecalMaterial->Bind(DeviceContext);

        ID3D11Buffer *DecalCB = DecalPassConstantBuffer;
        DeviceContext->VSSetConstantBuffers(GDecalPassConstantBufferSlot, 1, &DecalCB);
        DeviceContext->PSSetConstantBuffers(GDecalPassConstantBufferSlot, 1, &DecalCB);

        ID3D11ShaderResourceView *DepthSRV = SceneDepthSRV;
        DeviceContext->PSSetShaderResources(0, 1, &DepthSRV);

        ID3D11SamplerState *DepthSampler = PointSamplerState;
        DeviceContext->PSSetSamplers(0, 1, &DepthSampler);

        ID3D11ShaderResourceView *DecalSRV = ResolveDecalTextureSRV(AssignedMaterial, DefaultWhiteTextureSRV);
        DeviceContext->PSSetShaderResources(1, 1, &DecalSRV);

        ID3D11SamplerState *DecalSampler = ResolveDecalTextureSampler(AssignedMaterial, LinearSamplerState);
        DeviceContext->PSSetSamplers(1, 1, &DecalSampler);

        DeviceContext->DrawIndexed(UnitCubeIndexCount, 0, 0);
        ++DrawnDecalCount;
    }

    static uint32 GLastLoggedDrawnDecalCount = static_cast<uint32>(-1);
    if (GLastLoggedDrawnDecalCount != DrawnDecalCount)
    {
        UE_LOG("[DecalDebug] Decal pass draws issued: %u", DrawnDecalCount);
        GLastLoggedDrawnDecalCount = DrawnDecalCount;
    }

    ID3D11ShaderResourceView *NullSRVs[2] = {nullptr, nullptr};
    DeviceContext->PSSetShaderResources(0, 2, NullSRVs);
    DeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);

    if (RenderStateManager)
    {
        RenderStateManager->RebindState();
    }

    Renderer.SetConstantBuffers();
    return true;
}
