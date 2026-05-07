// NPCCombatComponent.cpp

#include "NPCCombatComponent.h"
#include "AIController.h"
#include "AI/PPAIControllerBase.h"
#include "Components/NPCCombatActionComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

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
	AggroHomeLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

void UNPCCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNPCCombatComponent, CurrentHealth);
	DOREPLIFETIME(UNPCCombatComponent, EngagementPolicy);
	DOREPLIFETIME(UNPCCombatComponent, bCombatActive);
	DOREPLIFETIME(UNPCCombatComponent, CurrentTarget);
	DOREPLIFETIME(UNPCCombatComponent, bIsDead);
	DOREPLIFETIME(UNPCCombatComponent, bTemporaryAggressionActive);
}

void UNPCCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bCombatActive || bIsDead)
	{
		return;
	}

	if (CurrentTarget && bAutoClearTarget)
	{
		UpdateTargetRetention();
	}

	if (IsBehaviorTreeRunning() || !bEnableDirectCombatFallback)
	{
		return;
	}

	if (!CurrentTarget)
	{
		if (CanInitiateEngagement())
		{
			ScanForTargets();
		}
	}
	else
	{
		// Validate target still visible
		if (!IsActorInSight(CurrentTarget.Get()))
		{
			ClearTarget();
		}
		else
		{
			UpdateDirectCombatMovement();
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

	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
		{
			Movement->MaxWalkSpeed = bActive ? ChaseSpeed : PatrolSpeed;
		}
	}

	if (!bActive)
	{
		if (AAIController* AIController = Cast<AAIController>(Cast<APawn>(GetOwner()) ? Cast<APawn>(GetOwner())->GetController() : nullptr))
		{
			AIController->StopMovement();
		}
		ClearTarget();
	}
}

void UNPCCombatComponent::SetEngagementPolicy(ENPCEngagementPolicy NewPolicy)
{
	EngagementPolicy = NewPolicy;

	if (EngagementPolicy == ENPCEngagementPolicy::Passive && !CurrentTarget)
	{
		ClearTarget();
	}
}

void UNPCCombatComponent::ApplyCombatSettings(const FNPCCombatSettings& Settings)
{
	const float PreviousMaxHealth = MaxHealth;
	MaxHealth = FMath::Max(1.0f, Settings.MaxHealth);
	if (!bIsDead && FMath::IsNearlyEqual(CurrentHealth, PreviousMaxHealth))
	{
		CurrentHealth = MaxHealth;
	}
	else
	{
		CurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, MaxHealth);
	}

	AttackRange = Settings.AttackRange;
	AttackDamage = Settings.AttackDamage;
	AttackCooldown = Settings.AttackCooldown;
	SetEngagementPolicy(Settings.EngagementPolicy);
	bRetaliateWhenDamaged = Settings.bRetaliateWhenDamaged;
	bBecomeAggressiveWhenDamaged = Settings.bBecomeAggressiveWhenDamaged;
	bAutoClearTarget = Settings.bAutoClearTarget;
	LoseSightAggroGraceTime = Settings.LoseSightAggroGraceTime;
	MaxChaseDistanceFromHome = Settings.MaxChaseDistanceFromHome;
	MaxTargetDistance = Settings.MaxTargetDistance;
	ChaseSpeed = Settings.ChaseSpeed;
	PatrolSpeed = Settings.PatrolSpeed;
}

bool UNPCCombatComponent::CanInitiateEngagement() const
{
	return bCombatActive && !bIsDead && EngagementPolicy == ENPCEngagementPolicy::Aggressive;
}

bool UNPCCombatComponent::CanRetaliate() const
{
	return !bIsDead && bRetaliateWhenDamaged && EngagementPolicy != ENPCEngagementPolicy::Passive;
}

void UNPCCombatComponent::BeginTemporaryAggression(AActor* NewTarget)
{
	if (bIsDead || !bBecomeAggressiveWhenDamaged || EngagementPolicy == ENPCEngagementPolicy::Passive)
	{
		return;
	}

	if (!bTemporaryAggressionActive && EngagementPolicy != ENPCEngagementPolicy::Aggressive)
	{
		PreviousEngagementPolicy = EngagementPolicy;
		bTemporaryAggressionActive = true;
		SetEngagementPolicy(ENPCEngagementPolicy::Aggressive);
	}

	SetCombatActive(true);
	SetTarget(NewTarget);
}

void UNPCCombatComponent::RestoreTemporaryEngagementPolicy()
{
	if (!bTemporaryAggressionActive)
	{
		return;
	}

	const ENPCEngagementPolicy PolicyToRestore = PreviousEngagementPolicy;
	bTemporaryAggressionActive = false;
	PreviousEngagementPolicy = PolicyToRestore;
	SetEngagementPolicy(PolicyToRestore);
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
	else if (DamageCauser && !CurrentTarget)
	{
		if (CanRetaliate())
		{
			BeginTemporaryAggression(DamageCauser);
		}
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
	if (!bCombatActive || bIsDead || !CurrentTarget)
	{
		return false;
	}

	if (!IsActorInAttackRange(CurrentTarget.Get()))
	{
		return false;
	}

	if (bUseActionRuleForDefaultAttack)
	{
		if (UNPCCombatActionComponent* ActionComponent = GetOwner() ? GetOwner()->FindComponentByClass<UNPCCombatActionComponent>() : nullptr)
		{
			if (ActionComponent->FindActionRule(DefaultAttackActionId))
			{
				return ActionComponent->ExecuteAction(DefaultAttackActionId, CurrentTarget.Get());
			}
		}
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
		LastTargetVisibleTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

		if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
		{
			if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
			{
				Movement->MaxWalkSpeed = ChaseSpeed;
			}
		}
		OnTargetAcquired.Broadcast(NewTarget);
	}
}

void UNPCCombatComponent::ClearTarget()
{
	if (CurrentTarget)
	{
		CurrentTarget = nullptr;

		if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
		{
			if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
			{
				Movement->MaxWalkSpeed = PatrolSpeed;
			}
		}

		if (AAIController* AIController = Cast<AAIController>(Cast<APawn>(GetOwner()) ? Cast<APawn>(GetOwner())->GetController() : nullptr))
		{
			AIController->StopMovement();
		}

		OnTargetLost.Broadcast();
		RestoreTemporaryEngagementPolicy();
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

void UNPCCombatComponent::UpdateTargetRetention()
{
	AActor* Target = CurrentTarget.Get();
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Target || !Owner || !World)
	{
		ClearTarget();
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (IsActorInSight(Target))
	{
		LastTargetVisibleTime = Now;
	}
	else if (LoseSightAggroGraceTime >= 0.0f && Now - LastTargetVisibleTime > LoseSightAggroGraceTime)
	{
		ClearTarget();
		return;
	}

	if (MaxChaseDistanceFromHome > 0.0f &&
		FVector::Dist2D(Owner->GetActorLocation(), AggroHomeLocation) > MaxChaseDistanceFromHome)
	{
		ClearTarget();
		return;
	}

	if (MaxTargetDistance > 0.0f &&
		FVector::Dist2D(Owner->GetActorLocation(), Target->GetActorLocation()) > MaxTargetDistance)
	{
		ClearTarget();
		return;
	}
}

void UNPCCombatComponent::UpdateDirectCombatMovement()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !CurrentTarget)
	{
		return;
	}

	AAIController* AIController = Cast<AAIController>(OwnerPawn->GetController());
	if (!AIController)
	{
		return;
	}

	if (const APPAIControllerBase* PPController = Cast<APPAIControllerBase>(AIController))
	{
		if (PPController->IsBehaviorTreeRunning())
		{
			return;
		}
	}

	if (IsActorInAttackRange(CurrentTarget.Get()))
	{
		AIController->StopMovement();
		TryAttack();
		return;
	}

	AIController->MoveToActor(CurrentTarget.Get(), FMath::Max(AttackRange * 0.8f, 50.0f), true, true, true, nullptr, true);
}

bool UNPCCombatComponent::IsBehaviorTreeRunning() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const AAIController* AIController = OwnerPawn ? Cast<AAIController>(OwnerPawn->GetController()) : nullptr;
	const APPAIControllerBase* PPController = AIController ? Cast<APPAIControllerBase>(AIController) : nullptr;
	return PPController && PPController->IsBehaviorTreeRunning();
}
