// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_PPCombatAttackTarget.generated.h"

/**
 * Executes UNPCCombatComponent::TryAttack on the controlled pawn.
 */
UCLASS()
class PICKPACKER_API UBTTask_PPCombatAttackTarget : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_PPCombatAttackTarget();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;
};
