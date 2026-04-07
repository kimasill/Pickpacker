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
	FVector LastKnownPlayerLocation;
	float SearchStartTime;
	float SearchElapsedTime;
	bool bIsActive;
	bool bIsChasingPlayer;
	bool bHasReachedLastKnownLocation;

	FMotherMoveToLocationMemory()
		: TargetLocation(FVector::ZeroVector)
		, LastKnownPlayerLocation(FVector::ZeroVector)
		, SearchStartTime(0.0f)
		, SearchElapsedTime(0.0f)
		, bIsActive(false)
		, bIsChasingPlayer(false)
		, bHasReachedLastKnownLocation(false)
	{
	}
};

/**
 * Move to a specific location (for Mother AI)
 */
UCLASS()
class PICKPACKER_API UBTTask_MotherMoveToLocation : public UBTTaskNode
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

	/** Search timeout when chasing player (seconds) */
	UPROPERTY(EditAnywhere, Category = "Punishment", meta = (ClampMin = "0.0"))
	float SearchTimeout = 10.0f;

	/** Credit penalty when search fails */
	UPROPERTY(EditAnywhere, Category = "Punishment", meta = (ClampMin = "0"))
	int32 CreditPenaltyOnFailure = 5;
};

