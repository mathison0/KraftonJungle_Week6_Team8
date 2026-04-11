#include "DecalActor.h"
#include "Component/DecalComponent.h"

void ADecalActor::PostSpawnInitialize()
{
    Root = FObjectFactory::ConstructObject<USceneComponent>(this, "Root");
    AddOwnedComponent(Root);
    SetRootComponent(Root);

    DecalComponent = FObjectFactory::ConstructObject<UDecalComponent>(this, "DecalComponent");
    AddOwnedComponent(DecalComponent);
    DecalComponent->AttachTo(Root);

    DecalComponent->SetDecalSize(FVector(100.0f, 100.0f, 100.0f));

    AActor::PostSpawnInitialize();
}