// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MotherGeneratePatrolPoint.generated.h"

/**
 * Task to generate a random patrol point within inspection volume
 * This replaces BTService_MotherGeneratePatrolPoint - generates point on demand instead of ticking
 */
UCLASS()
class BLASTER_API UBTTask_MotherGeneratePatrolPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MotherGeneratePatrolPoint();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

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

private:
	/** Get bounds from inspection actor */
	bool GetInspectionVolumeBounds(AActor* InspectionActor, FVector& OutCenter, FVector& OutExtent) const;

	/** Generate random patrol point within bounds */
	FVector GenerateRandomPatrolPoint(const FVector& Center, const FVector& Extent) const;
};
