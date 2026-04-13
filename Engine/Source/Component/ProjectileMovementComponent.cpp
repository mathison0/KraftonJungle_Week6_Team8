#include "ProjectileMovementComponent.h"
#include "Object/Class.h"
#include "Serializer/Archive.h"
#include <cmath>

IMPLEMENT_RTTI(UProjectileMovementComponent, UMovementComponent)

void UProjectileMovementComponent::PostConstruct()
{
	UMovementComponent::PostConstruct();
}

void UProjectileMovementComponent::BeginPlay()
{
	UMovementComponent::BeginPlay();

	bSimulationEnabled = IsComponentTickEnabled() && !Velocity.IsNearlyZero();
}

void UProjectileMovementComponent::LaunchWithVelocity(const FVector& InVelocity)
{
	Velocity = InVelocity;
	bSimulationEnabled = !Velocity.IsNearlyZero();
	SetComponentTickEnabled(true);
}

void UProjectileMovementComponent::Tick(float DeltaTime)
{
	UMovementComponent::Tick(DeltaTime);

	if (!bSimulationEnabled)
	{
		return;
	}

	if (ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	Velocity.Z += GravityZ * GravityScale * DeltaTime;

	if (MaxSpeed > 0.0f)
	{
		const float SpeedSq = Velocity.X * Velocity.X + Velocity.Y * Velocity.Y + Velocity.Z * Velocity.Z;
		if (SpeedSq > MaxSpeed * MaxSpeed)
		{
			const float Scale = MaxSpeed / std::sqrt(SpeedSq);
			Velocity.X *= Scale;
			Velocity.Y *= Scale;
			Velocity.Z *= Scale;
		}
	}

	MoveUpdatedComponent(Velocity * DeltaTime);
}

void UProjectileMovementComponent::DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const
{
	UMovementComponent::DuplicateShallow(DuplicatedObject, Context);

	UProjectileMovementComponent* Duplicated = static_cast<UProjectileMovementComponent*>(DuplicatedObject);
	Duplicated->Velocity = Velocity;
	Duplicated->GravityScale = GravityScale;
	Duplicated->MaxSpeed = MaxSpeed;
	Duplicated->bSimulationEnabled = false;
}

void UProjectileMovementComponent::Serialize(FArchive& Ar)
{
	UMovementComponent::Serialize(Ar);

	Ar.Serialize("VelocityX", Velocity.X);
	Ar.Serialize("VelocityY", Velocity.Y);
	Ar.Serialize("VelocityZ", Velocity.Z);
	Ar.Serialize("GravityScale", GravityScale);
	Ar.Serialize("MaxSpeed", MaxSpeed);
}
