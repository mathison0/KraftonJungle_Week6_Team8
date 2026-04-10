#include "DecalComponent.h"

#include <algorithm>

#include "Core/Paths.h"
#include "Object/Class.h"
#include "Serializer/Archive.h"

IMPLEMENT_RTTI(UDecalComponent, UPrimitiveComponent)

void UDecalComponent::SetDecalExtent(const FVector& InExtent)
{
	const FVector Sanitized(
		(std::max)(0.0f, InExtent.X),
		(std::max)(0.0f, InExtent.Y),
		(std::max)(0.0f, InExtent.Z));

	if (DecalExtent.X == Sanitized.X &&
		DecalExtent.Y == Sanitized.Y &&
		DecalExtent.Z == Sanitized.Z)
	{
		return;
	}

	DecalExtent = Sanitized;
	UpdateBounds();
}

void UDecalComponent::SetOpacity(float InOpacity)
{
	Opacity = (std::clamp)(InOpacity, 0.0f, 1.0f);
}

FBoxSphereBounds UDecalComponent::GetLocalBounds() const
{
	const FVector Center(DecalExtent.X * 0.5f, 0.0f, 0.0f);
	const FVector BoxExtent(DecalExtent.X * 0.5f, DecalExtent.Y, DecalExtent.Z);
	return { Center, BoxExtent.Size(), BoxExtent };
}

void UDecalComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);

	FString TexturePathString;
	if (!TexturePath.empty())
	{
		TexturePathString = FPaths::ToRelativePath(FPaths::FromWide(TexturePath));
	}

	if (Ar.IsSaving())
	{
		Ar.Serialize("DecalExtent", DecalExtent);
		Ar.Serialize("TintColor", TintColor);
		Ar.Serialize("Opacity", Opacity);
		Ar.Serialize("SortOrder", SortOrder);
		Ar.Serialize("HiddenInGame", bHiddenInGame);
		Ar.Serialize("TexturePath", TexturePathString);
	}
	else
	{
		Ar.Serialize("DecalExtent", DecalExtent);
		Ar.Serialize("TintColor", TintColor);
		Ar.Serialize("Opacity", Opacity);
		Ar.Serialize("SortOrder", SortOrder);
		Ar.Serialize("HiddenInGame", bHiddenInGame);
		Ar.Serialize("TexturePath", TexturePathString);

		DecalExtent.X = (std::max)(0.0f, DecalExtent.X);
		DecalExtent.Y = (std::max)(0.0f, DecalExtent.Y);
		DecalExtent.Z = (std::max)(0.0f, DecalExtent.Z);
		Opacity = (std::clamp)(Opacity, 0.0f, 1.0f);

		TexturePath = TexturePathString.empty()
			? std::wstring()
			: FPaths::ToWide(FPaths::ToAbsolutePath(TexturePathString));

		UpdateBounds();
	}
}

void UDecalComponent::DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const
{
	UPrimitiveComponent::DuplicateShallow(DuplicatedObject, Context);

	UDecalComponent* DuplicatedDecalComponent = static_cast<UDecalComponent*>(DuplicatedObject);
	DuplicatedDecalComponent->DecalExtent = DecalExtent;
	DuplicatedDecalComponent->TintColor = TintColor;
	DuplicatedDecalComponent->Opacity = Opacity;
	DuplicatedDecalComponent->SortOrder = SortOrder;
	DuplicatedDecalComponent->bHiddenInGame = bHiddenInGame;
	DuplicatedDecalComponent->TexturePath = TexturePath;
}
