#pragma once

#include "PrimitiveComponent.h"

class ENGINE_API UDecalComponent : public UPrimitiveComponent
{
  public:
    DECLARE_RTTI(UDecalComponent, UPrimitiveComponent)

    void SetDecalExtent(const FVector &InExtent);
    const FVector &GetDecalExtent() const
    {
        return DecalExtent;
    }

    void SetDecalMaterial(FMaterial *InMaterial);
    FMaterial *GetDecalMaterial() const
    {
        return DecalMaterial;
    }

    FMatrix GetDecalToWorldMatrix() const;
    FMatrix GetWorldToDecalMatrix() const;

    virtual FBoxSphereBounds GetLocalBounds() const override;
    virtual bool IsPickable() const override
    {
        return false;
    }

  private:
    // Half extent of the decal volume in local space.
    FVector DecalExtent = FVector(50.0f, 50.0f, 50.0f);
    FMaterial *DecalMaterial = nullptr;
    int32 SortOrder = 0;
};
