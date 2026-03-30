// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blaster/AI/PPAIControllerBase.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "MotherAIController.generated.h"

class AMotherAIActor;

/**
 * Mother AI Controller - Controls Mother AI behavior using Behavior Tree
 */
UCLASS()
class BLASTER_API AMotherAIController : public APPAIControllerBase
{
	GENERATED_BODY()

public:
	AMotherAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

	/**
	 * Get Mother AI Actor
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mother AI Controller")
	AMotherAIActor* GetMotherAI() const;

	/**
	 * Initialize Behavior Tree
	 */
	UFUNCTION(BlueprintCallable, Category = "Mother AI Controller")
	void InitializeBehaviorTree();

protected:
	/** Behavior Tree Asset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	UBehaviorTree* BehaviorTreeAsset;

	/** Blackboard Asset (BT 에셋과 동일한 BB를 가리키는 것이 일반적) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	UBlackboardData* BlackboardAsset;

private:
	/** Cached Mother AI reference */
	UPROPERTY()
	AMotherAIActor* MotherAI;
};
