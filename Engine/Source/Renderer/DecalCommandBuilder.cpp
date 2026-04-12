#include "Renderer/DecalCommandBuilder.h"

#include "Component/DecalComponent.h"
#include "Renderer/Material.h"
#include "Renderer/MeshData.h"
#include "Renderer/RenderFeatureInterfaces.h"

FMaterial* FDecalCommandBuilder::GetOrCreateDecalMaterial(
	const FSceneCommandBuildContext& BuildContext,
	const UDecalComponent* Component)
{
	if (!Component || !BuildContext.DecalFeature)
	{
		return nullptr;
	}

	FMaterial* BaseDecalMaterial = BuildContext.DecalFeature->GetBaseMaterial();
	if (!BaseDecalMaterial)
	{
		return nullptr;
	}

	auto Found = DecalMaterialsByComponent.find(Component);
	if (Found == DecalMaterialsByComponent.end())
	{
		std::unique_ptr<FDynamicMaterial> OwnedMaterial = BaseDecalMaterial->CreateDynamicMaterial();
		if (!OwnedMaterial)
		{
			return BaseDecalMaterial;
		}

		std::shared_ptr<FDynamicMaterial> Material(OwnedMaterial.release());
		Found = DecalMaterialsByComponent.emplace(Component, std::move(Material)).first;
	}

	FDynamicMaterial* Material = Found->second.get();
	if (!Material)
	{
		return BaseDecalMaterial;
	}

	FVector4 BaseColor = Component->GetTintColor();
	BaseColor.W *= Component->GetOpacity();
	if (!Material->SetVectorParameter("BaseColor", BaseColor))
	{
		Material->SetVectorParameter("ColorTint", BaseColor);
	}

	const FVector& Extent = Component->GetDecalExtent();
	const FVector4 DecalExtent(Extent.X, Extent.Y, Extent.Z, 0.0f);
	Material->SetParameterData("DecalExtent", &DecalExtent, sizeof(FVector4));

	const FVector2 ViewportSize = BuildContext.ViewportSize;
	const FVector4 ScreenSize(
		ViewportSize.X,
		ViewportSize.Y,
		ViewportSize.X > 0.0f ? 1.0f / ViewportSize.X : 0.0f,
		ViewportSize.Y > 0.0f ? 1.0f / ViewportSize.Y : 0.0f);
	Material->SetParameterData("ScreenSize", &ScreenSize, sizeof(FVector4));

	const FMatrix DecalWorldTransform = Component->GetWorldTransform();
	const FVector DecalOrigin = DecalWorldTransform.GetTranslation();
	const FVector DecalAxisX = DecalWorldTransform.GetScaledAxis(EAxis::X);
	const FVector DecalAxisY = DecalWorldTransform.GetScaledAxis(EAxis::Y);
	const FVector DecalAxisZ = DecalWorldTransform.GetScaledAxis(EAxis::Z);
	const FVector4 DecalOriginData(DecalOrigin.X, DecalOrigin.Y, DecalOrigin.Z, 1.0f);
	const FVector4 DecalAxisXData(DecalAxisX.X, DecalAxisX.Y, DecalAxisX.Z, 0.0f);
	const FVector4 DecalAxisYData(DecalAxisY.X, DecalAxisY.Y, DecalAxisY.Z, 0.0f);
	const FVector4 DecalAxisZData(DecalAxisZ.X, DecalAxisZ.Y, DecalAxisZ.Z, 0.0f);
	Material->SetParameterData("DecalOrigin", &DecalOriginData, sizeof(FVector4));
	Material->SetParameterData("DecalAxisX", &DecalAxisXData, sizeof(FVector4));
	Material->SetParameterData("DecalAxisY", &DecalAxisYData, sizeof(FVector4));
	Material->SetParameterData("DecalAxisZ", &DecalAxisZData, sizeof(FVector4));

	const std::wstring& TexturePath = Component->GetTexturePath();
	if (!TexturePath.empty())
	{
		std::shared_ptr<FMaterialTexture> Texture = BuildContext.DecalFeature->GetOrLoadTexture(TexturePath);
		if (Texture)
		{
			Material->SetMaterialTexture(Texture);
		}
	}

	return Material;
}

bool FDecalCommandBuilder::BuildDecalCommand(
	const FSceneCommandBuildContext& BuildContext,
	UDecalComponent* DecalComponent,
	const FClusteredDecalAssignment* ClusterAssignment,
	int32 DecalIndex,
	FRenderCommand& OutCommand,
	int32& OutClusterAssignmentCount,
	FDecalScreenClusterGrid* ClusterGrid)
{
	OutClusterAssignmentCount = 0;
	if (!DecalComponent || !BuildContext.DecalFeature)
	{
		return false;
	}

	FMaterial* DecalMaterial = GetOrCreateDecalMaterial(BuildContext, DecalComponent);
	if (!DecalMaterial)
	{
		DecalMaterial = BuildContext.DecalFeature->GetBaseMaterial();
	}
	if (!DecalMaterial)
	{
		return false;
	}

	auto FoundMesh = DecalMeshesByComponent.find(DecalComponent);
	if (FoundMesh == DecalMeshesByComponent.end())
	{
		FoundMesh = DecalMeshesByComponent.emplace(DecalComponent, std::make_shared<FDynamicMesh>()).first;
	}

	std::shared_ptr<FDynamicMesh> DecalMesh = FoundMesh->second;
	if (!DecalMesh)
	{
		return false;
	}

	if (!BuildContext.DecalFeature->BuildMesh(DecalComponent->GetDecalExtent(), *DecalMesh))
	{
		return false;
	}
	DecalMesh->bIsDirty = true;

	FRenderCommand Command;
	Command.RenderMesh = DecalMesh.get();
	Command.RenderMeshOwner = DecalMesh;
	Command.Material = DecalMaterial;
	Command.RenderLayer = ERenderLayer::Decal;
	Command.SortPriority = DecalComponent->GetSortOrder();
	Command.bDisableDepthTest = true;
	Command.bDisableDepthWrite = true;
	Command.WorldMatrix = DecalComponent->GetWorldTransform();

	if (ClusterGrid && ClusterAssignment)
	{
		FDecalCulling::AssignDecalToClusters(*ClusterGrid, DecalIndex, *ClusterAssignment, OutClusterAssignmentCount);
		if (OutClusterAssignmentCount <= 0)
		{
			return false;
		}

		Command.bUseScissorRect = ClusterAssignment->bUseScissorRect;
		Command.ScissorRect = ClusterAssignment->ScissorRect;
	}

	OutCommand = std::move(Command);
	return true;
}

void FDecalCommandBuilder::PruneStaleDecalResources(const TArray<const UDecalComponent*>& ActiveComponents)
{
	for (auto It = DecalMaterialsByComponent.begin(); It != DecalMaterialsByComponent.end();)
	{
		if (std::find(ActiveComponents.begin(), ActiveComponents.end(), It->first) == ActiveComponents.end())
		{
			It = DecalMaterialsByComponent.erase(It);
			continue;
		}

		++It;
	}

	for (auto It = DecalMeshesByComponent.begin(); It != DecalMeshesByComponent.end();)
	{
		if (std::find(ActiveComponents.begin(), ActiveComponents.end(), It->first) == ActiveComponents.end())
		{
			It = DecalMeshesByComponent.erase(It);
			continue;
		}

		++It;
	}
}
