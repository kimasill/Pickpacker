// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_MotherWaitAtLocation.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "AI/MotherAIActor.h"
#include "AI/MotherAIController.h"

UBTTask_MotherWaitAtLocation::UBTTask_MotherWaitAtLocation()
{
	NodeName = TEXT("Mother Wait At Location");
	bCreateNodeInstance = true;
	ElapsedTime = 0.0f;
}

EBTNodeResult::Type UBTTask_MotherWaitAtLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ElapsedTime = 0.0f;
	
	// Get wait duration from MotherAI if needed
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController)
	{
		if (AMotherAIActor* MotherAI = Cast<AMotherAIActor>(AIController->GetPawn()))
		{
			// Use MotherAI's InspectionDuration if this is for inspection
			if (MotherAI->GetAIState() == EMotherAIState::Inspecting)
			{
				WaitDuration = MotherAI->GetInspectionDuration();
			}
		}
	}
	
	return EBTNodeResult::InProgress;
}

void UBTTask_MotherWaitAtLocation::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	ElapsedTime += DeltaSeconds;

	if (ElapsedTime >= WaitDuration)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

