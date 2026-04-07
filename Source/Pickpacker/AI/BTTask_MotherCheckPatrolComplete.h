// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MotherCheckPatrolComplete.generated.h"

/**
 * Node Memory for Check Patrol Complete Task
 */
USTRUCT()
struct FMotherCheckPatrolCompleteMemory
{
	GENERATED_BODY()

	float ElapsedTime;
	bool bIsActive;

	FMotherCheckPatrolCompleteMemory()
		: ElapsedTime(0.0f)
		, bIsActive(false)
	{
	}
};

/**
 * Task to check if patrol is complete (either by time or by number of points visited)
 * This task checks completion conditions only - point counting is handled by BTTask_MotherIncrementPatrolCount
 * This task should be placed in a sequence that loops until patrol is complete
 */
UCLASS()
class PICKPACKER_API UBTTask_MotherCheckPatrolComplete : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MotherCheckPatrolComplete();

	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FMotherCheckPatrolCompleteMemory); }
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	/** Patrol duration in seconds (0 = use point count instead) */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float PatrolDuration = 30.0f;

	/** Number of patrol points to visit (0 = use duration instead) */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0"))
	int32 RequiredPointCount = 0;

	/** Blackboard key to track visited point count (required for point-based completion) */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector VisitedPointCountKey;
};
