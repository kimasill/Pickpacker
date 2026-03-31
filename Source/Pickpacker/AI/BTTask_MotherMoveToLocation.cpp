// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_MotherMoveToLocation.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "Blaster/AI/MotherAIActor.h"
#include "Blaster/AI/MotherAIController.h"
#include "Blaster/AI/DroneActor.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Blaster/GameMode/PickpackerGameMode.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Engine/World.h"

UBTTask_MotherMoveToLocation::UBTTask_MotherMoveToLocation()
{
	NodeName = TEXT("Mother Move To Location");
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_MotherMoveToLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FMotherMoveToLocationMemory* MyMemory = reinterpret_cast<FMotherMoveToLocationMemory*>(NodeMemory);
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	// Check if this is punishment mode (ShouldPunish key)
	bool bShouldPunish = BlackboardComp->GetValueAsBool(FName("ShouldPunish"));
	ACharacter* TargetPlayer = Cast<ACharacter>(BlackboardComp->GetValueAsObject(FName("TargetPlayer")));
	
	if (bShouldPunish && TargetPlayer)
	{
		// This is a punishment chase - only go to last known location
		MyMemory->bIsChasingPlayer = true;
		MyMemory->LastKnownPlayerLocation = TargetPlayer->GetActorLocation();
		MyMemory->TargetLocation = MyMemory->LastKnownPlayerLocation;
		MyMemory->bHasReachedLastKnownLocation = false;
		MyMemory->SearchStartTime = GetWorld()->GetTimeSeconds();
		MyMemory->SearchElapsedTime = 0.0f;
		
		UE_LOG(LogTemp, Log, TEXT("[BTTask_MotherMoveToLocation] Starting punishment chase to last known location: %s"), 
			*MyMemory->LastKnownPlayerLocation.ToString());
	}
	else if (TargetPlayer)
	{
		// Normal player tracking
		MyMemory->bIsChasingPlayer = false;
		MyMemory->TargetLocation = TargetPlayer->GetActorLocation();
	}
	else
	{
		// Normal movement - get target location from blackboard
		FVector TargetLocation = BlackboardComp->GetValueAsVector(TargetLocationKey.SelectedKeyName);
		
		if (TargetLocation.IsNearlyZero())
		{
			UE_LOG(LogTemp, Warning, TEXT("[BTTask_MotherMoveToLocation] Target location is zero and no target player"));
			return EBTNodeResult::Failed;
		}

		MyMemory->TargetLocation = TargetLocation;
		MyMemory->bIsChasingPlayer = false;
	}

	MyMemory->bIsActive = true;

	return EBTNodeResult::InProgress;
}

void UBTTask_MotherMoveToLocation::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FMotherMoveToLocationMemory* MyMemory = reinterpret_cast<FMotherMoveToLocationMemory*>(NodeMemory);
	
	if (!MyMemory->bIsActive)
	{
		return;
	}

	AAIController* LocalAIController = OwnerComp.GetAIOwner();
	if (!LocalAIController)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	AMotherAIActor* MotherAI = Cast<AMotherAIActor>(LocalAIController->GetPawn());
	if (!MotherAI)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	FVector CurrentLocation = MotherAI->GetActorLocation();

	// Handle punishment chase mode
	if (MyMemory->bIsChasingPlayer)
	{
		ACharacter* TargetPlayer = Cast<ACharacter>(BlackboardComp->GetValueAsObject(FName("TargetPlayer")));
		
		// Check if player is detected by drones or mother
		bool bPlayerDetected = false;
		FVector DetectedPlayerLocation = FVector::ZeroVector;

		if (TargetPlayer)
		{
			// Check Mother's DetectedPlayers
			TArray<ACharacter*> MotherDetectedPlayers = MotherAI->GetDetectedPlayers();
			for (ACharacter* Detected : MotherDetectedPlayers)
			{
				if (Detected == TargetPlayer)
				{
					bPlayerDetected = true;
					DetectedPlayerLocation = Detected->GetActorLocation();
					break;
				}
			}

			// Check all drones' DetectedPlayers
			if (!bPlayerDetected)
			{
				TArray<ADroneActor*> ActiveDrones = MotherAI->GetActiveDrones();
				for (ADroneActor* Drone : ActiveDrones)
				{
					if (Drone)
					{
						TArray<ACharacter*> DroneDetectedPlayers = Drone->GetDetectedPlayers();
						for (ACharacter* Detected : DroneDetectedPlayers)
						{
							if (Detected == TargetPlayer)
							{
								bPlayerDetected = true;
								DetectedPlayerLocation = Detected->GetActorLocation();
								break;
							}
						}
						if (bPlayerDetected)
						{
							break;
						}
					}
				}
			}

			// Also check if Mother can directly see the player
			if (!bPlayerDetected && MotherAI->CanSeePlayer(TargetPlayer))
			{
				bPlayerDetected = true;
				DetectedPlayerLocation = TargetPlayer->GetActorLocation();
			}

			if (bPlayerDetected)
			{
				// Player found! Update last known location and continue chasing
				MyMemory->LastKnownPlayerLocation = DetectedPlayerLocation;
				MyMemory->TargetLocation = DetectedPlayerLocation;
				BlackboardComp->SetValueAsVector(FName("TargetLocation"), DetectedPlayerLocation);
				MyMemory->SearchStartTime = GetWorld()->GetTimeSeconds(); // Reset search timer
				MyMemory->SearchElapsedTime = 0.0f;
				MyMemory->bHasReachedLastKnownLocation = false;
				UE_LOG(LogTemp, Log, TEXT("[BTTask_MotherMoveToLocation] Player detected, updating location: %s"), 
					*DetectedPlayerLocation.ToString());
			}
			else
			{
				// Check if TargetLocation was updated by drone (drone reports new location via blackboard)
				FVector UpdatedLocation = BlackboardComp->GetValueAsVector(FName("TargetLocation"));
				if (!UpdatedLocation.IsNearlyZero() && FVector::Dist(UpdatedLocation, MyMemory->LastKnownPlayerLocation) > 50.0f)
				{
					// Drone reported new location
					MyMemory->LastKnownPlayerLocation = UpdatedLocation;
					MyMemory->TargetLocation = UpdatedLocation;
					MyMemory->SearchStartTime = GetWorld()->GetTimeSeconds(); // Reset search timer
					MyMemory->SearchElapsedTime = 0.0f;
					MyMemory->bHasReachedLastKnownLocation = false;
					UE_LOG(LogTemp, Log, TEXT("[BTTask_MotherMoveToLocation] Drone updated player location: %s"), 
						*UpdatedLocation.ToString());
				}
			}
		}

		// Check if reached last known location
		float DistanceToTarget = FVector::Dist(CurrentLocation, MyMemory->TargetLocation);
		if (!MyMemory->bHasReachedLastKnownLocation && DistanceToTarget <= AcceptableRadius)
		{
			MyMemory->bHasReachedLastKnownLocation = true;
			MyMemory->SearchStartTime = GetWorld()->GetTimeSeconds();
			MyMemory->SearchElapsedTime = 0.0f;
			UE_LOG(LogTemp, Log, TEXT("[BTTask_MotherMoveToLocation] Reached last known player location, starting patrol"));
			// Note: Patrol will be handled by BTTask_MotherPatrolAroundLastKnownLocation
		}

		// Update search elapsed time
		MyMemory->SearchElapsedTime = GetWorld()->GetTimeSeconds() - MyMemory->SearchStartTime;

		// Check search timeout
		if (MyMemory->SearchElapsedTime >= SearchTimeout)
		{
			// Search failed - apply credit penalty
			if (UWorld* World = GetWorld())
			{
				if (APickpackerGameMode* GameMode = Cast<APickpackerGameMode>(World->GetAuthGameMode()))
				{
					GameMode->ApplyCreditDelta(-CreditPenaltyOnFailure, 
						FString::Printf(TEXT("Failed to find player during punishment chase")));
					UE_LOG(LogTemp, Warning, TEXT("[BTTask_MotherMoveToLocation] Search timeout - applying credit penalty: %d"), 
						CreditPenaltyOnFailure);
				}
			}

			// Clear target player and ShouldPunish from blackboard
			BlackboardComp->SetValueAsObject(FName("TargetPlayer"), nullptr);
			BlackboardComp->SetValueAsBool(FName("ShouldPunish"), false);
			
			MyMemory->bIsActive = false;
			FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
			return;
		}

		// Check punishment distance - if close enough, make player move to mother instead
		if (TargetPlayer && bPlayerDetected)
		{
			float DistanceToPlayer = FVector::Dist(CurrentLocation, TargetPlayer->GetActorLocation());
			float PunishmentDistance = MotherAI->GetPunishmentDistance();
			
			if (DistanceToPlayer <= PunishmentDistance)
			{
				// Close enough - make player move to mother (obstacle avoidance)
				ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(TargetPlayer);
				if (BlasterCharacter && BlasterCharacter->HasAuthority())
				{
					// Calculate target position for player (slightly closer to mother)
					FVector PlayerLocation = TargetPlayer->GetActorLocation();
					FVector DirectionToMother = (CurrentLocation - PlayerLocation).GetSafeNormal();
					FVector TargetPosition = CurrentLocation - DirectionToMother * (PunishmentDistance * 0.8f);
					
					// Move player towards target position using character movement
					if (UCharacterMovementComponent* PlayerMovement = BlasterCharacter->GetCharacterMovement())
					{
						FVector MoveDirection = (TargetPosition - PlayerLocation).GetSafeNormal();
						float MoveDistance = FVector::Dist(PlayerLocation, TargetPosition);
						
						if (MoveDistance > 10.0f) // Only move if not already at target
						{
							// Use AddMovementInput to move player (works on server)
							BlasterCharacter->AddMovementInput(MoveDirection, 1.0f);
						}
					}
				}
				
				// Don't move mother, wait for player to come closer
				// Check if close enough to execute punishment
				if (DistanceToPlayer <= PunishmentDistance * 0.9f)
				{
					// Close enough - task succeeded, punishment will execute
					MyMemory->bIsActive = false;
					FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
					return;
				}
				return;
			}
			else
			{
				// Still far - continue chasing
				MyMemory->TargetLocation = TargetPlayer->GetActorLocation();
			}
		}
		// If reached last known location and player not visible, task should succeed to allow patrol task
		else if (MyMemory->bHasReachedLastKnownLocation)
		{
			// Task succeeded, patrol task will take over
			MyMemory->bIsActive = false;
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
			return;
		}
	}
	else
	{
		// Normal movement mode - update target if it's a player
		ACharacter* TargetPlayer = Cast<ACharacter>(BlackboardComp->GetValueAsObject(FName("TargetPlayer")));
		if (TargetPlayer)
		{
			MyMemory->TargetLocation = TargetPlayer->GetActorLocation();
		}
		else
		{
			// Get from blackboard
			FVector TargetLocation = BlackboardComp->GetValueAsVector(TargetLocationKey.SelectedKeyName);
			if (!TargetLocation.IsNearlyZero())
			{
				MyMemory->TargetLocation = TargetLocation;
			}
		}
	}

	float Distance = FVector::Dist(CurrentLocation, MyMemory->TargetLocation);

	// Check if reached
	if (Distance <= AcceptableRadius)
	{
		if (!MyMemory->bIsChasingPlayer)
		{
			MyMemory->bIsActive = false;
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		}
		return;
	}

	// Move towards target using navigation if available
	if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
	{
		FAIMoveRequest MoveReq;
		MoveReq.SetGoalLocation(MyMemory->TargetLocation);
		MoveReq.SetAcceptanceRadius(AcceptableRadius);
		MoveReq.SetUsePathfinding(true);
		MoveReq.SetAllowPartialPath(true);
		
		FPathFindingQuery Query;
		
		if (LocalAIController->BuildPathfindingQuery(MoveReq, Query))
		{
			const FPathFindingResult PathResult = NavSys->FindPathSync(Query, EPathFindingMode::Regular);
			if (PathResult.IsSuccessful() && PathResult.Path.IsValid())
			{
				EPathFollowingRequestResult::Type MoveResult = LocalAIController->MoveToLocation(MyMemory->TargetLocation, AcceptableRadius, false, true, false, false, nullptr, true);
				if (MoveResult == EPathFollowingRequestResult::Type::Failed)
				{
					// Fallback to direct movement
					MotherAI->MoveToLocation(MyMemory->TargetLocation, MovementSpeed);
				}
			}
			else
			{
				// Fallback to direct movement if pathfinding fails
				MotherAI->MoveToLocation(MyMemory->TargetLocation, MovementSpeed);
			}
		}
		else
		{
			// Fallback if we couldn't build a query
			MotherAI->MoveToLocation(MyMemory->TargetLocation, MovementSpeed);
		}
	}
	else
	{
		// Fallback to direct movement if no navigation system
		MotherAI->MoveToLocation(MyMemory->TargetLocation, MovementSpeed);
	}
}

EBTNodeResult::Type UBTTask_MotherMoveToLocation::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FMotherMoveToLocationMemory* MyMemory = reinterpret_cast<FMotherMoveToLocationMemory*>(NodeMemory);
	MyMemory->bIsActive = false;

	return EBTNodeResult::Aborted;
}

