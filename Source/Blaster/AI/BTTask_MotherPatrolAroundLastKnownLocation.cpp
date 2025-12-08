// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_MotherPatrolAroundLastKnownLocation.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "Blaster/AI/MotherAIActor.h"
#include "Blaster/AI/MotherAIController.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Engine/World.h"

UBTTask_MotherPatrolAroundLastKnownLocation::UBTTask_MotherPatrolAroundLastKnownLocation()
{
	NodeName = TEXT("Generate Patrol Point Around Last Known Location");
	bCreateNodeInstance = false;
}

EBTNodeResult::Type UBTTask_MotherPatrolAroundLastKnownLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	AMotherAIActor* MotherAI = Cast<AMotherAIActor>(AIController->GetPawn());
	if (!MotherAI)
	{
		return EBTNodeResult::Failed;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return EBTNodeResult::Failed;
	}

	// Get last known location from blackboard or target player
	FVector LastKnownLocation = FVector::ZeroVector;
	ACharacter* TargetPlayer = Cast<ACharacter>(BlackboardComp->GetValueAsObject(FName("TargetPlayer")));
	if (TargetPlayer)
	{
		LastKnownLocation = TargetPlayer->GetActorLocation();
	}
	else
	{
		// Try to get from TargetLocation
		FVector TargetLocation = BlackboardComp->GetValueAsVector(FName("TargetLocation"));
		if (!TargetLocation.IsNearlyZero())
		{
			LastKnownLocation = TargetLocation;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[BTTask_MotherPatrolAroundLastKnownLocation] No last known location available"));
			return EBTNodeResult::Failed;
		}
	}

	// Generate valid patrol point on NavMesh
	FVector PatrolPoint = GenerateRandomPatrolPointOnNavMesh(LastKnownLocation, PatrolRadius, World);
	
	if (PatrolPoint.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_MotherPatrolAroundLastKnownLocation] Failed to generate valid patrol point"));
		return EBTNodeResult::Failed;
	}

	// Store patrol point in blackboard
	if (PatrolPointLocationKey.IsSet())
	{
		BlackboardComp->SetValueAsVector(PatrolPointLocationKey.SelectedKeyName, PatrolPoint);
	}
	else
	{
		// Fallback to TargetLocation if key not set
		BlackboardComp->SetValueAsVector(FName("TargetLocation"), PatrolPoint);
	}

	// Set AI state to Searching
	MotherAI->SetAIState(EMotherAIState::Searching);

	UE_LOG(LogTemp, Log, TEXT("[BTTask_MotherPatrolAroundLastKnownLocation] Generated patrol point: %s"), 
		*PatrolPoint.ToString());

	return EBTNodeResult::Succeeded;
}

FVector UBTTask_MotherPatrolAroundLastKnownLocation::GenerateRandomPatrolPointOnNavMesh(const FVector& Center, float Radius, UWorld* World) const
{
	if (!World)
	{
		return FVector::ZeroVector;
	}

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	if (!NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_MotherPatrolAroundLastKnownLocation] Navigation system not found"));
		return FVector::ZeroVector;
	}

	FNavLocation NavLocation;
	
	// Try to find valid NavMesh point within radius
	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		// Generate random point on circle around center
		float Angle = FMath::RandRange(0.0f, 2.0f * PI);
		float Distance = FMath::RandRange(Radius * 0.3f, Radius);
		
		FVector RandomPoint;
		RandomPoint.X = Center.X + FMath::Cos(Angle) * Distance;
		RandomPoint.Y = Center.Y + FMath::Sin(Angle) * Distance;
		RandomPoint.Z = Center.Z; // Keep Z at center level

		// Project to NavMesh (Point first, then OutLocation, then Extent)
		if (NavSys->ProjectPointToNavigation(RandomPoint, NavLocation, FVector(Radius, Radius, 200.0f)))
		{
			return NavLocation.Location;
		}
	}

	// If all attempts failed, try to find any valid point near center
	if (NavSys->ProjectPointToNavigation(Center, NavLocation, FVector(Radius * 2.0f, Radius * 2.0f, 200.0f)))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_MotherPatrolAroundLastKnownLocation] Using fallback NavMesh point near center"));
		return NavLocation.Location;
	}

	UE_LOG(LogTemp, Warning, TEXT("[BTTask_MotherPatrolAroundLastKnownLocation] Failed to find any valid NavMesh point"));
	return FVector::ZeroVector;
}
