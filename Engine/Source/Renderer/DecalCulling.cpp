#include "Renderer/DecalCulling.h"

#include <cfloat>
#include <cmath>

#include "Component/DecalComponent.h"
#include "Level/BVH.h"
#include "Level/Level.h"

namespace
{
	struct FOBB
	{
		FVector Center = FVector::ZeroVector;
		FVector AxisX = FVector::ForwardVector;
		FVector AxisY = FVector::RightVector;
		FVector AxisZ = FVector::UpVector;
		FVector Extent = FVector::ZeroVector;
	};

	static FOBB MakeDecalOBB(const UDecalComponent& Component)
	{
		const FVector Extent = Component.GetDecalExtent();
		const FMatrix Transform = Component.GetWorldTransform();

		FOBB OBB;
		OBB.AxisX = Transform.GetUnitAxis(EAxis::X);
		OBB.AxisY = Transform.GetUnitAxis(EAxis::Y);
		OBB.AxisZ = Transform.GetUnitAxis(EAxis::Z);
		OBB.Extent = Extent;
		OBB.Center = Transform.GetTranslation() + OBB.AxisX * (Extent.X * 0.5f);
		return OBB;
	}

	static bool OverlapOnAxis(const FOBB& OBB, const FAABB& AABB, const FVector& Axis)
	{
		const float AxisLengthSq = Axis.SizeSquared();
		if (AxisLengthSq <= 1.0e-8f)
		{
			return true;
		}

		const FVector TestAxis = Axis / std::sqrt(AxisLengthSq);
		const FVector AABBCenter = (AABB.PMin + AABB.PMax) * 0.5f;
		const FVector AABBExtent = (AABB.PMax - AABB.PMin) * 0.5f;

		const float OBBProjectionRadius =
			std::fabs(FVector::DotProduct(OBB.AxisX * OBB.Extent.X, TestAxis)) +
			std::fabs(FVector::DotProduct(OBB.AxisY * OBB.Extent.Y, TestAxis)) +
			std::fabs(FVector::DotProduct(OBB.AxisZ * OBB.Extent.Z, TestAxis));

		const float AABBProjectionRadius =
			std::fabs(AABBExtent.X * TestAxis.X) +
			std::fabs(AABBExtent.Y * TestAxis.Y) +
			std::fabs(AABBExtent.Z * TestAxis.Z);

		const float CenterDistance = std::fabs(FVector::DotProduct(OBB.Center - AABBCenter, TestAxis));
		return CenterDistance <= (OBBProjectionRadius + AABBProjectionRadius + 1.0e-4f);
	}

	static bool IntersectOBBAABB(const FOBB& OBB, const FAABB& AABB)
	{
		const FVector AABBAxes[3] = { FVector::ForwardVector, FVector::RightVector, FVector::UpVector };
		const FVector OBBAxes[3] = { OBB.AxisX, OBB.AxisY, OBB.AxisZ };

		for (const FVector& Axis : OBBAxes)
		{
			if (!OverlapOnAxis(OBB, AABB, Axis))
			{
				return false;
			}
		}

		for (const FVector& Axis : AABBAxes)
		{
			if (!OverlapOnAxis(OBB, AABB, Axis))
			{
				return false;
			}
		}

		for (const FVector& OBBAxis : OBBAxes)
		{
			for (const FVector& AABBAxis : AABBAxes)
			{
				if (!OverlapOnAxis(OBB, AABB, FVector::CrossProduct(OBBAxis, AABBAxis)))
				{
					return false;
				}
			}
		}

		return true;
	}

	static bool HasOverlappingDecalReceiver(
		ULevel* SceneLevel,
		const FOBB& DecalOBB,
		const std::unordered_set<const UPrimitiveComponent*>& VisibleReceivers)
	{
		if (!SceneLevel || VisibleReceivers.empty())
		{
			return false;
		}

		TArray<UPrimitiveComponent*> OverlappingPrimitives;
		SceneLevel->QueryPrimitivesByBounds(
			[&](const FAABB& Bounds)
			{
				return IntersectOBBAABB(DecalOBB, Bounds);
			},
			OverlappingPrimitives);

		return std::ranges::any_of(
			OverlappingPrimitives,
			[&VisibleReceivers](const UPrimitiveComponent* Primitive)
			{
				return Primitive != nullptr &&
					VisibleReceivers.find(Primitive) != VisibleReceivers.end();
			});
	}

	static bool BuildDecalClusterAssignment(
		const UDecalComponent& DecalComponent,
		const FSceneCommandBuildContext& BuildContext,
		const FDecalScreenClusterGrid& ClusterGrid,
		FClusteredDecalAssignment& OutAssignment)
	{
		if (BuildContext.ViewportSize.X <= 0.0f || BuildContext.ViewportSize.Y <= 0.0f ||
			ClusterGrid.TilesX <= 0 || ClusterGrid.TilesY <= 0 || ClusterGrid.DepthSlices <= 0)
		{
			return false;
		}

		const FVector Extent = DecalComponent.GetDecalExtent();
		const FMatrix WorldMatrix = DecalComponent.GetWorldTransform();
		const FMatrix ViewProjection = BuildContext.ViewMatrix * BuildContext.ProjectionMatrix;

		const FVector LocalCorners[8] =
		{
			FVector(0.0f, -Extent.Y, -Extent.Z),
			FVector(0.0f, -Extent.Y, Extent.Z),
			FVector(0.0f, Extent.Y, -Extent.Z),
			FVector(0.0f, Extent.Y, Extent.Z),
			FVector(Extent.X, -Extent.Y, -Extent.Z),
			FVector(Extent.X, -Extent.Y, Extent.Z),
			FVector(Extent.X, Extent.Y, -Extent.Z),
			FVector(Extent.X, Extent.Y, Extent.Z)
		};

		float ScreenMinX = FLT_MAX;
		float ScreenMinY = FLT_MAX;
		float ScreenMaxX = -FLT_MAX;
		float ScreenMaxY = -FLT_MAX;
		float ViewDepthMin = FLT_MAX;
		float ViewDepthMax = 0.0f;
		bool bHasProjectedCorner = false;
		bool bHasCornerBehindNearPlane = false;
		const float SafeNearPlane = (std::max)(BuildContext.NearPlane, 0.001f);
		const float SafeFarPlane = (std::max)(BuildContext.FarPlane, SafeNearPlane + 0.001f);
		const float LogDepthDenominator = std::log(SafeFarPlane / SafeNearPlane);

		for (const FVector& LocalCorner : LocalCorners)
		{
			const FVector WorldCorner = WorldMatrix.TransformPosition(LocalCorner);
			const FVector ViewCorner = BuildContext.ViewMatrix.TransformPosition(WorldCorner);
			const float ViewDepth = ViewCorner.X;
			if (ViewDepth <= SafeNearPlane)
			{
				bHasCornerBehindNearPlane = true;
			}

			const DirectX::XMVECTOR ClipCorner = DirectX::XMVector4Transform(
				DirectX::XMVectorSet(WorldCorner.X, WorldCorner.Y, WorldCorner.Z, 1.0f),
				ViewProjection.ToXMMatrix());

			const float ClipW = DirectX::XMVectorGetW(ClipCorner);
			if (ClipW <= 1.0e-5f)
			{
				bHasCornerBehindNearPlane = true;
				continue;
			}

			const float InvW = 1.0f / ClipW;
			const float NdcX = DirectX::XMVectorGetX(ClipCorner) * InvW;
			const float NdcY = DirectX::XMVectorGetY(ClipCorner) * InvW;
			const float ScreenX = (NdcX * 0.5f + 0.5f) * BuildContext.ViewportSize.X;
			const float ScreenY = (1.0f - (NdcY * 0.5f + 0.5f)) * BuildContext.ViewportSize.Y;

			ScreenMinX = (std::min)(ScreenMinX, ScreenX);
			ScreenMinY = (std::min)(ScreenMinY, ScreenY);
			ScreenMaxX = (std::max)(ScreenMaxX, ScreenX);
			ScreenMaxY = (std::max)(ScreenMaxY, ScreenY);
			if (ViewDepth > 0.0f)
			{
				ViewDepthMin = (std::min)(ViewDepthMin, ViewDepth);
				ViewDepthMax = (std::max)(ViewDepthMax, ViewDepth);
			}
			bHasProjectedCorner = true;
		}

		if (!bHasProjectedCorner || ViewDepthMin == FLT_MAX)
		{
			return false;
		}

		ScreenMinX = (std::clamp)(ScreenMinX, 0.0f, BuildContext.ViewportSize.X);
		ScreenMinY = (std::clamp)(ScreenMinY, 0.0f, BuildContext.ViewportSize.Y);
		ScreenMaxX = (std::clamp)(ScreenMaxX, 0.0f, BuildContext.ViewportSize.X);
		ScreenMaxY = (std::clamp)(ScreenMaxY, 0.0f, BuildContext.ViewportSize.Y);

		if (ScreenMaxX <= ScreenMinX || ScreenMaxY <= ScreenMinY)
		{
			return false;
		}

		OutAssignment.TileMinX = (std::clamp)(static_cast<int32>(ScreenMinX / static_cast<float>(ClusterGrid.TileSizeX)), 0, ClusterGrid.TilesX - 1);
		OutAssignment.TileMaxX = (std::clamp)(static_cast<int32>((ScreenMaxX - 1.0f) / static_cast<float>(ClusterGrid.TileSizeX)), 0, ClusterGrid.TilesX - 1);
		OutAssignment.TileMinY = (std::clamp)(static_cast<int32>(ScreenMinY / static_cast<float>(ClusterGrid.TileSizeY)), 0, ClusterGrid.TilesY - 1);
		OutAssignment.TileMaxY = (std::clamp)(static_cast<int32>((ScreenMaxY - 1.0f) / static_cast<float>(ClusterGrid.TileSizeY)), 0, ClusterGrid.TilesY - 1);

		ViewDepthMin = (std::clamp)(ViewDepthMin, SafeNearPlane, SafeFarPlane);
		ViewDepthMax = (std::clamp)(ViewDepthMax, SafeNearPlane, SafeFarPlane);

		float SliceDepthMin = 0.0f;
		float SliceDepthMax = 1.0f;
		if (BuildContext.bOrthographic || LogDepthDenominator <= 1.0e-6f)
		{
			const float LinearDenominator = SafeFarPlane - SafeNearPlane;
			SliceDepthMin = (ViewDepthMin - SafeNearPlane) / LinearDenominator;
			SliceDepthMax = (ViewDepthMax - SafeNearPlane) / LinearDenominator;
		}
		else
		{
			SliceDepthMin = std::log(ViewDepthMin / SafeNearPlane) / LogDepthDenominator;
			SliceDepthMax = std::log(ViewDepthMax / SafeNearPlane) / LogDepthDenominator;
		}

		SliceDepthMin = (std::clamp)(SliceDepthMin, 0.0f, 1.0f);
		SliceDepthMax = (std::clamp)(SliceDepthMax, 0.0f, 1.0f);
		OutAssignment.SliceMin = (std::clamp)(static_cast<int32>(SliceDepthMin * static_cast<float>(ClusterGrid.DepthSlices)), 0, ClusterGrid.DepthSlices - 1);
		OutAssignment.SliceMax = (std::clamp)(static_cast<int32>(SliceDepthMax * static_cast<float>(ClusterGrid.DepthSlices)), 0, ClusterGrid.DepthSlices - 1);

		if (OutAssignment.TileMaxX < OutAssignment.TileMinX ||
			OutAssignment.TileMaxY < OutAssignment.TileMinY ||
			OutAssignment.SliceMax < OutAssignment.SliceMin)
		{
			return false;
		}

		OutAssignment.bHasClusters = true;
		OutAssignment.bUseScissorRect = !bHasCornerBehindNearPlane;
		OutAssignment.ScissorRect.left = OutAssignment.TileMinX * ClusterGrid.TileSizeX;
		OutAssignment.ScissorRect.top = OutAssignment.TileMinY * ClusterGrid.TileSizeY;
		OutAssignment.ScissorRect.right = (std::min)((OutAssignment.TileMaxX + 1) * ClusterGrid.TileSizeX, static_cast<int32>(BuildContext.ViewportSize.X));
		OutAssignment.ScissorRect.bottom = (std::min)((OutAssignment.TileMaxY + 1) * ClusterGrid.TileSizeY, static_cast<int32>(BuildContext.ViewportSize.Y));
		return true;
	}
}

namespace FDecalCulling
{
	bool CanPrimitiveReceiveDecal(const UPrimitiveComponent* Primitive)
	{
		return Primitive != nullptr &&
			Primitive->GetRenderMesh() != nullptr &&
			!Primitive->IsA(UDecalComponent::StaticClass());
	}

	FDecalCullResult CullDecal(
		ULevel* SceneLevel,
		const UDecalComponent& DecalComponent,
		const FSceneCommandBuildContext& BuildContext,
		const std::unordered_set<const UPrimitiveComponent*>& VisibleReceivers,
		const FDecalScreenClusterGrid* ClusterGrid)
	{
		FDecalCullResult Result;
		const FOBB DecalOBB = MakeDecalOBB(DecalComponent);
		Result.bHasReceiver = HasOverlappingDecalReceiver(SceneLevel, DecalOBB, VisibleReceivers);
		if (!Result.bHasReceiver)
		{
			return Result;
		}

		if (ClusterGrid)
		{
			Result.bHasClusters = BuildDecalClusterAssignment(DecalComponent, BuildContext, *ClusterGrid, Result.ClusterAssignment);
		}
		else
		{
			Result.bHasClusters = true;
		}

		return Result;
	}

	void AssignDecalToClusters(
		FDecalScreenClusterGrid& ClusterGrid,
		int32 DecalIndex,
		const FClusteredDecalAssignment& Assignment,
		int32& OutClusterAssignmentCount)
	{
		OutClusterAssignmentCount = 0;
		if (!Assignment.bHasClusters)
		{
			return;
		}

		for (int32 Slice = Assignment.SliceMin; Slice <= Assignment.SliceMax; ++Slice)
		{
			for (int32 TileY = Assignment.TileMinY; TileY <= Assignment.TileMaxY; ++TileY)
			{
				for (int32 TileX = Assignment.TileMinX; TileX <= Assignment.TileMaxX; ++TileX)
				{
					const int32 ClusterIndex =
						((Slice * ClusterGrid.TilesY) + TileY) * ClusterGrid.TilesX + TileX;
					if (ClusterIndex < 0 || ClusterIndex >= static_cast<int32>(ClusterGrid.ClusterDecalIndices.size()))
					{
						continue;
					}

					ClusterGrid.ClusterDecalIndices[ClusterIndex].push_back(DecalIndex);
					++OutClusterAssignmentCount;
				}
			}
		}
	}
}
