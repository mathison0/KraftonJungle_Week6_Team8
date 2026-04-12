#pragma once

#include "CoreMinimal.h"
#include "Level/SceneRenderPacket.h"
#include "Renderer/DecalCommandBuilder.h"
#include "Renderer/SceneCommandBuildContext.h"

class FMaterial;
class FDynamicMaterial;
struct FDynamicMesh;
struct FRenderCommandQueue;
class UBillboardComponent;
class USubUVComponent;

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

	void PruneStaleSubUVMaterials(const TArray<const USubUVComponent*>& ActiveComponents);

	static uint32 ToColorKey(const FVector4& Color);
	static void UpdateSubUVMaterialParams(
		FMaterial& Material,
		int32 Columns,
		int32 Rows,
		int32 CurrentFrame);

private:
	TMap<uint32, std::shared_ptr<FDynamicMaterial>> TextMaterialsByColor;
	TMap<const USubUVComponent*, std::shared_ptr<FDynamicMaterial>> SubUVMaterialsByComponent;
	FDecalCommandBuilder DecalCommandBuilder;
};
