// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_WaitAtPatrolPoint.generated.h"

/**
 * Wait at Patrol Point Task
 */
UCLASS()
class BLASTER_API UBTTask_WaitAtPatrolPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_WaitAtPatrolPoint();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:
	/** Wait time in seconds */
	UPROPERTY(EditAnywhere, Category = "Wait")
	float WaitTime = 2.0f;

	/** Use wait time from patrol point */
	UPROPERTY(EditAnywhere, Category = "Wait")
	bool bUsePatrolPointWaitTime = true;

private:
	/** Elapsed time */
	float ElapsedTime;
};











