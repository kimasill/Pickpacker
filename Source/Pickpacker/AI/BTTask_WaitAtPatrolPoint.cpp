// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_WaitAtPatrolPoint.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "AI/PPPatrolRouteComponent.h"

UBTTask_WaitAtPatrolPoint::UBTTask_WaitAtPatrolPoint()
{
	NodeName = TEXT("Wait At Patrol Point");
	bCreateNodeInstance = true;
	ElapsedTime = 0.0f;
}

EBTNodeResult::Type UBTTask_WaitAtPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
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
	if (!PatrolRoute)
	{
		return EBTNodeResult::Failed;
	}

	float ActualWaitTime = WaitTime;
	if (bUsePatrolPointWaitTime)
	{
		ActualWaitTime = PatrolRoute->GetCurrentPatrolPointWaitTime();
	}

	if (ActualWaitTime <= 0.0f)
	{
		return EBTNodeResult::Succeeded;
	}

	ElapsedTime = 0.0f;

	UE_LOG(LogTemp, Log, TEXT("[BTTask_WaitAtPatrolPoint] Waiting for %.2f seconds"), ActualWaitTime);

	return EBTNodeResult::InProgress;
}

void UBTTask_WaitAtPatrolPoint::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	ElapsedTime += DeltaSeconds;

	float ActualWaitTime = WaitTime;
	if (bUsePatrolPointWaitTime)
	{
		if (AAIController* AIController = OwnerComp.GetAIOwner())
		{
			if (APawn* Pawn = AIController->GetPawn())
			{
				if (UPPPatrolRouteComponent* PatrolRoute = UPPPatrolRouteComponent::FindPatrolRoute(Pawn))
				{
					ActualWaitTime = PatrolRoute->GetCurrentPatrolPointWaitTime();
				}
			}
		}
	}

	if (ElapsedTime >= ActualWaitTime)
	{
		UE_LOG(LogTemp, Log, TEXT("[BTTask_WaitAtPatrolPoint] Wait completed"));
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}
