// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_MotherCheckPlayerProximity.generated.h"

/**
 * Service to check if Mother AI is close enough to player for punishment
 * Updates Chasing blackboard variable
 */
UCLASS()
class PICKPACKER_API UBTService_MotherCheckPlayerProximity : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_MotherCheckPlayerProximity();

	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

	/** Chasing blackboard key */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector ChasingKey;

	/** Distance threshold to consider close enough (punishment distance) */
	UPROPERTY(EditAnywhere, Category = "Proximity", meta = (ClampMin = "0.0"))
	float ProximityDistance = 200.0f;

	/** Check interval in seconds */
	UPROPERTY(EditAnywhere, Category = "Proximity", meta = (ClampMin = "0.0"))
	float CheckInterval = 0.1f;

private:
	float LastCheckTime = 0.0f;
};
