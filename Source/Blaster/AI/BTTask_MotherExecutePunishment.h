// Copyright notice
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BTTask_MotherExecutePunishment.generated.h"

/**
 * Execute punishment on target player (for Mother AI)
 */
UCLASS()
class BLASTER_API UBTTask_MotherExecutePunishment : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MotherExecutePunishment();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	/** Target player key in blackboard */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetPlayerKey;
};

