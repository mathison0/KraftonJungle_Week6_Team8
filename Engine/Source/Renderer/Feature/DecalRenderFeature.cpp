#include "Renderer/Feature/DecalRenderFeature.h"

#include <WICTextureLoader.h>

#include "Level/SceneRenderPacket.h"
#include "Renderer/Material.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/Renderer.h"

#include <algorithm>

bool FDecalRenderFeature::Initialize(FRenderer& Renderer)
{
	if (bInitialized)
	{
		return true;
	}

	Device = Renderer.GetDevice();
	DeviceContext = Renderer.GetDeviceContext();
	if (!Device || !DeviceContext)
	{
		return false;
	}

	if (!InitializeBaseMaterial(Renderer))
	{
		return false;
	}

	bInitialized = true;
	return true;
}

void FDecalRenderFeature::Release()
{
	BaseMaterial.reset();
	TextureCache.clear();
	Device = nullptr;
	DeviceContext = nullptr;
	ResetPreparedState();
	bInitialized = false;
}

void FDecalRenderFeature::BeginFrame()
{
	ResetPreparedState();
}

bool FDecalRenderFeature::PrepareFrame(
	FRenderer& Renderer,
	const FSceneRenderPacket& Packet,
	const FSceneViewRenderRequest& SceneView,
	const D3D11_VIEWPORT& Viewport)
{
	(void)SceneView;

	if (!bInitialized && !Initialize(Renderer))
	{
		return false;
	}

	Stats.TotalDecalCount = static_cast<int32>(Packet.DecalPrimitives.size());
	Stats.VisibleDecalCount = Stats.TotalDecalCount;

	ClusterGrid.TilesX = (Viewport.Width > 0.0f)
		? static_cast<int32>((static_cast<uint32>(Viewport.Width) + static_cast<uint32>(ClusterGrid.TileSizeX) - 1u) / static_cast<uint32>(ClusterGrid.TileSizeX))
		: 0;
	ClusterGrid.TilesY = (Viewport.Height > 0.0f)
		? static_cast<int32>((static_cast<uint32>(Viewport.Height) + static_cast<uint32>(ClusterGrid.TileSizeY) - 1u) / static_cast<uint32>(ClusterGrid.TileSizeY))
		: 0;

	const int32 TileCount = ClusterGrid.TilesX * ClusterGrid.TilesY;
	if (TileCount > 0)
	{
		ClusterGrid.TileDecalIndices.resize(TileCount);
	}

	return true;
}

bool FDecalRenderFeature::Render(FRenderer& Renderer)
{
	(void)Renderer;
	return bInitialized;
}

FMaterial* FDecalRenderFeature::GetBaseMaterial() const
{
	return BaseMaterial.get();
}

std::shared_ptr<FMaterialTexture> FDecalRenderFeature::GetOrLoadTexture(const std::wstring& Path)
{
	if (Path.empty() || !Device || !DeviceContext)
	{
		return nullptr;
	}

	auto Found = TextureCache.find(Path);
	if (Found != TextureCache.end())
	{
		return Found->second;
	}

	ID3D11ShaderResourceView* SRV = nullptr;
	HRESULT Hr = DirectX::CreateWICTextureFromFile(Device, DeviceContext, Path.c_str(), nullptr, &SRV);
	if (FAILED(Hr) || !SRV)
	{
		return nullptr;
	}

	ID3D11SamplerState* Sampler = nullptr;
	D3D11_SAMPLER_DESC SamplerDesc = {};
	SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SamplerDesc.MinLOD = 0;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	Hr = Device->CreateSamplerState(&SamplerDesc, &Sampler);
	if (FAILED(Hr) || !Sampler)
	{
		SRV->Release();
		return nullptr;
	}

	std::shared_ptr<FMaterialTexture> MaterialTexture = std::make_shared<FMaterialTexture>();
	MaterialTexture->TextureSRV = SRV;
	MaterialTexture->SamplerState = Sampler;
	TextureCache[Path] = MaterialTexture;
	return MaterialTexture;
}

bool FDecalRenderFeature::BuildMesh(const FVector& Extent, FRenderMesh& OutMesh) const
{
	const float SizeX = (std::max)(0.0f, Extent.X);
	const float SizeY = (std::max)(0.0f, Extent.Y);
	const float SizeZ = (std::max)(0.0f, Extent.Z);
	if (SizeX <= 0.0f || SizeY <= 0.0f || SizeZ <= 0.0f)
	{
		OutMesh.Vertices.clear();
		OutMesh.Indices.clear();
		OutMesh.Sections.clear();
		OutMesh.Topology = EMeshTopology::EMT_TriangleList;
		return false;
	}

	OutMesh.Vertices.clear();
	OutMesh.Indices.clear();
	OutMesh.Sections.clear();
	OutMesh.Topology = EMeshTopology::EMT_TriangleList;

	const FVector P000(0.0f, -SizeY, -SizeZ);
	const FVector P001(0.0f, -SizeY, SizeZ);
	const FVector P010(0.0f, SizeY, -SizeZ);
	const FVector P011(0.0f, SizeY, SizeZ);
	const FVector P100(SizeX, -SizeY, -SizeZ);
	const FVector P101(SizeX, -SizeY, SizeZ);
	const FVector P110(SizeX, SizeY, -SizeZ);
	const FVector P111(SizeX, SizeY, SizeZ);

	const FVector4 White(1.0f, 1.0f, 1.0f, 1.0f);

	auto AddFace = [&](const FVector& Normal, const FVector& A, const FVector& B, const FVector& C, const FVector& D)
	{
		const uint32 BaseIndex = static_cast<uint32>(OutMesh.Vertices.size());

		FVertex V0 = { A, White, Normal, FVector2(0.0f, 0.0f) };
		FVertex V1 = { B, White, Normal, FVector2(1.0f, 0.0f) };
		FVertex V2 = { C, White, Normal, FVector2(1.0f, 1.0f) };
		FVertex V3 = { D, White, Normal, FVector2(0.0f, 1.0f) };

		OutMesh.Vertices.push_back(V0);
		OutMesh.Vertices.push_back(V1);
		OutMesh.Vertices.push_back(V2);
		OutMesh.Vertices.push_back(V3);

		OutMesh.Indices.push_back(BaseIndex + 0u);
		OutMesh.Indices.push_back(BaseIndex + 1u);
		OutMesh.Indices.push_back(BaseIndex + 2u);
		OutMesh.Indices.push_back(BaseIndex + 0u);
		OutMesh.Indices.push_back(BaseIndex + 2u);
		OutMesh.Indices.push_back(BaseIndex + 3u);
	};

	AddFace(FVector(1.0f, 0.0f, 0.0f), P101, P111, P110, P100);
	AddFace(FVector(-1.0f, 0.0f, 0.0f), P001, P000, P010, P011);
	AddFace(FVector(0.0f, 1.0f, 0.0f), P011, P111, P110, P010);
	AddFace(FVector(0.0f, -1.0f, 0.0f), P001, P000, P100, P101);
	AddFace(FVector(0.0f, 0.0f, 1.0f), P001, P101, P111, P011);
	AddFace(FVector(0.0f, 0.0f, -1.0f), P000, P010, P110, P100);

	OutMesh.Sections.push_back({ 0u, 0u, static_cast<uint32>(OutMesh.Indices.size()) });
	OutMesh.UpdateLocalBound();
	return true;
}

bool FDecalRenderFeature::InitializeBaseMaterial(FRenderer& Renderer)
{
	if (BaseMaterial)
	{
		return true;
	}

	FMaterial* DefaultMaterial = Renderer.GetDefaultTextureMaterial();
	if (!DefaultMaterial)
	{
		return false;
	}

	BaseMaterial = DefaultMaterial->CreateDynamicMaterial();
	if (!BaseMaterial)
	{
		return false;
	}

	BaseMaterial->SetOriginName("M_DecalBase");
	return true;
}

void FDecalRenderFeature::ResetPreparedState()
{
	Stats.Reset();
	ClusterGrid.Reset();
}
