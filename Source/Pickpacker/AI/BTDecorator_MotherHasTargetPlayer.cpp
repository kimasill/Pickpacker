// Fill out your copyright notice in the Description page of Project Settings.

#include "BTDecorator_MotherHasTargetPlayer.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "GameFramework/Character.h"

UBTDecorator_MotherHasTargetPlayer::UBTDecorator_MotherHasTargetPlayer()
{
	NodeName = TEXT("Has Target Player");
	bCreateNodeInstance = true;
}

bool UBTDecorator_MotherHasTargetPlayer::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return false;
	}

	ACharacter* TargetPlayer = Cast<ACharacter>(BlackboardComp->GetValueAsObject(TargetPlayerKey.SelectedKeyName));
	return TargetPlayer != nullptr;
}

