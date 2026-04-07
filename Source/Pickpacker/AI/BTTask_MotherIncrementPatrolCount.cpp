// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_MotherIncrementPatrolCount.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "AI/MotherAIActor.h"
#include "AI/MotherAIController.h"

UBTTask_MotherIncrementPatrolCount::UBTTask_MotherIncrementPatrolCount()
{
	NodeName = TEXT("Increment Patrol Count");
	bCreateNodeInstance = false; // No need for instance memory
}

EBTNodeResult::Type UBTTask_MotherIncrementPatrolCount::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	if (!VisitedPointCountKey.IsSet())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_MotherIncrementPatrolCount] VisitedPointCountKey is not set"));
		return EBTNodeResult::Failed;
	}

	// Get current count and increment
	int32 CurrentCount = BlackboardComp->GetValueAsInt(VisitedPointCountKey.SelectedKeyName);
	int32 NewCount = CurrentCount + 1;
	BlackboardComp->SetValueAsInt(VisitedPointCountKey.SelectedKeyName, NewCount);

	UE_LOG(LogTemp, Log, TEXT("[BTTask_MotherIncrementPatrolCount] Incremented patrol count: %d -> %d"), 
		CurrentCount, NewCount);

	return EBTNodeResult::Succeeded;
}
