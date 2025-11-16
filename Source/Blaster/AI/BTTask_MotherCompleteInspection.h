// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MotherCompleteInspection.generated.h"

/**
 * Complete inspection task (for Mother AI)
 */
UCLASS()
class BLASTER_API UBTTask_MotherCompleteInspection : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MotherCompleteInspection();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};

