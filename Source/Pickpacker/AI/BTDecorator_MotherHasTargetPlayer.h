// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_MotherHasTargetPlayer.generated.h"

/**
 * Decorator to check if Mother AI has a target player (for Behavior Tree)
 */
UCLASS()
class PICKPACKER_API UBTDecorator_MotherHasTargetPlayer : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_MotherHasTargetPlayer();

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;

	/** Target player key in blackboard */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetPlayerKey;
};

