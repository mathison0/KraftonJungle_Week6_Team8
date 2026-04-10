#include "Renderer/Feature/DecalRenderFeature.h"

#include "Level/SceneRenderPacket.h"
#include "Renderer/Material.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/Renderer.h"

bool FDecalRenderFeature::Initialize(FRenderer& Renderer)
{
	if (bInitialized)
	{
		return true;
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

bool FDecalRenderFeature::BuildMesh(const FVector& Extent, FRenderMesh& OutMesh) const
{
	(void)Extent;
	(void)OutMesh;
	return false;
}

bool FDecalRenderFeature::InitializeBaseMaterial(FRenderer& Renderer)
{
	if (BaseMaterial)
	{
		return true;
	}

	FMaterial* DefaultMaterial = Renderer.GetDefaultMaterial();
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
