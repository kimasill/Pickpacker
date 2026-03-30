// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_MotherGeneratePatrolPoint.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Blaster/AI/MotherAIActor.h"
#include "Blaster/AI/PPPatrolBoundsLibrary.h"

UBTTask_MotherGeneratePatrolPoint::UBTTask_MotherGeneratePatrolPoint()
{
	NodeName = TEXT("Generate Patrol Point");
	bCreateNodeInstance = false;
}

EBTNodeResult::Type UBTTask_MotherGeneratePatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
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

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	AActor* InspectionActor = MotherAI->GetCurrentInspectionActor();
	if (!InspectionActor && InspectionActorKey.IsSet())
	{
		InspectionActor = Cast<AActor>(BlackboardComp->GetValueAsObject(InspectionActorKey.SelectedKeyName));
	}

	if (!InspectionActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_MotherGeneratePatrolPoint] No inspection actor available"));
		return EBTNodeResult::Failed;
	}

	FVector VolumeCenter;
	FVector VolumeExtent;
	if (!PPPatrolBoundsLibrary::GetVolumeBoundsFromActor(InspectionActor, VolumeCenter, VolumeExtent))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_MotherGeneratePatrolPoint] Failed to get volume bounds"));
		return EBTNodeResult::Failed;
	}

	const FVector PatrolPoint = PPPatrolBoundsLibrary::GenerateRandomPatrolPointHorizontal(
		VolumeCenter, VolumeExtent, MinPatrolRadius, MaxPatrolRadius);

	BlackboardComp->SetValueAsVector(PatrolTargetLocationKey.SelectedKeyName, PatrolPoint);

	UE_LOG(LogTemp, Log, TEXT("[BTTask_MotherGeneratePatrolPoint] Generated patrol point: %s"), *PatrolPoint.ToString());

	return EBTNodeResult::Succeeded;
}
