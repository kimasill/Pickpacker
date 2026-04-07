// NPCCombatComponent.cpp

#include "NPCCombatComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

UNPCCombatComponent::UNPCCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UNPCCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
}

void UNPCCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bCombatActive || bIsDead)
	{
		return;
	}

	// Scan for targets if we don't have one
	if (!CurrentTarget.IsValid())
	{
		ScanForTargets();
	}
	else
	{
		// Validate target still visible
		if (!IsActorInSight(CurrentTarget.Get()))
		{
			AActor* OldTarget = CurrentTarget.Get();
			ClearTarget();
		}
	}
}

// =========================================================================
// API
// =========================================================================

void UNPCCombatComponent::SetCombatActive(bool bActive)
{
	bCombatActive = bActive;
	SetComponentTickEnabled(bActive && !bIsDead);

	if (!bActive)
	{
		ClearTarget();
	}
}

void UNPCCombatComponent::ApplyDamage(float Damage, AActor* DamageCauser)
{
	if (bIsDead || Damage <= 0.0f)
	{
		return;
	}

	const float OldHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - Damage, 0.0f, MaxHealth);

	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		bIsDead = true;
		SetCombatActive(false);
		OnDied.Broadcast();
	}
	else if (DamageCauser && !CurrentTarget.IsValid())
	{
		// Aggro toward damage causer
		SetTarget(DamageCauser);
	}
}

bool UNPCCombatComponent::IsActorInSight(AActor* Target) const
{
	if (!Target || !GetOwner())
	{
		return false;
	}

	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const float Distance = FVector::Dist(OwnerLocation, TargetLocation);

	if (Distance > SightRange)
	{
		return false;
	}

	const FVector Forward = GetOwner()->GetActorForwardVector();
	const FVector ToTarget = (TargetLocation - OwnerLocation).GetSafeNormal();
	const float DotProduct = FVector::DotProduct(Forward, ToTarget);
	const float HalfAngleRad = FMath::DegreesToRadians(SightAngle * 0.5f);

	return DotProduct >= FMath::Cos(HalfAngleRad);
}

bool UNPCCombatComponent::IsActorInAttackRange(AActor* Target) const
{
	if (!Target || !GetOwner())
	{
		return false;
	}

	const float Distance = FVector::Dist(GetOwner()->GetActorLocation(), Target->GetActorLocation());
	return Distance <= AttackRange;
}

bool UNPCCombatComponent::TryAttack()
{
	if (!bCombatActive || bIsDead || !CurrentTarget.IsValid())
	{
		return false;
	}

	if (!IsActorInAttackRange(CurrentTarget.Get()))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastAttackTime < AttackCooldown)
	{
		return false;
	}

	LastAttackTime = CurrentTime;

	// Apply damage to target
	UGameplayStatics::ApplyDamage(
		CurrentTarget.Get(),
		AttackDamage,
		nullptr, // Controller - NPC doesn't have a standard controller here
		GetOwner(),
		nullptr
	);

	return true;
}

void UNPCCombatComponent::SetTarget(AActor* NewTarget)
{
	if (CurrentTarget.Get() == NewTarget)
	{
		return;
	}

	CurrentTarget = NewTarget;

	if (NewTarget)
	{
		OnTargetAcquired.Broadcast(NewTarget);
	}
}

void UNPCCombatComponent::ClearTarget()
{
	if (CurrentTarget.IsValid())
	{
		CurrentTarget.Reset();
		OnTargetLost.Broadcast();
	}
}

void UNPCCombatComponent::ResetHealth()
{
	CurrentHealth = MaxHealth;
	bIsDead = false;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

// =========================================================================
// Internal
// =========================================================================

void UNPCCombatComponent::ScanForTargets()
{
	UWorld* World = GetWorld();
	if (!World || !GetOwner())
	{
		return;
	}

	float ClosestDist = SightRange + 1.0f;
	AActor* ClosestTarget = nullptr;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			APawn* Pawn = PC->GetPawn();
			if (Pawn && IsActorInSight(Pawn))
			{
				const float Dist = FVector::Dist(GetOwner()->GetActorLocation(), Pawn->GetActorLocation());
				if (Dist < ClosestDist)
				{
					ClosestDist = Dist;
					ClosestTarget = Pawn;
				}
			}
		}
	}

	if (ClosestTarget)
	{
		SetTarget(ClosestTarget);
	}
}
