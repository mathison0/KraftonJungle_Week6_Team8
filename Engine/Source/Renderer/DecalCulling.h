#pragma once

#include "CoreMinimal.h"
#include "Renderer/Feature/DecalRenderFeature.h"
#include "Renderer/SceneCommandBuildContext.h"

#include <d3d11.h>
#include <unordered_set>

class UDecalComponent;
class UPrimitiveComponent;
class ULevel;

struct ENGINE_API FClusteredDecalAssignment
{
	bool bHasClusters = false;
	bool bUseScissorRect = false;
	D3D11_RECT ScissorRect = {};
	int32 TileMinX = 0;
	int32 TileMaxX = -1;
	int32 TileMinY = 0;
	int32 TileMaxY = -1;
	int32 SliceMin = 0;
	int32 SliceMax = -1;
};

struct ENGINE_API FDecalCullResult
{
	bool bHasReceiver = false;
	bool bHasClusters = false;
	FClusteredDecalAssignment ClusterAssignment;
};

namespace FDecalCulling
{
	bool CanPrimitiveReceiveDecal(const UPrimitiveComponent* Primitive);

	FDecalCullResult CullDecal(
		ULevel* SceneLevel,
		const UDecalComponent& DecalComponent,
		const FSceneCommandBuildContext& BuildContext,
		const std::unordered_set<const UPrimitiveComponent*>& VisibleReceivers,
		const FDecalScreenClusterGrid* ClusterGrid);

	void AssignDecalToClusters(
		FDecalScreenClusterGrid& ClusterGrid,
		int32 DecalIndex,
		const FClusteredDecalAssignment& Assignment,
		int32& OutClusterAssignmentCount);
}
