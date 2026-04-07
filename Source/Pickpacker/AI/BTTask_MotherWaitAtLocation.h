// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MotherWaitAtLocation.generated.h"

/**
 * Wait at a location for specified duration (for Mother AI)
 */
UCLASS()
class PICKPACKER_API UBTTask_MotherWaitAtLocation : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MotherWaitAtLocation();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	/** Wait duration in seconds */
	UPROPERTY(EditAnywhere, Category = "Wait", meta = (ClampMin = "0.0"))
	float WaitDuration = 30.0f;

private:
	/** Elapsed time */
	float ElapsedTime = 0.0f;
};

