// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MotherPatrolAroundLastKnownLocation.generated.h"

/**
 * Generate patrol point around last known player location (for Mother AI punishment chase)
 * This task only generates a valid NavMesh point and stores it in Blackboard.
 * Movement is handled by standard Move To task.
 */
UCLASS()
class BLASTER_API UBTTask_MotherPatrolAroundLastKnownLocation : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MotherPatrolAroundLastKnownLocation();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	/** Blackboard key to store generated patrol point location */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector PatrolPointLocationKey;

	/** Patrol radius around last known location */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float PatrolRadius = 300.0f;

	/** Maximum attempts to find valid NavMesh point */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "1", ClampMax = "20"))
	int32 MaxAttempts = 10;

private:
	/** Generate random patrol point around center on NavMesh */
	FVector GenerateRandomPatrolPointOnNavMesh(const FVector& Center, float Radius, UWorld* World) const;
};
