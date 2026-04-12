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

	// 경계 페이드 활성 여부
	void SetFadeEnabled(bool bInEnabled) { bFadeEnabled = bInEnabled; }
	bool IsFadeEnabled() const { return bFadeEnabled; }

	// 경계에서 안쪽으로 페이드가 시작되는 비율 (0.0 ~ 1.0)
	void SetFadeRadius(float InRadius) { FadeRadius = std::clamp(InRadius, 0.0f, 1.0f); }
	float GetFadeRadius() const { return FadeRadius; }

private:
	FVector DecalExtent = FVector(1.0f, 0.5f, 0.5f); // 데칼 볼륨 크기
	FVector4 TintColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f); // 데칼 최종 색상 곱
	float Opacity = 1.0f; // 투명도
	float FadeRadius = 0.2f; // 데칼 가장자리의 페이드 시작 거리
	int32 SortOrder = 0; // 데칼끼리 겹칠 때 우선순위
	std::wstring TexturePath; // 데칼 텍스쳐 경로
	bool bHiddenInGame = false;
	bool bFadeEnabled = true;
};
