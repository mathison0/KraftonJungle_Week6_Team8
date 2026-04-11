#pragma once

#include "Actor.h"

class UDecalComponent;

class ENGINE_API ADecalActor : public AActor
{
public:
    DECLARE_RTTI(ADecalActor, AActor)

    void PostSpawnInitialize() override;
    void FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const override;

private:
    USceneComponent* Root = nullptr;
    UDecalComponent* DecalComponent = nullptr;
};