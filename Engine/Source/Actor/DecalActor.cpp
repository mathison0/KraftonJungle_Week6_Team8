#include "DecalActor.h"
#include "Component/DecalComponent.h"
#include "Component/SceneComponent.h"
#include "Object/Class.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_RTTI(ADecalActor, AActor)

void ADecalActor::PostSpawnInitialize()
{
    Root = FObjectFactory::ConstructObject<USceneComponent>(this, "Root");
    AddOwnedComponent(Root);
    SetRootComponent(Root);

    DecalComponent = FObjectFactory::ConstructObject<UDecalComponent>(this, "DecalComponent");
    AddOwnedComponent(DecalComponent);
    DecalComponent->AttachTo(Root);

    // Half extent. Keeps the previous visible full size at 100 units.
    DecalComponent->SetDecalExtent(FVector(50.0f, 50.0f, 50.0f));

    AActor::PostSpawnInitialize();
}

void ADecalActor::FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const
{
    AActor::FixupDuplicatedReferences(DuplicatedObject, Context);

    ADecalActor* DuplicatedActor = static_cast<ADecalActor*>(DuplicatedObject);
    DuplicatedActor->Root = Context.FindDuplicate(Root);
    DuplicatedActor->DecalComponent = Context.FindDuplicate(DecalComponent);
}
