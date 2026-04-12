#pragma once

#include "CoreMinimal.h"
#include "Renderer/DecalCulling.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/SceneCommandBuildContext.h"

#include <memory>

class FDynamicMaterial;
class UDecalComponent;
struct FDynamicMesh;
struct FMaterialTexture;

class ENGINE_API FDecalCommandBuilder
{
public:
	FMaterial* GetOrCreateDecalMaterial(
		const FSceneCommandBuildContext& BuildContext,
		const UDecalComponent* Component);

	bool BuildDecalCommand(
		const FSceneCommandBuildContext& BuildContext,
		UDecalComponent* DecalComponent,
		const FClusteredDecalAssignment* ClusterAssignment,
		int32 DecalIndex,
		FRenderCommand& OutCommand,
		int32& OutClusterAssignmentCount,
		FDecalScreenClusterGrid* ClusterGrid);

	void PruneStaleDecalResources(const TArray<const UDecalComponent*>& ActiveComponents);

private:
	TMap<const UDecalComponent*, std::shared_ptr<FDynamicMaterial>> DecalMaterialsByComponent;
	TMap<const UDecalComponent*, std::shared_ptr<FDynamicMesh>> DecalMeshesByComponent;
};
