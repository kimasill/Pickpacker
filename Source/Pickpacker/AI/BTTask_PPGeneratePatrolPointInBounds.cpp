// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_PPGeneratePatrolPointInBounds.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "AI/PPPatrolBoundsLibrary.h"

UBTTask_PPGeneratePatrolPointInBounds::UBTTask_PPGeneratePatrolPointInBounds()
{
	NodeName = TEXT("PP Generate Patrol Point In Bounds");
	bCreateNodeInstance = false;
}

EBTNodeResult::Type UBTTask_PPGeneratePatrolPointInBounds::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	if (!VolumeActorKey.IsSet())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_PPGeneratePatrolPointInBounds] VolumeActorKey not set"));
		return EBTNodeResult::Failed;
	}

	AActor* VolumeActor = Cast<AActor>(BlackboardComp->GetValueAsObject(VolumeActorKey.SelectedKeyName));
	if (!VolumeActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_PPGeneratePatrolPointInBounds] No volume actor on blackboard"));
		return EBTNodeResult::Failed;
	}

	FVector VolumeCenter;
	FVector VolumeExtent;
	if (!PPPatrolBoundsLibrary::GetVolumeBoundsFromActor(VolumeActor, VolumeCenter, VolumeExtent))
	{
		return EBTNodeResult::Failed;
	}

	const FVector PatrolPoint = PPPatrolBoundsLibrary::GenerateRandomPatrolPointHorizontal(
		VolumeCenter, VolumeExtent, MinPatrolRadius, MaxPatrolRadius);

	BlackboardComp->SetValueAsVector(PatrolTargetLocationKey.SelectedKeyName, PatrolPoint);
	return EBTNodeResult::Succeeded;
}
