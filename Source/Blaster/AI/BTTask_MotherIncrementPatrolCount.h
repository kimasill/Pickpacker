// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MotherIncrementPatrolCount.generated.h"

/**
 * Task to increment patrol point count when MoveTo completes successfully
 * This should be placed after MoveTo task in the sequence
 */
UCLASS()
class BLASTER_API UBTTask_MotherIncrementPatrolCount : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MotherIncrementPatrolCount();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	/** Blackboard key to track visited point count */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector VisitedPointCountKey;
};
