#pragma once

#include "CoreMinimal.h"
#include "Renderer/RenderFeatureInterfaces.h"

class FMaterial;

struct ENGINE_API FSceneCommandBuildContext
{
	FMaterial* DefaultMaterial = nullptr;
	ISceneTextFeature* TextFeature = nullptr;
	ISceneSubUVFeature* SubUVFeature = nullptr;
	ISceneBillboardFeature* BillboardFeature = nullptr;
	ISceneDecalFeature* DecalFeature = nullptr;
	FVector2 ViewportSize = FVector2(0.0f, 0.0f);
	FMatrix ViewMatrix = FMatrix::Identity;
	FMatrix ProjectionMatrix = FMatrix::Identity;
	float NearPlane = 0.1f;
	float FarPlane = 1000.0f;
	bool bOrthographic = false;
	float TotalTimeSeconds = 0.0f;
};
