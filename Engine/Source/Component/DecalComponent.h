#pragma once

#include "PrimitiveComponent.h"

class FArchive;

class ENGINE_API UDecalComponent : public UPrimitiveComponent
{
public:
	DECLARE_RTTI(UDecalComponent, UPrimitiveComponent)

	virtual FBoxSphereBounds GetLocalBounds() const override;
	void Serialize(FArchive& Ar) override;
	void DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const override;

	void SetDecalExtent(const FVector& InExtent);
	const FVector& GetDecalExtent() const { return DecalExtent; }

	void SetTintColor(const FVector4& InTintColor) { TintColor = InTintColor; }
	const FVector4& GetTintColor() const { return TintColor; }

	void SetOpacity(float InOpacity);
	float GetOpacity() const { return Opacity; }

	void SetSortOrder(int32 InSortOrder) { SortOrder = InSortOrder; }
	int32 GetSortOrder() const { return SortOrder; }

	void SetTexturePath(const std::wstring& InPath) { TexturePath = InPath; }
	const std::wstring& GetTexturePath() const { return TexturePath; }

	void SetHiddenInGame(bool bInHidden) { bHiddenInGame = bInHidden; }
	bool IsHiddenInGame() const { return bHiddenInGame; }

private:
	FVector DecalExtent = FVector(1.0f, 0.5f, 0.5f);
	FVector4 TintColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float Opacity = 1.0f;
	int32 SortOrder = 0;
	bool bHiddenInGame = false;
	std::wstring TexturePath;
};
