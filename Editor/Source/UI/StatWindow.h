#pragma once
#include "CoreMinimal.h"
#include "imgui.h"
#include "Renderer/Feature/DecalRenderFeature.h"

struct FRect;
class FRenderer;

struct FObjectEntry
{
	FString Name;
	FString ClassName;
	uint32 Size = 0;
};

class FStatWindow
{
public:
	void Render(const FRect& AreaRect);
	void SetObjectCount(uint32 InCount) { ObjectCount = InCount; }
	void SetHeapUsage(uint32 InBytes) { HeapUsageBytes = InBytes; }
	void SetDecalStats(const FDecalPassStats& InStats) { DecalStats = InStats; }
	void ClearDecalStats() { DecalStats = FDecalPassStats(); }

private:
	void RefreshObjectList();

	uint32 ObjectCount = 0;
	uint32 HeapUsageBytes = 0;
	FDecalPassStats DecalStats;
	bool bHasDecalStats = false;

	TArray<FObjectEntry> ObjectEntries;
	bool bShowObjectList = false;
};
