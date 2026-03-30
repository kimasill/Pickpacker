// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_BruteExecuteTarget.generated.h"

/**
 * Blackboard의 타겟 플레이어에 대해 Brute 처형 실행 (BT 기반 Brute용).
 */
UCLASS()
class BLASTER_API UBTTask_BruteExecuteTarget : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_BruteExecuteTarget();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetPlayerKey;
};
