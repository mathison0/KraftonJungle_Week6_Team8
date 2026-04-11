#include "Component/DecalComponent.h"
#include "Object/Class.h"
#include "Renderer/Material.h"

IMPLEMENT_RTTI(UDecalComponent, UPrimitiveComponent)

void UDecalComponent::SetDecalSize(const FVector &InSize)
{
    DecalSize = InSize;
    UpdateBounds();
}

void UDecalComponent::SetDecalMaterial(FMaterial *InMaterial)
{
    DecalMaterial = InMaterial;
}

FMatrix UDecalComponent::GetDecalToWorldMatrix() const
{
    return FTransform(GetWorldTransform()).ToMatrixWithScale();
}

FMatrix UDecalComponent::GetWorldToDecalMatrix() const
{
    FMatrix Out = GetDecalToWorldMatrix();
    Out.Inverse();
    return Out;
}

FBoxSphereBounds UDecalComponent::GetLocalBounds() const
{
    FBoxSphereBounds Bounds;
    Bounds.Center = FVector::ZeroVector;
    Bounds.BoxExtent = DecalSize * 0.5f;
    Bounds.Radius = Bounds.BoxExtent.Size();
    return Bounds;
}