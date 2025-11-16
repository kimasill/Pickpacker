// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_MotherMoveToLocation.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "Blaster/AI/MotherAIActor.h"
#include "Blaster/AI/MotherAIController.h"
#include "GameFramework/Character.h"

UBTTask_MotherMoveToLocation::UBTTask_MotherMoveToLocation()
{
	NodeName = TEXT("Mother Move To Location");
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_MotherMoveToLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FMotherMoveToLocationMemory* MyMemory = reinterpret_cast<FMotherMoveToLocationMemory*>(NodeMemory);
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	// Get target location from blackboard
	FVector TargetLocation = BlackboardComp->GetValueAsVector(TargetLocationKey.SelectedKeyName);
	
	// TargetPlayer가 있으면 플레이어 위치 사용
	if (TargetLocation.IsNearlyZero())
	{
		// Try to get from TargetPlayer if available
		ACharacter* TargetPlayer = Cast<ACharacter>(BlackboardComp->GetValueAsObject(FName("TargetPlayer")));
		if (TargetPlayer)
		{
			TargetLocation = TargetPlayer->GetActorLocation();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[BTTask_MotherMoveToLocation] Target location is zero and no target player"));
			return EBTNodeResult::Failed;
		}
	}

	MyMemory->TargetLocation = TargetLocation;
	MyMemory->bIsActive = true;

	return EBTNodeResult::InProgress;
}

void UBTTask_MotherMoveToLocation::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FMotherMoveToLocationMemory* MyMemory = reinterpret_cast<FMotherMoveToLocationMemory*>(NodeMemory);
	
	if (!MyMemory->bIsActive)
	{
		return;
	}

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	AMotherAIActor* MotherAI = Cast<AMotherAIActor>(AIController->GetPawn());
	if (!MotherAI)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// If TargetPlayer is set, continuously update target location
	ACharacter* TargetPlayer = Cast<ACharacter>(BlackboardComp->GetValueAsObject(FName("TargetPlayer")));
	if (TargetPlayer)
	{
		MyMemory->TargetLocation = TargetPlayer->GetActorLocation();
	}

	FVector CurrentLocation = MotherAI->GetActorLocation();
	float Distance = FVector::Dist(CurrentLocation, MyMemory->TargetLocation);

	// Check if reached
	if (Distance <= AcceptableRadius)
	{
		MyMemory->bIsActive = false;
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	// Move towards target using helper function
	MotherAI->MoveToLocation(MyMemory->TargetLocation, MovementSpeed);
}

EBTNodeResult::Type UBTTask_MotherMoveToLocation::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FMotherMoveToLocationMemory* MyMemory = reinterpret_cast<FMotherMoveToLocationMemory*>(NodeMemory);
	MyMemory->bIsActive = false;

	return EBTNodeResult::Aborted;
}

