// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MotherGeneratePatrolPoint.generated.h"

/**
 * Mother 점검용 — Inspection 액터는 Mother 또는 Blackboard에서 가져온 뒤 PPPatrolBoundsLibrary로 샘플링.
 */
UCLASS()
class PICKPACKER_API UBTTask_MotherGeneratePatrolPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MotherGeneratePatrolPoint();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector PatrolTargetLocationKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector InspectionActorKey;

	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float MinPatrolRadius = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float MaxPatrolRadius = 500.0f;
};
