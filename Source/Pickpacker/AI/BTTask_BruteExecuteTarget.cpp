// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_BruteExecuteTarget.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "AI/BruteActor.h"
#include "Character/BlasterCharacter.h"

UBTTask_BruteExecuteTarget::UBTTask_BruteExecuteTarget()
{
	NodeName = TEXT("Brute Execute Target");
}

EBTNodeResult::Type UBTTask_BruteExecuteTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	ABruteActor* Brute = Cast<ABruteActor>(AIController->GetPawn());
	if (!Brute)
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB)
	{
		return EBTNodeResult::Failed;
	}

	ABlasterCharacter* Target = Cast<ABlasterCharacter>(BB->GetValueAsObject(TargetPlayerKey.SelectedKeyName));
	if (!Target)
	{
		return EBTNodeResult::Failed;
	}

	Brute->ExecuteTarget(Target);
	return EBTNodeResult::Succeeded;
}
