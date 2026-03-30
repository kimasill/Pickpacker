// Fill out your copyright notice in the Description page of Project Settings.

#include "PPAIControllerBase.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Navigation/CrowdFollowingComponent.h"

APPAIControllerBase::APPAIControllerBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{
	PrimaryActorTick.bCanEverTick = false;
}

void APPAIControllerBase::OnUnPossess()
{
	StopBehaviorTreeIfRunning();
	Super::OnUnPossess();
}

bool APPAIControllerBase::RunBehaviorTreeWithBlackboard(UBehaviorTree* BTAsset, UBlackboardData* BBOverride)
{
	if (!BTAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PPAIControllerBase] RunBehaviorTreeWithBlackboard: BehaviorTree is null"));
		return false;
	}

	UBlackboardData* BBToUse = BBOverride ? BBOverride : BTAsset->BlackboardAsset.Get();
	if (!BBToUse)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PPAIControllerBase] No BlackboardData (override or from BehaviorTree)"));
		return false;
	}

	UBlackboardComponent* BlackboardComp = nullptr;
	if (!UseBlackboard(BBToUse, BlackboardComp))
	{
		UE_LOG(LogTemp, Error, TEXT("[PPAIControllerBase] UseBlackboard failed"));
		return false;
	}

	const bool bStarted = RunBehaviorTree(BTAsset);
	bPPBehaviorTreeRunning = bStarted;
	if (!bStarted)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PPAIControllerBase] RunBehaviorTree returned false"));
	}
	return bStarted;
}

void APPAIControllerBase::StopBehaviorTreeIfRunning()
{
	if (UBehaviorTreeComponent* BTC = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
	{
		BTC->StopTree();
	}
	bPPBehaviorTreeRunning = false;
}
