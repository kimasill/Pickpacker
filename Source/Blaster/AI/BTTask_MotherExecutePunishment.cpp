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

	// Execute punishment (이 함수가 몽타주를 재생하고 움직임을 중지함)
	// OnPunishmentEnd 노티파이에서 타겟 제거 및 후속 처리가 이루어짐
	MotherAI->ExecutePunishment(TargetPlayer);

	// 타겟 제거는 OnPunishmentEnd에서 처리하므로 여기서는 제거하지 않음
	// Behavior Tree는 처벌 애니메이션이 끝날 때까지 대기해야 함
	// 노티파이로 OnPunishmentEnd가 호출되면 그곳에서 타겟 제거 및 상태 복귀 처리

	return EBTNodeResult::Succeeded;
}

