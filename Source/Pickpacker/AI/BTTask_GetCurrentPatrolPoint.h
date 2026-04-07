// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_GetCurrentPatrolPoint.generated.h"

/**
 * Get Current Patrol Point Task
 */
UCLASS()
class PICKPACKER_API UBTTask_GetCurrentPatrolPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_GetCurrentPatrolPoint();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};














