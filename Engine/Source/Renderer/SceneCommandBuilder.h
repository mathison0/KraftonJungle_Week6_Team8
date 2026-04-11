#pragma once

#include "CoreMinimal.h"
#include "Level/SceneRenderPacket.h"
#include "Renderer/RenderFeatureInterfaces.h"

class FMaterial;
class FDynamicMaterial;
struct FDynamicMesh;
struct FRenderCommandQueue;
class UBillboardComponent;
class USubUVComponent;
class UDecalComponent;

/**
 * Scene packet -> render command 변환에 필요한 frame-local 서비스 집합.
 */
struct ENGINE_API FSceneCommandBuildContext
{
	FMaterial* DefaultMaterial = nullptr;
	ISceneTextFeature* TextFeature = nullptr;
	ISceneSubUVFeature* SubUVFeature = nullptr;
	ISceneBillboardFeature* BillboardFeature = nullptr;
	ISceneDecalFeature* DecalFeature = nullptr;
	FVector2 ViewportSize = FVector2(0.0f, 0.0f);
	float TotalTimeSeconds = 0.0f;
};

class ENGINE_API FSceneCommandBuilder
{
public:
	void BuildQueue(
		const FSceneCommandBuildContext& BuildContext,
		const FSceneRenderPacket& Packet,
		const FVector& CameraPosition,
		FRenderCommandQueue& OutQueue);

private:
	FMaterial* GetOrCreateTextMaterial(const FSceneCommandBuildContext& BuildContext, const FVector4& TextColor);
	FMaterial* GetOrCreateSubUVMaterial(const FSceneCommandBuildContext& BuildContext, const USubUVComponent* Component);
	FMaterial* GetOrCreateDecalMaterial(const FSceneCommandBuildContext& BuildContext, const UDecalComponent* Component);

	void PruneStaleSubUVMaterials(const TArray<const USubUVComponent*>& ActiveComponents);
	void PruneStaleDecalMaterials(const TArray<const UDecalComponent*>& ActiveComponents);

	static uint32 ToColorKey(const FVector4& Color);
	static void UpdateSubUVMaterialParams(
		FMaterial& Material,
		int32 Columns,
		int32 Rows,
		int32 CurrentFrame);

private:
	TMap<uint32, std::shared_ptr<FDynamicMaterial>> TextMaterialsByColor;
	TMap<const USubUVComponent*, std::shared_ptr<FDynamicMaterial>> SubUVMaterialsByComponent;
	TMap<const UDecalComponent*, std::shared_ptr<FDynamicMaterial>> DecalMaterialsByComponent;
	TMap<const UDecalComponent*, std::shared_ptr<FDynamicMesh>> DecalMeshesByComponent;
};
