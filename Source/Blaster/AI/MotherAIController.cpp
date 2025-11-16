// Fill out your copyright notice in the Description page of Project Settings.

#include "MotherAIController.h"
#include "Blaster/AI/MotherAIActor.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

AMotherAIController::AMotherAIController()
{
	PrimaryActorTick.bCanEverTick = false;
	
	// Create Behavior Tree Component
	BehaviorTreeComponent = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComponent"));
	
	// Create Blackboard Component
	BlackboardComponent = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComponent"));
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

void AMotherAIController::OnUnPossess()
{
	if (BehaviorTreeComponent)
	{
		BehaviorTreeComponent->StopTree();
	}

	Super::OnUnPossess();
}

AMotherAIActor* AMotherAIController::GetMotherAI() const
{
	return MotherAI;
}

void AMotherAIController::InitializeBehaviorTree()
{
	if (!BlackboardAsset || !BehaviorTreeAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MotherAIController] Blackboard or Behavior Tree asset not set"));
		return;
	}

	// Initialize Blackboard
	if (UseBlackboard(BlackboardAsset, BlackboardComponent))
	{
		// Run Behavior Tree
		if (BehaviorTreeComponent)
		{
			RunBehaviorTree(BehaviorTreeAsset);
			UE_LOG(LogTemp, Log, TEXT("[MotherAIController] Behavior Tree initialized"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[MotherAIController] Failed to initialize Blackboard"));
	}
}

