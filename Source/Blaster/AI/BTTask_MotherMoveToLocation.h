// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MotherMoveToLocation.generated.h"

/**
 * Node Memory for Move To Location Task
 */
USTRUCT()
struct FMotherMoveToLocationMemory
{
	GENERATED_BODY()

	FVector TargetLocation;
	bool bIsActive;

	FMotherMoveToLocationMemory()
		: TargetLocation(FVector::ZeroVector)
		, bIsActive(false)
	{
	}
};

/**
 * Move to a specific location (for Mother AI)
 */
UCLASS()
class BLASTER_API UBTTask_MotherMoveToLocation : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MotherMoveToLocation();

	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FMotherMoveToLocationMemory); }
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	/** Target location key in blackboard */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetLocationKey;

	/** Movement speed */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0"))
	float MovementSpeed = 400.0f;

	/** Acceptable radius to consider reached */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0"))
	float AcceptableRadius = 100.0f;
};

