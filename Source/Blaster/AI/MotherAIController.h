// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "MotherAIController.generated.h"

class AMotherAIActor;

/**
 * Mother AI Controller - Controls Mother AI behavior using Behavior Tree
 */
UCLASS()
class BLASTER_API AMotherAIController : public AAIController
{
	GENERATED_BODY()

public:
	AMotherAIController();

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

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
	/** Behavior Tree Component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UBehaviorTreeComponent* BehaviorTreeComponent;

	/** Behavior Tree Asset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	UBehaviorTree* BehaviorTreeAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UBlackboardComponent* BlackboardComponent;

	/** Blackboard Asset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	UBlackboardData* BlackboardAsset;

private:
	/** Cached Mother AI reference */
	UPROPERTY()
	AMotherAIActor* MotherAI;
};

