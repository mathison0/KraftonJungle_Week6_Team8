#include "Renderer/Feature/DecalRenderFeature.h"

#include <WICTextureLoader.h>

#include "Level/SceneRenderPacket.h"
#include "Core/Paths.h"
#include "Renderer/Material.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/Renderer.h"
#include "Renderer/ShaderMap.h"

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
	if (DepthCopySRV)
	{
		DepthCopySRV->Release();
		DepthCopySRV = nullptr;
	}
	if (DepthCopyTexture)
	{
		DepthCopyTexture->Release();
		DepthCopyTexture = nullptr;
	}
	DepthCopyWidth = 0;
	DepthCopyHeight = 0;
	Device = nullptr;
	DeviceContext = nullptr;
	DepthTextureSRV = nullptr;
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

	DepthTextureSRV = nullptr;

	Stats.TotalDecalCount = static_cast<int32>(Packet.DecalPrimitives.size());
	Stats.VisibleDecalCount = Stats.TotalDecalCount;

	ClusterGrid.TilesX = (Viewport.Width > 0.0f)
		? static_cast<int32>((static_cast<uint32>(Viewport.Width) + static_cast<uint32>(ClusterGrid.TileSizeX) - 1u) / static_cast<uint32>(ClusterGrid.TileSizeX))
		: 0;
	ClusterGrid.TilesY = (Viewport.Height > 0.0f)
		? static_cast<int32>((static_cast<uint32>(Viewport.Height) + static_cast<uint32>(ClusterGrid.TileSizeY) - 1u) / static_cast<uint32>(ClusterGrid.TileSizeY))
		: 0;

	const int32 TileCount = ClusterGrid.TilesX * ClusterGrid.TilesY;
	const int32 ClusterCount = TileCount * ClusterGrid.DepthSlices;
	if (ClusterCount > 0)
	{
		ClusterGrid.ClusterDecalIndices.resize(ClusterCount);
		for (TArray<int32>& Cluster : ClusterGrid.ClusterDecalIndices)
		{
			Cluster.clear();
		}
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

ID3D11ShaderResourceView* FDecalRenderFeature::GetDepthTextureSRV() const
{
	return DepthTextureSRV;
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

	const FVector4 White(1.0f, 1.0f, 1.0f, 1.0f);
	const FVector Corners[8] =
	{
		FVector(0.0f, -SizeY, -SizeZ),
		FVector(0.0f, -SizeY, SizeZ),
		FVector(0.0f, SizeY, -SizeZ),
		FVector(0.0f, SizeY, SizeZ),
		FVector(SizeX, -SizeY, -SizeZ),
		FVector(SizeX, -SizeY, SizeZ),
		FVector(SizeX, SizeY, -SizeZ),
		FVector(SizeX, SizeY, SizeZ)
	};

	for (const FVector& Corner : Corners)
	{
		OutMesh.Vertices.push_back({ Corner, White, FVector::ZeroVector, FVector2(0.0f, 0.0f) });
	}

	const uint32 BoxIndices[] =
	{
		0u, 2u, 3u, 0u, 3u, 1u,
		4u, 5u, 7u, 4u, 7u, 6u,
		0u, 1u, 5u, 0u, 5u, 4u,
		2u, 6u, 7u, 2u, 7u, 3u,
		0u, 4u, 6u, 0u, 6u, 2u,
		1u, 3u, 7u, 1u, 7u, 5u
	};

	OutMesh.Indices.insert(OutMesh.Indices.end(), std::begin(BoxIndices), std::end(BoxIndices));

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

	const std::wstring ShaderDir = FPaths::ShaderDir();
	const std::wstring VSPath = ShaderDir + L"DecalVertexShader.hlsl";
	const std::wstring PSPath = ShaderDir + L"DecalPixelShader.hlsl";

	auto VS = FShaderMap::Get().GetOrCreateVertexShader(Device, VSPath.c_str());
	auto PS = FShaderMap::Get().GetOrCreatePixelShader(Device, PSPath.c_str());
	if (!VS || !PS)
	{
		return false;
	}

	BaseMaterial = std::make_shared<FMaterial>();
	if (!BaseMaterial)
	{
		return false;
	}

	BaseMaterial->SetOriginName("M_DecalBase");

	BaseMaterial->SetVertexShader(VS);
	BaseMaterial->SetPixelShader(PS);

	FRasterizerStateOption RasterizerOption;
	RasterizerOption.FillMode = D3D11_FILL_SOLID;
	RasterizerOption.CullMode = D3D11_CULL_FRONT;
	BaseMaterial->SetRasterizerOption(RasterizerOption);
	BaseMaterial->SetRasterizerState(Renderer.GetRenderStateManager()->GetOrCreateRasterizerState(RasterizerOption));

	FDepthStencilStateOption DepthOption;
	DepthOption.DepthEnable = false;
	DepthOption.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	DepthOption.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	BaseMaterial->SetDepthStencilOption(DepthOption);
	BaseMaterial->SetDepthStencilState(Renderer.GetRenderStateManager()->GetOrCreateDepthStencilState(DepthOption));

	FBlendStateOption BlendOption;
	BlendOption.BlendEnable = true;
	BlendOption.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	BlendOption.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	BlendOption.BlendOp = D3D11_BLEND_OP_ADD;
	BlendOption.SrcBlendAlpha = D3D11_BLEND_ONE;
	BlendOption.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	BlendOption.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	BaseMaterial->SetBlendOption(BlendOption);
	BaseMaterial->SetBlendState(Renderer.GetRenderStateManager()->GetOrCreateBlendState(BlendOption));

	const int32 SlotIndex = BaseMaterial->CreateConstantBuffer(Device, 48);
	if (SlotIndex >= 0)
	{
		BaseMaterial->RegisterParameter("BaseColor", SlotIndex, 0, sizeof(FVector4));
		BaseMaterial->RegisterParameter("DecalExtent", SlotIndex, 16, sizeof(FVector4));
		BaseMaterial->RegisterParameter("ScreenSize", SlotIndex, 32, sizeof(FVector4));
	}

	const int32 MatrixSlotIndex = BaseMaterial->CreateConstantBuffer(Device, sizeof(FVector4) * 4);
	if (MatrixSlotIndex >= 0)
	{
		BaseMaterial->RegisterParameter("DecalOrigin", MatrixSlotIndex, 0, sizeof(FVector4));
		BaseMaterial->RegisterParameter("DecalAxisX", MatrixSlotIndex, 16, sizeof(FVector4));
		BaseMaterial->RegisterParameter("DecalAxisY", MatrixSlotIndex, 32, sizeof(FVector4));
		BaseMaterial->RegisterParameter("DecalAxisZ", MatrixSlotIndex, 48, sizeof(FVector4));
	}
	const FVector4 DefaultColor(1.0f, 1.0f, 1.0f, 1.0f);
	const FVector4 DefaultExtent(1.0f, 0.5f, 0.5f, 0.0f);
	const FVector4 DefaultScreenSize(1.0f, 1.0f, 1.0f, 1.0f);
	BaseMaterial->SetParameterData("BaseColor", &DefaultColor, sizeof(FVector4));
	BaseMaterial->SetParameterData("DecalExtent", &DefaultExtent, sizeof(FVector4));
	BaseMaterial->SetParameterData("ScreenSize", &DefaultScreenSize, sizeof(FVector4));

	return true;
}

bool FDecalRenderFeature::UpdateDepthCopy(FRenderer& Renderer, ID3D11DepthStencilView* SourceDepthDSV)
{
	if (!bInitialized && !Initialize(Renderer))
	{
		return false;
	}

	DepthTextureSRV = nullptr;
	if (!SourceDepthDSV || !DeviceContext)
	{
		return false;
	}

	ID3D11Resource* DepthResource = nullptr;
	SourceDepthDSV->GetResource(&DepthResource);
	if (!DepthResource)
	{
		return false;
	}

	ID3D11Texture2D* DepthTexture = nullptr;
	const bool bSucceeded =
		SUCCEEDED(DepthResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&DepthTexture))) &&
		DepthTexture != nullptr;
	DepthResource->Release();
	if (!bSucceeded)
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC Desc = {};
	DepthTexture->GetDesc(&Desc);
	if (EnsureDepthCopyResources(Desc.Width, Desc.Height))
	{
		DeviceContext->CopyResource(DepthCopyTexture, DepthTexture);
		DepthTextureSRV = DepthCopySRV;
	}
	DepthTexture->Release();
	return DepthTextureSRV != nullptr;
}

bool FDecalRenderFeature::EnsureDepthCopyResources(uint32 Width, uint32 Height)
{
	if (!Device || Width == 0 || Height == 0)
	{
		return false;
	}

	if (DepthCopyTexture && DepthCopySRV && DepthCopyWidth == Width && DepthCopyHeight == Height)
	{
		return true;
	}

	if (DepthCopySRV)
	{
		DepthCopySRV->Release();
		DepthCopySRV = nullptr;
	}
	if (DepthCopyTexture)
	{
		DepthCopyTexture->Release();
		DepthCopyTexture = nullptr;
	}

	D3D11_TEXTURE2D_DESC Desc = {};
	Desc.Width = Width;
	Desc.Height = Height;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(Device->CreateTexture2D(&Desc, nullptr, &DepthCopyTexture)) || !DepthCopyTexture)
	{
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.MipLevels = 1;
	if (FAILED(Device->CreateShaderResourceView(DepthCopyTexture, &SRVDesc, &DepthCopySRV)) || !DepthCopySRV)
	{
		DepthCopyTexture->Release();
		DepthCopyTexture = nullptr;
		return false;
	}

	DepthCopyWidth = Width;
	DepthCopyHeight = Height;
	return true;
}

void FDecalRenderFeature::ResetPreparedState()
{
	Stats.Reset();
	ClusterGrid.Reset();
}

void FDecalRenderFeature::BindDepthSRVToCommands(TArray<FRenderCommand>& Commands) const
{
	if (!DepthTextureSRV)
	{
		return;
	}
	for (FRenderCommand& Command : Commands)
	{
		if (Command.Material)
		{
			Command.Material->SetPixelTextureBinding(1, DepthTextureSRV, nullptr);
		}
	}
}