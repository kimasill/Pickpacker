// Fill out your copyright notice in the Description page of Project Settings.

#include "MotherAIController.h"
#include "Blaster/AI/MotherAIActor.h"

AMotherAIController::AMotherAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AMotherAIController::BeginPlay()
{
	Super::BeginPlay();
}

void AMotherAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	MotherAI = Cast<AMotherAIActor>(InPawn);
	if (MotherAI)
	{
		InitializeBehaviorTree();
	}
}

AMotherAIActor* AMotherAIController::GetMotherAI() const
{
	return MotherAI;
}

void AMotherAIController::InitializeBehaviorTree()
{
	if (!BlackboardAsset || !BehaviorTreeAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MotherAIController] Blackboard or Behavior Tree asset not set - check BP_MotherAI defaults"));
		return;
	}

	if (RunBehaviorTreeWithBlackboard(BehaviorTreeAsset, BlackboardAsset))
	{
		UE_LOG(LogTemp, Log, TEXT("[MotherAIController] Behavior Tree initialized"));
	}
}
