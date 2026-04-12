#include "SpotLightFakeActor.h"

#include "Core/Paths.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"
#include "Serializer/Archive.h"
#include "Component/BillboardComponent.h"
#include "Component/DecalComponent.h"
#include "Component/SceneComponent.h"

IMPLEMENT_RTTI(ASpotLightFakeActor, AActor)

void ASpotLightFakeActor::PostSpawnInitialize()
{
	RootSceneComponent = FObjectFactory::ConstructObject<USceneComponent>(this, "RootSceneComponent");
	AddOwnedComponent(RootSceneComponent);
	SetRootComponent(RootSceneComponent);

	DecalComponent = FObjectFactory::ConstructObject<UDecalComponent>(this, "DecalComponent");
	AddOwnedComponent(DecalComponent);
	DecalComponent->AttachTo(RootSceneComponent);
	DecalComponent->SetRelativeTransform(FTransform(FRotator(-90.0f, 0.0f, 0.0f), FVector::ZeroVector, FVector::OneVector));
	DecalComponent->SetDecalExtent(FVector(3.0f, 1.5f, 1.5f));
	DecalComponent->SetTexturePath((FPaths::TextureDir() / L"SpotLightProjection.png").wstring());
	DecalComponent->SetTintColor(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	DecalComponent->SetOpacity(1.0f);
	DecalComponent->SetFadeEnabled(true);
	DecalComponent->SetFadeRadius(0.8f);

	BillboardComponent = FObjectFactory::ConstructObject<UBillboardComponent>(this, "BillboardComponent");
	AddOwnedComponent(BillboardComponent);
	BillboardComponent->AttachTo(RootSceneComponent);
	BillboardComponent->SetSize(FVector2(0.5f, 0.5f));
	BillboardComponent->SetTexturePath((FPaths::TextureDir() / L"SpotLightGlow.png").wstring());
	BillboardComponent->SetHiddenInGame(false);
	UpdateBillboardPlacement();

	AActor::PostSpawnInitialize();
}

void ASpotLightFakeActor::Serialize(FArchive& Ar)
{
	AActor::Serialize(Ar);
}

void ASpotLightFakeActor::FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const
{
	AActor::FixupDuplicatedReferences(DuplicatedObject, Context);
	ASpotLightFakeActor* Duplicated = static_cast<ASpotLightFakeActor*>(DuplicatedObject);
	Duplicated->RootSceneComponent = Context.FindDuplicate(RootSceneComponent);
	Duplicated->BillboardComponent = Context.FindDuplicate(BillboardComponent);
	Duplicated->DecalComponent = Context.FindDuplicate(DecalComponent);
}

void ASpotLightFakeActor::SetBillboardTexturePath(const std::wstring& InPath)
{
	if (BillboardComponent)
	{
		BillboardComponent->SetTexturePath(InPath);
	}
}

const std::wstring& ASpotLightFakeActor::GetBillboardTexturePath() const
{
	static const std::wstring Empty;
	return BillboardComponent ? BillboardComponent->GetTexturePath() : Empty;
}

void ASpotLightFakeActor::SetBillboardSize(const FVector2& InSize)
{
	if (BillboardComponent)
	{
		BillboardComponent->SetSize(InSize);
		UpdateBillboardPlacement();
	}
}

const FVector2& ASpotLightFakeActor::GetBillboardSize() const
{
	static const FVector2 Default(0.5f, 0.5f);
	return BillboardComponent ? BillboardComponent->GetSize() : Default;
}

void ASpotLightFakeActor::SetDecalTexturePath(const std::wstring& InPath)
{
	if (DecalComponent)
	{
		DecalComponent->SetTexturePath(InPath);
	}
}

const std::wstring& ASpotLightFakeActor::GetDecalTexturePath() const
{
	static const std::wstring Empty;
	return DecalComponent ? DecalComponent->GetTexturePath() : Empty;
}

void ASpotLightFakeActor::SetDecalExtent(const FVector& InExtent)
{
	if (DecalComponent)
	{
		DecalComponent->SetDecalExtent(InExtent);
		UpdateBillboardPlacement();
	}
}

const FVector& ASpotLightFakeActor::GetDecalExtent() const
{
	static const FVector Default(3.0f, 1.5f, 1.5f);
	return DecalComponent ? DecalComponent->GetDecalExtent() : Default;
}

void ASpotLightFakeActor::SetDecalFadeEnabled(bool bInEnabled)
{
	if (DecalComponent)
	{
		DecalComponent->SetFadeEnabled(bInEnabled);
	}
}

bool ASpotLightFakeActor::IsDecalFadeEnabled() const
{
	return DecalComponent ? DecalComponent->IsFadeEnabled() : true;
}

void ASpotLightFakeActor::SetDecalFadeRadius(float InRadius)
{
	if (DecalComponent)
	{
		DecalComponent->SetFadeRadius(InRadius);
	}
}

float ASpotLightFakeActor::GetDecalFadeRadius() const
{
	return DecalComponent ? DecalComponent->GetFadeRadius() : 0.8f;
}

void ASpotLightFakeActor::UpdateBillboardPlacement()
{
	if (!BillboardComponent || !DecalComponent)
	{
		return;
	}

	const FVector& DecalExtent = DecalComponent->GetDecalExtent();
	const FTransform& DecalTransform = DecalComponent->GetRelativeTransform();
	const FVector Corners[8] =
	{
		FVector(0.0f, -DecalExtent.Y, -DecalExtent.Z),
		FVector(0.0f, -DecalExtent.Y, DecalExtent.Z),
		FVector(0.0f, DecalExtent.Y, -DecalExtent.Z),
		FVector(0.0f, DecalExtent.Y, DecalExtent.Z),
		FVector(DecalExtent.X, -DecalExtent.Y, -DecalExtent.Z),
		FVector(DecalExtent.X, -DecalExtent.Y, DecalExtent.Z),
		FVector(DecalExtent.X, DecalExtent.Y, -DecalExtent.Z),
		FVector(DecalExtent.X, DecalExtent.Y, DecalExtent.Z)
	};

	FVector MinBounds = DecalTransform.TransformPosition(Corners[0]);
	FVector MaxBounds = MinBounds;
	for (int32 CornerIndex = 1; CornerIndex < 8; ++CornerIndex)
	{
		const FVector WorldCorner = DecalTransform.TransformPosition(Corners[CornerIndex]);
		MinBounds.X = (std::min)(MinBounds.X, WorldCorner.X);
		MinBounds.Y = (std::min)(MinBounds.Y, WorldCorner.Y);
		MinBounds.Z = (std::min)(MinBounds.Z, WorldCorner.Z);
		MaxBounds.X = (std::max)(MaxBounds.X, WorldCorner.X);
		MaxBounds.Y = (std::max)(MaxBounds.Y, WorldCorner.Y);
		MaxBounds.Z = (std::max)(MaxBounds.Z, WorldCorner.Z);
	}

	const FVector2& BillboardSize = BillboardComponent->GetSize();
	BillboardComponent->SetRelativeLocation(FVector(
		(MinBounds.X + MaxBounds.X) * 0.5f,
		(MinBounds.Y + MaxBounds.Y) * 0.5f,
		MaxBounds.Z + BillboardSize.Y * 0.5f));
}
