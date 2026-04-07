// Fill out your copyright notice in the Description page of Project Settings.

#include "BruteActor.h"

#include "MotherPatrolZoneActor.h"

#include "Animation/AnimInstance.h"
#include "Character/BlasterCharacter.h"
#include "PlayerState/BlasterPlayerState.h"

#include "AIController.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "AI/UPPSightPerceptionComponent.h"
#include "AI/BruteAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "TimerManager.h"

ABruteActor::ABruteActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	bReplicates = true;
	SetReplicateMovement(true);

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = ABruteAIController::StaticClass();

	PerceptionComp = CreateDefaultSubobject<UPPSightPerceptionComponent>(TEXT("Perception"));
	PerceptionComp->ApplySightParameters(DetectionRange, DetectionAngle);

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
	}
}

void ABruteActor::BeginPlay()
{
	Super::BeginPlay();

	if (PerceptionComp)
	{
		PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &ABruteActor::OnTargetPerceptionUpdated);
	}

	GuardLocation = GetActorLocation();

	if (HasAuthority())
	{
		StartPatrolIfNeeded();

		if (BruteBehaviorTree)
		{
			FTimerHandle BTStartTimer;
			GetWorld()->GetTimerManager().SetTimer(
				BTStartTimer,
				this,
				&ABruteActor::TryStartBruteBehaviorTree,
				0.05f,
				false);
		}
	}
}

void ABruteActor::TryStartBruteBehaviorTree()
{
	if (!BruteBehaviorTree || !HasAuthority())
	{
		return;
	}
	if (ABruteAIController* C = Cast<ABruteAIController>(GetController()))
	{
		C->RunBehaviorTreeWithBlackboard(BruteBehaviorTree, BruteBlackboardAsset);
	}
}

void ABruteActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority())
	{
		return;
	}

	if (BruteBehaviorTree)
	{
		return;
	}

	// Execution flow is driven by timers/montage callbacks.
	if (State == EBruteState::Executing)
	{
		return;
	}

	const bool bHasTarget = TargetPlayer.IsValid();
	const FVector MyLoc = GetActorLocation();

	if (State == EBruteState::Chasing)
	{
		if (!bHasTarget)
		{
			EnterSuspiciousState();
			return;
		}

		const ABlasterCharacter* Target = TargetPlayer.Get();
		const float DistToTarget = FVector::Dist(MyLoc, Target->GetActorLocation());

		const float DistFromPost = FVector::Dist(MyLoc, GuardLocation);
		if (BruteRole == EBruteRole::Guard && DistFromPost > MaxGuardPursuitDistance)
		{
			EnterSuspiciousState();
			return;
		}

		if (DistToTarget > MaxChaseDistance)
		{
			EnterSuspiciousState();
			return;
		}

		// If close enough -> execute.
		if (DistToTarget <= ExecutionDistance)
		{
			ExecuteTarget(TargetPlayer.Get());
			return;
		}

		// Keep chasing toward target location.
		MoveToLocation(Target->GetActorLocation(), PatrolAcceptanceRadius);

		// If we lost sight, transition to suspicious but keep last known.
		if (!bCanSeeTarget)
		{
			LastKnownTargetLocation = Target->GetActorLocation();
			EnterSuspiciousState();
			return;
		}
	}
	else if (State == EBruteState::Suspicious)
	{
		// Move to last known and wait for timer to clear.
		if (!LastKnownTargetLocation.IsNearlyZero())
		{
			MoveToLocation(LastKnownTargetLocation, PatrolAcceptanceRadius);
		}
	}
	else if (State == EBruteState::Returning)
	{
		if (BruteRole == EBruteRole::Guard)
		{
			MoveToLocation(GuardLocation, PatrolAcceptanceRadius);
		}
		else
		{
			StartPatrolIfNeeded();
		}
	}
	else if (State == EBruteState::Patrolling)
	{
		// Very lightweight patrol: periodically pick random point in PatrolZone and MoveTo it.
		if (!PatrolZone)
		{
			SetState(EBruteState::Idle);
			return;
		}

		const float Now = GetWorld()->TimeSeconds;
		if (Now < NextPatrolMoveTime)
		{
			return;
		}

		const FVector NextPt = GenerateRandomPatrolPoint();
		MoveToLocation(NextPt, PatrolAcceptanceRadius);
		NextPatrolMoveTime = Now + PatrolWaitTime;
	}
}

void ABruteActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABruteActor, State);
}

void ABruteActor::OnRep_State(EBruteState OldState)
{
	OnStateChanged.Broadcast(State);
}

void ABruteActor::SetRole(EBruteRole NewRole)
{
	BruteRole = NewRole;
	if (HasAuthority())
	{
		StartPatrolIfNeeded();
	}
}

void ABruteActor::SetState(EBruteState NewState)
{
	if (State == NewState)
	{
		return;
	}

	const EBruteState OldState = State;
	State = NewState;

	ApplySpeedForState(NewState);

	OnStateChanged.Broadcast(NewState);
	if (HasAuthority())
	{
		OnRep_State(OldState);
	}
}

void ABruteActor::ApplySpeedForState(EBruteState NewState)
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		const float Desired = (NewState == EBruteState::Chasing) ? ChaseSpeed : WalkSpeed;
		Movement->MaxWalkSpeed = Desired;
	}
}

void ABruteActor::StartPatrolIfNeeded()
{
	if (BruteRole == EBruteRole::Watch)
	{
		SetState(EBruteState::Patrolling);
		NextPatrolMoveTime = 0.0f;
	}
	else
	{
		SetState(EBruteState::Idle);
	}
}

void ABruteActor::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!HasAuthority())
	{
		return;
	}

	ABlasterCharacter* BlasterChar = Cast<ABlasterCharacter>(Actor);
	if (!BlasterChar)
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		bCanSeeTarget = true;
		LastKnownTargetLocation = BlasterChar->GetActorLocation();
		StartChase(BlasterChar);
	}
	else
	{
		// Lost sight.
		if (TargetPlayer.IsValid() && TargetPlayer.Get() == BlasterChar)
		{
			bCanSeeTarget = false;
			LastKnownTargetLocation = BlasterChar->GetActorLocation();
			EnterSuspiciousState();
		}
	}
}

bool ABruteActor::CanSeePlayer(ABlasterCharacter* Player) const
{
	if (!Player)
	{
		return false;
	}

	// Simple LOS trace.
	const FVector Start = GetActorLocation() + FVector(0, 0, 50.f);
	const FVector End = Player->GetActorLocation() + FVector(0, 0, 50.f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BruteLOS), false, this);
	Params.AddIgnoredActor(this);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	return !bHit;
}

void ABruteActor::StartChase(ABlasterCharacter* Target)
{
	if (!Target || bIsExecuting)
	{
		return;
	}

	TargetPlayer = Target;
	SetState(EBruteState::Chasing);

	// Cancel suspicion timer if any.
	GetWorld()->GetTimerManager().ClearTimer(SuspicionTimerHandle);
}

void ABruteActor::EnterSuspiciousState()
{
	if (State == EBruteState::Executing)
	{
		return;
	}

	SetState(EBruteState::Suspicious);

	// Start/refresh suspicion timer.
	GetWorld()->GetTimerManager().ClearTimer(SuspicionTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(
		SuspicionTimerHandle,
		this,
		&ABruteActor::ClearSuspicionAndReturn,
		SuspicionDuration,
		false
	);
}

void ABruteActor::ClearSuspicionAndReturn()
{
	TargetPlayer = nullptr;
	bCanSeeTarget = false;

	SetState(EBruteState::Returning);
}

void ABruteActor::MoveToLocation(const FVector& TargetLocation, float AcceptanceRadius)
{
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		FAIMoveRequest Req;
		Req.SetGoalLocation(TargetLocation);
		Req.SetAcceptanceRadius(AcceptanceRadius);
		Req.SetUsePathfinding(true);
		Req.SetAllowPartialPath(true);
		AI->MoveTo(Req);
	}
}

FVector ABruteActor::GenerateRandomPatrolPoint() const
{
	if (!PatrolZone)
	{
		return GuardLocation;
	}

	const FVector Center = PatrolZone->GetZoneCenter();
	const FVector Extent = PatrolZone->GetZoneExtent();

	FVector Pt;
	Pt.X = Center.X + FMath::FRandRange(-Extent.X, Extent.X);
	Pt.Y = Center.Y + FMath::FRandRange(-Extent.Y, Extent.Y);
	Pt.Z = Center.Z;

	return Pt;
}

void ABruteActor::ExecuteTarget(ABlasterCharacter* Target)
{
	if (!HasAuthority() || !Target)
	{
		return;
	}

	if (bIsExecuting)
	{
		return;
	}

	bIsExecuting = true;
	SetState(EBruteState::Executing);
	TargetPlayer = Target;

	// Adjust distance similar to Mother punishment.
	const FVector PlayerLocation = Target->GetActorLocation();
	const FVector MyLocation = GetActorLocation();
	const float CurrentDistance = FVector::Dist(MyLocation, PlayerLocation);

	if (FMath::Abs(CurrentDistance - ExecutionDistance) > 10.f)
	{
		const FVector DirToPlayer = (PlayerLocation - MyLocation).GetSafeNormal();
		const FVector NewLoc = PlayerLocation - DirToPlayer * ExecutionDistance;
		SetActorLocation(NewLoc);
	}

	// Rotate to face player.
	FVector ToPlayer = (PlayerLocation - GetActorLocation()).GetSafeNormal();
	ToPlayer.Z = 0.f;
	if (!ToPlayer.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.f, ToPlayer.Rotation().Yaw, 0.f));
	}

	// Freeze player and rotate camera toward Brute (reuse existing punished flow).
	Target->SetBeingPunished(true, this);

	// Setup montage end delegate.
	if (ExecutionMontage && GetMesh() && GetMesh()->GetAnimInstance())
	{
		FOnMontageEnded Ended;
		Ended.BindUObject(this, &ABruteActor::OnExecutionEnd);
		GetMesh()->GetAnimInstance()->Montage_SetEndDelegate(Ended, ExecutionMontage);
	}

	// Wait for camera rotation then play montage (multicast).
	GetWorld()->GetTimerManager().SetTimer(
		ExecutionCameraRotationTimer,
		this,
		&ABruteActor::Multicast_PlayExecution,
		CameraRotationWaitTime,
		false
	);
}

void ABruteActor::Multicast_PlayExecution_Implementation()
{
	if (ExecutionMontage && GetMesh() && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Play(ExecutionMontage);
	}
}

void ABruteActor::OnExecutionHit()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!TargetPlayer.IsValid())
	{
		return;
	}

	ABlasterCharacter* Victim = TargetPlayer.Get();
	ABlasterPlayerState* PS = Victim ? Victim->GetPlayerState<ABlasterPlayerState>() : nullptr;
	if (!PS)
	{
		return;
	}

	// Mother 처형과 동일하게: 카메라쉐이크 + 목숨 감소.
	Victim->Client_PlayPunishmentCameraShake();
	PS->LoseLife();
}

void ABruteActor::OnExecutionEnd(UAnimMontage* Montage, bool bInterrupted)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bIsExecuting)
	{
		return;
	}

	bIsExecuting = false;

	if (TargetPlayer.IsValid())
	{
		TargetPlayer->SetBeingPunished(false, nullptr);
	}

	Multicast_ExecutionEnd(bInterrupted);

	TargetPlayer = nullptr;
	bCanSeeTarget = false;

	ClearSuspicionAndReturn();
}

void ABruteActor::Multicast_ExecutionEnd_Implementation(bool bInterrupted)
{
	OnExecutionFinished.Broadcast(bInterrupted);
}

void ABruteActor::StopExecutionForTarget(ABlasterCharacter* Target)
{
	if (!HasAuthority() || !Target)
	{
		return;
	}

	if (!bIsExecuting)
	{
		return;
	}

	if (TargetPlayer.IsValid() && TargetPlayer.Get() != Target)
	{
		return;
	}

	if (GetMesh() && GetMesh()->GetAnimInstance() && ExecutionMontage)
	{
		GetMesh()->GetAnimInstance()->Montage_Stop(0.2f, ExecutionMontage);
	}

	OnExecutionEnd(ExecutionMontage, true);
}

