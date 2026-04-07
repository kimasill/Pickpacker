// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_MotherCheckInspectionTime.generated.h"

/**
 * Service to check if it's time for inspection and update blackboard (for Mother AI)
 */
UCLASS()
class PICKPACKER_API UBTService_MotherCheckInspectionTime : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_MotherCheckInspectionTime();

	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	/** Should inspect key in blackboard */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector ShouldInspectKey;

	/** Inspection location key in blackboard */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector InspectionLocationKey;
};

