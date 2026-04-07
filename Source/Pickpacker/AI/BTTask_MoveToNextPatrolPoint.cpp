// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_MoveToNextPatrolPoint.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "AI/PPPatrolRouteComponent.h"
#include "AI/PatrolPointActor.h"

UBTTask_MoveToNextPatrolPoint::UBTTask_MoveToNextPatrolPoint()
{
	NodeName = TEXT("Move To Next Patrol Point");
}

EBTNodeResult::Type UBTTask_MoveToNextPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	UPPPatrolRouteComponent* PatrolRoute = UPPPatrolRouteComponent::FindPatrolRoute(Pawn);
	if (!PatrolRoute || PatrolRoute->PatrolPoints.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_MoveToNextPatrolPoint] No UPPPatrolRouteComponent or empty patrol route"));
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	PatrolRoute->MoveToNextPatrolPoint();

	APatrolPointActor* NewPoint = PatrolRoute->GetCurrentPatrolPoint();
	if (NewPoint)
	{
		BlackboardComp->SetValueAsObject("CurrentPatrolPoint", NewPoint);
		BlackboardComp->SetValueAsVector("PatrolPointLocation", NewPoint->GetActorLocation());

		UE_LOG(LogTemp, Log, TEXT("[BTTask_MoveToNextPatrolPoint] Moved to next patrol point: %s"), *NewPoint->GetName());
		return EBTNodeResult::Succeeded;
	}

	UE_LOG(LogTemp, Warning, TEXT("[BTTask_MoveToNextPatrolPoint] No patrol points available"));
	return EBTNodeResult::Failed;
}
