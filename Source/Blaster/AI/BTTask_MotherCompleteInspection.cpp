// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_MotherCompleteInspection.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "Blaster/AI/MotherAIActor.h"
#include "Blaster/AI/MotherAIController.h"

UBTTask_MotherCompleteInspection::UBTTask_MotherCompleteInspection()
{
	NodeName = TEXT("Mother Complete Inspection");
}

EBTNodeResult::Type UBTTask_MotherCompleteInspection::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
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

	// Complete inspection (이 함수가 Blackboard를 업데이트함)
	MotherAI->CompleteInspection();

	return EBTNodeResult::Succeeded;
}

