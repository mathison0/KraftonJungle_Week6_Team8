#pragma once

#include "CoreMinimal.h"
#include "Renderer/RenderFeatureInterfaces.h"

#include <d3d11.h>
#include <memory>

class FRenderer;
class FMaterial;
struct FMaterialTexture;
struct FSceneRenderPacket;
struct FSceneViewRenderRequest;
struct FRenderMesh;

struct ENGINE_API FDecalPassStats
{
	int32 TotalDecalCount = 0;
	int32 VisibleDecalCount = 0;
	int32 CulledDecalCount = 0;
	int32 SceneBVHVisitedNodeCount = 0;
	int32 SceneBVHCulledNodeCount = 0;
	int32 ReceiverPrimitiveCount = 0;
	int32 MeshBVHVisitedNodeCount = 0;
	int32 ClusterAssignmentCount = 0;
	int32 MaxClusterDecalCount = 0;
	int32 RenderedDecalCount = 0;
	int32 DrawCallCount = 0;
	float CpuTimeMs = 0.0f;

	void Reset()
	{
		TotalDecalCount = 0;
		VisibleDecalCount = 0;
		CulledDecalCount = 0;
		SceneBVHVisitedNodeCount = 0;
		SceneBVHCulledNodeCount = 0;
		ReceiverPrimitiveCount = 0;
		MeshBVHVisitedNodeCount = 0;
		ClusterAssignmentCount = 0;
		MaxClusterDecalCount = 0;
		RenderedDecalCount = 0;
		DrawCallCount = 0;
		CpuTimeMs = 0.0f;
	}
};

struct ENGINE_API FDecalScreenClusterGrid
{
	int32 TileSizeX = 16;
	int32 TileSizeY = 16;
	int32 TilesX = 0;
	int32 TilesY = 0;
	int32 DepthSlices = 16;
	TArray<TArray<int32>> ClusterDecalIndices;

	void Reset()
	{
		TilesX = 0;
		TilesY = 0;
		ClusterDecalIndices.clear();
	}
};

class ENGINE_API FDecalRenderFeature final : public ISceneDecalFeature
{
public:
	bool Initialize(FRenderer& Renderer);
	void Release();

	void BeginFrame();
	bool PrepareFrame(
		FRenderer& Renderer,
		const FSceneRenderPacket& Packet,
		const FSceneViewRenderRequest& SceneView,
		const D3D11_VIEWPORT& Viewport);
	bool UpdateDepthCopy(FRenderer& Renderer, ID3D11DepthStencilView* SourceDepthDSV);
	bool Render(FRenderer& Renderer);

	FMaterial* GetBaseMaterial() const override;
	bool BuildMesh(const FVector& Extent, FRenderMesh& OutMesh) const override;
	std::shared_ptr<FMaterialTexture> GetOrLoadTexture(const std::wstring& Path) override;
	ID3D11ShaderResourceView* GetDepthTextureSRV() const override;

	const FDecalPassStats& GetStats() const { return Stats; }
	FDecalPassStats& GetMutableStats() { return Stats; }
	const FDecalScreenClusterGrid& GetClusterGrid() const { return ClusterGrid; }
	FDecalScreenClusterGrid& GetMutableClusterGrid() { return ClusterGrid; }

	void BindDepthSRVToCommands(TArray<FRenderCommand>& Commands) const override;

private:
	bool InitializeBaseMaterial(FRenderer& Renderer);
	bool EnsureDepthCopyResources(uint32 Width, uint32 Height);
	void ResetPreparedState();

private:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	ID3D11ShaderResourceView* DepthTextureSRV = nullptr;
	ID3D11Texture2D* DepthCopyTexture = nullptr;
	ID3D11ShaderResourceView* DepthCopySRV = nullptr;
	uint32 DepthCopyWidth = 0;
	uint32 DepthCopyHeight = 0;
	std::shared_ptr<FMaterial> BaseMaterial;
	TMap<std::wstring, std::shared_ptr<FMaterialTexture>> TextureCache;
	FDecalPassStats Stats;
	FDecalScreenClusterGrid ClusterGrid;
	bool bInitialized = false;
};
