// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_MotherGeneratePatrolPoint.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "Blaster/AI/MotherAIActor.h"
#include "Blaster/AI/MotherAIController.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"

UBTTask_MotherGeneratePatrolPoint::UBTTask_MotherGeneratePatrolPoint()
{
	NodeName = TEXT("Generate Patrol Point");
	bCreateNodeInstance = false;
}

EBTNodeResult::Type UBTTask_MotherGeneratePatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
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

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	// Get inspection actor
	AActor* InspectionActor = MotherAI->GetCurrentInspectionActor();
	if (!InspectionActor && InspectionActorKey.IsSet())
	{
		InspectionActor = Cast<AActor>(BlackboardComp->GetValueAsObject(InspectionActorKey.SelectedKeyName));
	}

	if (!InspectionActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_MotherGeneratePatrolPoint] No inspection actor available"));
		return EBTNodeResult::Failed;
	}

	// Get volume bounds
	FVector VolumeCenter;
	FVector VolumeExtent;
	if (!GetInspectionVolumeBounds(InspectionActor, VolumeCenter, VolumeExtent))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_MotherGeneratePatrolPoint] Failed to get volume bounds"));
		return EBTNodeResult::Failed;
	}

	// Generate patrol point
	FVector PatrolPoint = GenerateRandomPatrolPoint(VolumeCenter, VolumeExtent);
	BlackboardComp->SetValueAsVector(PatrolTargetLocationKey.SelectedKeyName, PatrolPoint);

	UE_LOG(LogTemp, Log, TEXT("[BTTask_MotherGeneratePatrolPoint] Generated patrol point: %s"), 
		*PatrolPoint.ToString());

	return EBTNodeResult::Succeeded;
}

bool UBTTask_MotherGeneratePatrolPoint::GetInspectionVolumeBounds(AActor* InspectionActor, FVector& OutCenter, FVector& OutExtent) const
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

FVector UBTTask_MotherGeneratePatrolPoint::GenerateRandomPatrolPoint(const FVector& Center, const FVector& Extent) const
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
