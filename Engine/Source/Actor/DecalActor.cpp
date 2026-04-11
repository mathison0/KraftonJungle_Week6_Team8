#include "DecalActor.h"
#include "Asset/ObjManager.h"
#include "Core/Paths.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"
#include "Component/DecalComponent.h"

IMPLEMENT_RTTI(ADecalActor, AActor)

void ADecalActor::PostSpawnInitialize()
{
	DecalComponent = FObjectFactory::ConstructObject<UDecalComponent>(this, "DecalComponent");
	AddOwnedComponent(DecalComponent);
	DecalComponent->SetTexturePath((FPaths::TextureDir() / L"texture.png").wstring());
	AActor::PostSpawnInitialize();
}

void ADecalActor::FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const
{
	AActor::FixupDuplicatedReferences(DuplicatedObject, Context);
	static_cast<ADecalActor*>(DuplicatedObject)->DecalComponent = Context.FindDuplicate(DecalComponent);
}
