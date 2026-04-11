#pragma once

#include "PrimitiveComponent.h"

class ENGINE_API UDecalComponent : public UPrimitiveComponent
{
  public:
    DECLARE_RTTI(UDecalComponent, UPrimitiveComponent)

    void SetDecalSize(const FVector &InSize);
    const FVector &GetDecalSize() const
    {
        return DecalSize;
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
    FVector DecalSize = FVector(100.0f, 100.0f, 100.0f);
    FMaterial *DecalMaterial = nullptr;
    int32 SortOrder = 0;
};