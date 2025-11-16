// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_GetCurrentPatrolPoint.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Blaster/AI/DroneActor.h"
#include "Blaster/AI/PatrolPointActor.h"

UBTTask_GetCurrentPatrolPoint::UBTTask_GetCurrentPatrolPoint()
{
	NodeName = TEXT("Get Current Patrol Point");
}

EBTNodeResult::Type UBTTask_GetCurrentPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
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

	ADroneActor* Drone = Cast<ADroneActor>(Pawn);
	if (!Drone)
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	// Get current patrol point
	APatrolPointActor* CurrentPoint = Drone->GetCurrentPatrolPoint();
	
	if (!CurrentPoint)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_GetCurrentPatrolPoint] No current patrol point"));
		BlackboardComp->ClearValue("CurrentPatrolPoint");
		BlackboardComp->ClearValue("PatrolPointLocation");
		return EBTNodeResult::Failed;
	}

	// Set in blackboard
	BlackboardComp->SetValueAsObject("CurrentPatrolPoint", CurrentPoint);
	BlackboardComp->SetValueAsVector("PatrolPointLocation", CurrentPoint->GetActorLocation());

	UE_LOG(LogTemp, Log, TEXT("[BTTask_GetCurrentPatrolPoint] Set patrol point: %s at location: %s"), 
		*CurrentPoint->GetName(), *CurrentPoint->GetActorLocation().ToString());

	return EBTNodeResult::Succeeded;
}




