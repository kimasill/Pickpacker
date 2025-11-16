// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_MotherCheckSuspiciousPlayers.generated.h"

/**
 * Service to check for suspicious players and update blackboard (for Mother AI)
 */
UCLASS()
class BLASTER_API UBTService_MotherCheckSuspiciousPlayers : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_MotherCheckSuspiciousPlayers();

	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	/** Target player key in blackboard */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetPlayerKey;

	/** Detection range */
	UPROPERTY(EditAnywhere, Category = "Detection", meta = (ClampMin = "0.0"))
	float DetectionRange = 2000.0f;

	/** Check interval in seconds */
	UPROPERTY(EditAnywhere, Category = "Detection", meta = (ClampMin = "0.0"))
	float CheckInterval = 0.5f;

private:
	float LastCheckTime = 0.0f;
};

