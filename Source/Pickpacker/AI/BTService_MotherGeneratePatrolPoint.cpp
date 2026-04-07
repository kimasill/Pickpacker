// Fill out your copyright notice in the Description page of Project Settings.

#include "BTService_MotherGeneratePatrolPoint.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "AI/MotherAIActor.h"
#include "AI/MotherAIController.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"

UBTService_MotherGeneratePatrolPoint::UBTService_MotherGeneratePatrolPoint()
{
	NodeName = TEXT("Generate Patrol Point");
	bCreateNodeInstance = true;
	Interval = 0.5f; // Check every 0.5 seconds
	RandomDeviation = 0.0f;
}

void UBTService_MotherGeneratePatrolPoint::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	// Generate initial patrol point when service becomes relevant
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return;
	}

	AMotherAIActor* MotherAI = Cast<AMotherAIActor>(AIController->GetPawn());
	if (!MotherAI)
	{
		return;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return;
	}

	// Get inspection actor
	AActor* InspectionActor = MotherAI->GetCurrentInspectionActor();
	if (!InspectionActor && InspectionActorKey.IsSet())
	{
		InspectionActor = Cast<AActor>(BlackboardComp->GetValueAsObject(InspectionActorKey.SelectedKeyName));
	}

	if (!InspectionActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTService_MotherGeneratePatrolPoint] No inspection actor available"));
		return;
	}

	// Get volume bounds
	FVector VolumeCenter;
	FVector VolumeExtent;
	if (!GetInspectionVolumeBounds(InspectionActor, VolumeCenter, VolumeExtent))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTService_MotherGeneratePatrolPoint] Failed to get volume bounds"));
		return;
	}

	// Generate first patrol point
	FVector PatrolPoint = GenerateRandomPatrolPoint(VolumeCenter, VolumeExtent);
	BlackboardComp->SetValueAsVector(PatrolTargetLocationKey.SelectedKeyName, PatrolPoint);

	// Initialize point count tracking
	if (VisitedPointCountKey.IsSet())
	{
		LastKnownPointCount = BlackboardComp->GetValueAsInt(VisitedPointCountKey.SelectedKeyName);
		// Reset count to 0 if starting new patrol
		BlackboardComp->SetValueAsInt(VisitedPointCountKey.SelectedKeyName, 0);
		LastKnownPointCount = 0;
	}

	UE_LOG(LogTemp, Log, TEXT("[BTService_MotherGeneratePatrolPoint] Generated initial patrol point: %s"), 
		*PatrolPoint.ToString());
}

void UBTService_MotherGeneratePatrolPoint::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return;
	}

	AMotherAIActor* MotherAI = Cast<AMotherAIActor>(AIController->GetPawn());
	if (!MotherAI)
	{
		return;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return;
	}

	// Check if point count has increased (meaning MoveTo completed and IncrementPatrolCount was called)
	if (VisitedPointCountKey.IsSet())
	{
		int32 CurrentPointCount = BlackboardComp->GetValueAsInt(VisitedPointCountKey.SelectedKeyName);
		
		// If count increased, generate new patrol point
		if (CurrentPointCount > LastKnownPointCount)
		{
			LastKnownPointCount = CurrentPointCount;

			// Generate new patrol point
			AActor* InspectionActor = MotherAI->GetCurrentInspectionActor();
			if (!InspectionActor && InspectionActorKey.IsSet())
			{
				InspectionActor = Cast<AActor>(BlackboardComp->GetValueAsObject(InspectionActorKey.SelectedKeyName));
			}

			if (!InspectionActor)
			{
				return;
			}

			// Get volume bounds
			FVector VolumeCenter;
			FVector VolumeExtent;
			if (!GetInspectionVolumeBounds(InspectionActor, VolumeCenter, VolumeExtent))
			{
				return;
			}

			// Generate new patrol point
			FVector NewPatrolPoint = GenerateRandomPatrolPoint(VolumeCenter, VolumeExtent);
			BlackboardComp->SetValueAsVector(PatrolTargetLocationKey.SelectedKeyName, NewPatrolPoint);

			UE_LOG(LogTemp, Log, TEXT("[BTService_MotherGeneratePatrolPoint] Generated new patrol point (count: %d): %s"), 
				CurrentPointCount, *NewPatrolPoint.ToString());
		}
	}
	else
	{
		// Fallback: if VisitedPointCountKey is not set, use distance-based check (for backward compatibility)
		FVector CurrentPatrolPoint = BlackboardComp->GetValueAsVector(PatrolTargetLocationKey.SelectedKeyName);
		
		if (!CurrentPatrolPoint.IsNearlyZero())
		{
			FVector CurrentLocation = MotherAI->GetActorLocation();
			float Distance = FVector::Dist(CurrentLocation, CurrentPatrolPoint);
			
			// Use AcceptableRadius if still defined, otherwise default to 100.0f
			float Radius = 100.0f; // Default fallback
			
			if (Distance <= Radius)
			{
				// Generate new patrol point (same logic as above)
				AActor* InspectionActor = MotherAI->GetCurrentInspectionActor();
				if (!InspectionActor && InspectionActorKey.IsSet())
				{
					InspectionActor = Cast<AActor>(BlackboardComp->GetValueAsObject(InspectionActorKey.SelectedKeyName));
				}

				if (InspectionActor)
				{
					FVector VolumeCenter;
					FVector VolumeExtent;
					if (GetInspectionVolumeBounds(InspectionActor, VolumeCenter, VolumeExtent))
					{
						FVector NewPatrolPoint = GenerateRandomPatrolPoint(VolumeCenter, VolumeExtent);
						BlackboardComp->SetValueAsVector(PatrolTargetLocationKey.SelectedKeyName, NewPatrolPoint);

						UE_LOG(LogTemp, Log, TEXT("[BTService_MotherGeneratePatrolPoint] Generated new patrol point (distance-based fallback): %s"), 
							*NewPatrolPoint.ToString());
					}
				}
			}
		}
	}
}

bool UBTService_MotherGeneratePatrolPoint::GetInspectionVolumeBounds(AActor* InspectionActor, FVector& OutCenter, FVector& OutExtent) const
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

FVector UBTService_MotherGeneratePatrolPoint::GenerateRandomPatrolPoint(const FVector& Center, const FVector& Extent) const
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
