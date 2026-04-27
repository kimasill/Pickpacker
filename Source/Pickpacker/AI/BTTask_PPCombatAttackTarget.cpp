// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_PPCombatAttackTarget.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "AI/PPBlackboardKeys.h"
#include "Components/NPCCombatComponent.h"
#include "NPC/ModularNPCActor.h"

UBTTask_PPCombatAttackTarget::UBTTask_PPCombatAttackTarget()
{
	NodeName = TEXT("PP Combat Attack Target");
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_PPCombatAttackTarget, TargetActorKey), AActor::StaticClass());
	TargetActorKey.SelectedKeyName = PPBlackboardKeys::TargetActor;
}

EBTNodeResult::Type UBTTask_PPCombatAttackTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	APawn* ControlledPawn = AIController->GetPawn();
	if (!ControlledPawn)
	{
		return EBTNodeResult::Failed;
	}

	UNPCCombatComponent* CombatComponent = nullptr;
	if (AModularNPCActor* ModularNPC = Cast<AModularNPCActor>(ControlledPawn))
	{
		CombatComponent = ModularNPC->CombatComponent;
	}
	else
	{
		CombatComponent = ControlledPawn->FindComponentByClass<UNPCCombatComponent>();
	}

	if (!CombatComponent)
	{
		return EBTNodeResult::Failed;
	}

	if (UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent())
	{
		if (AActor* TargetActor = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetActorKey.SelectedKeyName)))
		{
			CombatComponent->SetTarget(TargetActor);
		}
	}

	return CombatComponent->TryAttack() ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}
