#include "Component/DecalComponent.h"
#include "Object/Class.h"
#include "Renderer/Material.h"
#include "Math/MathUtility.h"

IMPLEMENT_RTTI(UDecalComponent, UPrimitiveComponent)

void UDecalComponent::SetDecalExtent(const FVector &InExtent)
{
    DecalExtent.X = FMath::Max(InExtent.X, 0.5f);
    DecalExtent.Y = FMath::Max(InExtent.Y, 0.5f);
    DecalExtent.Z = FMath::Max(InExtent.Z, 0.5f);
    UpdateBounds();
}

void UDecalComponent::SetDecalMaterial(FMaterial *InMaterial)
{
    DecalMaterial = InMaterial;
}

FMatrix UDecalComponent::GetDecalToWorldMatrix() const
{
    // Keep the decal volume in stable world space by scaling the unit cube first,
    // then applying the component's cached world transform directly.
    const FVector FullSize = DecalExtent * 2.0f;
    return FMatrix::MakeScale(FullSize) * GetWorldTransform();
}


FMatrix UDecalComponent::GetWorldToDecalMatrix() const
{
    return GetDecalToWorldMatrix().GetInverse();
}

FBoxSphereBounds UDecalComponent::GetLocalBounds() const
{
    FBoxSphereBounds Bounds;
    Bounds.Center = FVector::ZeroVector;
    Bounds.BoxExtent = DecalExtent;
    Bounds.Radius = Bounds.BoxExtent.Size();
    return Bounds;
}
