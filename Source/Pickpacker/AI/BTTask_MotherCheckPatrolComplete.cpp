// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_MotherCheckPatrolComplete.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "AI/MotherAIActor.h"
#include "AI/MotherAIController.h"
#include "Engine/World.h"

UBTTask_MotherCheckPatrolComplete::UBTTask_MotherCheckPatrolComplete()
{
	NodeName = TEXT("Check Patrol Complete");
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_MotherCheckPatrolComplete::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FMotherCheckPatrolCompleteMemory* MyMemory = reinterpret_cast<FMotherCheckPatrolCompleteMemory*>(NodeMemory);
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	AMotherAIActor* MotherAI = Cast<AMotherAIActor>(AIController->GetPawn());
	if (!MotherAI)
	{
		return EBTNodeResult::Failed;
	}

	// Initialize memory
	MyMemory->ElapsedTime = 0.0f;
	MyMemory->bIsActive = true;

	UE_LOG(LogTemp, Log, TEXT("[BTTask_MotherCheckPatrolComplete] Starting patrol check - Duration: %.2f, Required Points: %d"), 
		PatrolDuration, RequiredPointCount);

	return EBTNodeResult::InProgress;
}

void UBTTask_MotherCheckPatrolComplete::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FMotherCheckPatrolCompleteMemory* MyMemory = reinterpret_cast<FMotherCheckPatrolCompleteMemory*>(NodeMemory);
	
	if (!MyMemory->bIsActive)
	{
		return;
	}

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	AMotherAIActor* MotherAI = Cast<AMotherAIActor>(AIController->GetPawn());
	if (!MotherAI)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	MyMemory->ElapsedTime += DeltaSeconds;

	// Check if patrol duration exceeded (if using duration-based completion)
	if (PatrolDuration > 0.0f && MyMemory->ElapsedTime >= PatrolDuration)
	{
		UE_LOG(LogTemp, Log, TEXT("[BTTask_MotherCheckPatrolComplete] Patrol duration exceeded: %.2f seconds"), 
			MyMemory->ElapsedTime);
		MyMemory->bIsActive = false;
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	// Check if required point count reached (if using point-based completion)
	if (RequiredPointCount > 0)
	{
		if (!VisitedPointCountKey.IsSet())
		{
			UE_LOG(LogTemp, Warning, TEXT("[BTTask_MotherCheckPatrolComplete] RequiredPointCount > 0 but VisitedPointCountKey is not set"));
			MyMemory->bIsActive = false;
			FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
			return;
		}

		// Get current count from blackboard (updated by BTTask_MotherIncrementPatrolCount)
		int32 CurrentCount = BlackboardComp->GetValueAsInt(VisitedPointCountKey.SelectedKeyName);

		// Check if we've visited enough points
		if (CurrentCount >= RequiredPointCount)
		{
			UE_LOG(LogTemp, Log, TEXT("[BTTask_MotherCheckPatrolComplete] Required point count reached: %d/%d"), 
				CurrentCount, RequiredPointCount);
			MyMemory->bIsActive = false;
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
			return;
		}
	}

	// Continue patrolling
	return;
}
