// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_MotherExecutePunishment.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "Blaster/AI/MotherAIActor.h"
#include "Blaster/AI/MotherAIController.h"
#include "GameFramework/Character.h"

UBTTask_MotherExecutePunishment::UBTTask_MotherExecutePunishment()
{
	NodeName = TEXT("Mother Execute Punishment");
}

EBTNodeResult::Type UBTTask_MotherExecutePunishment::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
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

	// Get target player from blackboard
	ACharacter* TargetPlayer = Cast<ACharacter>(BlackboardComp->GetValueAsObject(TargetPlayerKey.SelectedKeyName));
	if (!TargetPlayer)
	{
		return EBTNodeResult::Failed;
	}

	// Execute punishment (이 함수가 Blackboard를 업데이트함)
	MotherAI->ExecutePunishment(TargetPlayer);

	// Blackboard에서 타겟 제거
	BlackboardComp->SetValueAsObject(TargetPlayerKey.SelectedKeyName, nullptr);

	return EBTNodeResult::Succeeded;
}

