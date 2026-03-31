// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_MotherGeneratePatrolPoint.generated.h"

/**
 * Service to generate random patrol points within inspection volume and update blackboard (for Mother AI)
 * 
 * @deprecated This service has been replaced with BTTask_MotherGeneratePatrolPoint
 * The task-based approach is cleaner and doesn't require ticking.
 * See PATROL_REFACTOR_GUIDE.md for migration instructions.
 */
UCLASS()
class BLASTER_API UBTService_MotherGeneratePatrolPoint : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_MotherGeneratePatrolPoint();

	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	/** Patrol target location key in blackboard */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector PatrolTargetLocationKey;

	/** Inspection actor key in blackboard (optional, for getting volume bounds) */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector InspectionActorKey;

	/** Minimum distance from center to generate patrol point */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float MinPatrolRadius = 50.0f;

	/** Maximum distance from center to generate patrol point */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float MaxPatrolRadius = 500.0f;

	/** Blackboard key to track visited point count (used to detect when to generate next point) */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector VisitedPointCountKey;

	/** Last known point count (to detect when count increases) */
	UPROPERTY()
	int32 LastKnownPointCount = -1;

private:
	/** Get bounds from inspection actor */
	bool GetInspectionVolumeBounds(AActor* InspectionActor, FVector& OutCenter, FVector& OutExtent) const;

	/** Generate random patrol point within bounds */
	FVector GenerateRandomPatrolPoint(const FVector& Center, const FVector& Extent) const;
};
