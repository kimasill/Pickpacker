// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_MotherPatrolInVolume.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "Blaster/AI/MotherAIActor.h"
#include "Blaster/AI/MotherAIController.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Navigation/PathFollowingComponent.h"

UBTTask_MotherPatrolInVolume::UBTTask_MotherPatrolInVolume()
{
	NodeName = TEXT("Mother Patrol In Volume");
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_MotherPatrolInVolume::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FMotherPatrolInVolumeMemory* MyMemory = reinterpret_cast<FMotherPatrolInVolumeMemory*>(NodeMemory);
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

	AActor* InspectionActor = MotherAI->GetCurrentInspectionActor();

	if (!InspectionActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_MotherPatrolInVolume] No inspection actor available"));
		return EBTNodeResult::Failed;
	}

	// Get volume bounds
	FVector VolumeCenter;
	FVector VolumeExtent;
	if (!GetInspectionVolumeBounds(InspectionActor, VolumeCenter, VolumeExtent))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_MotherPatrolInVolume] Failed to get volume bounds"));
		return EBTNodeResult::Failed;
	}

	// Generate first patrol point
	MyMemory->CurrentPatrolTarget = GenerateRandomPatrolPoint(VolumeCenter, VolumeExtent);
	MyMemory->ElapsedTime = 0.0f;
	MyMemory->WaitTime = 0.0f;
	MyMemory->bIsActive = true;

	UE_LOG(LogTemp, Log, TEXT("[BTTask_MotherPatrolInVolume] Starting patrol in volume for %.2f seconds"), PatrolDuration);

	return EBTNodeResult::InProgress;
}

void UBTTask_MotherPatrolInVolume::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FMotherPatrolInVolumeMemory* MyMemory = reinterpret_cast<FMotherPatrolInVolumeMemory*>(NodeMemory);
	
	if (!MyMemory->bIsActive)
	{
		return;
	}

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	AMotherAIActor* MotherAI = Cast<AMotherAIActor>(AIController->GetPawn());
	if (!MotherAI)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	MyMemory->ElapsedTime += DeltaSeconds;

	// Check if patrol duration exceeded
	if (MyMemory->ElapsedTime >= PatrolDuration)
	{
		MyMemory->bIsActive = false;
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	FVector CurrentLocation = MotherAI->GetActorLocation();
	float Distance = FVector::Dist(CurrentLocation, MyMemory->CurrentPatrolTarget);

	// Check if reached current patrol point
	if (Distance <= AcceptableRadius)
	{
		// Wait at point for specified duration
		MyMemory->WaitTime += DeltaSeconds;
		
		if (MyMemory->WaitTime >= WaitAtPointDuration)
		{
			// Generate new patrol point
			AActor* InspectionActor = MotherAI->GetCurrentInspectionActor();
			if (InspectionActor)
			{
				FVector VolumeCenter;
				FVector VolumeExtent;
				if (GetInspectionVolumeBounds(InspectionActor, VolumeCenter, VolumeExtent))
				{
					MyMemory->CurrentPatrolTarget = GenerateRandomPatrolPoint(VolumeCenter, VolumeExtent);
					MyMemory->WaitTime = 0.0f;
				}
			}
		}
	}
	else
	{
		// Reset wait time when moving
		MyMemory->WaitTime = 0.0f;
		// Move towards patrol target using navigation if possible
		if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
		{
			FAIMoveRequest MoveReq;
			MoveReq.SetGoalLocation(MyMemory->CurrentPatrolTarget);
			MoveReq.SetAcceptanceRadius(AcceptableRadius);
			MoveReq.SetUsePathfinding(true);
			MoveReq.SetAllowPartialPath(true);

			FPathFindingQuery Query;
			if (AIController->BuildPathfindingQuery(MoveReq, Query))
			{
				const FPathFindingResult PathResult = NavSys->FindPathSync(Query, EPathFindingMode::Regular);
				if (PathResult.IsSuccessful() && PathResult.Path.IsValid())
				{
					const EPathFollowingRequestResult::Type MoveResult = AIController->MoveToLocation(MyMemory->CurrentPatrolTarget, AcceptableRadius, false, true, false, false, nullptr, true);
					if (MoveResult == EPathFollowingRequestResult::Type::Failed)
					{
						// Fallback to direct movement
						MotherAI->MoveToLocation(MyMemory->CurrentPatrolTarget, MovementSpeed);
					}
				}
				else
				{
					// Fallback to direct movement if pathfinding fails
					MotherAI->MoveToLocation(MyMemory->CurrentPatrolTarget, MovementSpeed);
				}
			}
			else
			{
				// Fallback if query couldn't be built
				MotherAI->MoveToLocation(MyMemory->CurrentPatrolTarget, MovementSpeed);
			}
		}
		else
		{
			// Fallback to direct movement if no navigation system
			MotherAI->MoveToLocation(MyMemory->CurrentPatrolTarget, MovementSpeed);
		}
	}
}

EBTNodeResult::Type UBTTask_MotherPatrolInVolume::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FMotherPatrolInVolumeMemory* MyMemory = reinterpret_cast<FMotherPatrolInVolumeMemory*>(NodeMemory);
	MyMemory->bIsActive = false;

	return EBTNodeResult::Aborted;
}

bool UBTTask_MotherPatrolInVolume::GetInspectionVolumeBounds(AActor* InspectionActor, FVector& OutCenter, FVector& OutExtent) const
{
	if (!InspectionActor)
	{
		return false;
	}

	// Try to find BoxComponent first
	if (UBoxComponent* BoxComp = InspectionActor->FindComponentByClass<UBoxComponent>())
	{
		OutCenter = BoxComp->GetComponentLocation();
		OutExtent = BoxComp->GetScaledBoxExtent();
		return true;
	}

	// Try SphereComponent
	if (USphereComponent* SphereComp = InspectionActor->FindComponentByClass<USphereComponent>())
	{
		OutCenter = SphereComp->GetComponentLocation();
		float Radius = SphereComp->GetScaledSphereRadius();
		OutExtent = FVector(Radius, Radius, Radius);
		return true;
	}

	// Fallback: use actor bounds
	FVector Origin;
	FVector BoxExtent;
	InspectionActor->GetActorBounds(false, Origin, BoxExtent);
	OutCenter = Origin;
	OutExtent = BoxExtent;

	return true;
}

FVector UBTTask_MotherPatrolInVolume::GenerateRandomPatrolPoint(const FVector& Center, const FVector& Extent) const
{
	// Generate random point within bounds
	FVector RandomPoint;
	RandomPoint.X = FMath::RandRange(Center.X - Extent.X, Center.X + Extent.X);
	RandomPoint.Y = FMath::RandRange(Center.Y - Extent.Y, Center.Y + Extent.Y);
	RandomPoint.Z = Center.Z; // Keep Z at center level (or adjust as needed)

	// Ensure point is within min/max radius from center
	FVector ToPoint = RandomPoint - Center;
	ToPoint.Z = 0.0f; // Only consider horizontal distance
	float Distance = ToPoint.Size();

	if (Distance < MinPatrolRadius)
	{
		// Move point to minimum radius
		ToPoint = ToPoint.GetSafeNormal() * MinPatrolRadius;
		RandomPoint = Center + ToPoint;
		RandomPoint.Z = Center.Z;
	}
	else if (Distance > MaxPatrolRadius)
	{
		// Move point to maximum radius
		ToPoint = ToPoint.GetSafeNormal() * MaxPatrolRadius;
		RandomPoint = Center + ToPoint;
		RandomPoint.Z = Center.Z;
	}

	return RandomPoint;
}
