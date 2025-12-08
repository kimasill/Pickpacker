// Fill out your copyright notice in the Description page of Project Settings.

#include "BTService_MotherCheckPlayerProximity.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "Blaster/AI/MotherAIActor.h"
#include "Blaster/AI/MotherAIController.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Engine/World.h"

UBTService_MotherCheckPlayerProximity::UBTService_MotherCheckPlayerProximity()
{
	NodeName = TEXT("Check Player Proximity");
	bCreateNodeInstance = true;
	Interval = 0.1f;
	RandomDeviation = 0.0f;
	LastCheckTime = 0.0f;
}

void UBTService_MotherCheckPlayerProximity::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	float CurrentTime = OwnerComp.GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastCheckTime < CheckInterval)
	{
		return;
	}
	LastCheckTime = CurrentTime;

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return;
	}

	AMotherAIActor* MotherAI = Cast<AMotherAIActor>(AIController->GetPawn());
	if (!MotherAI)
	{
		return;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return;
	}

	// Get target player
	ACharacter* TargetPlayer = Cast<ACharacter>(BlackboardComp->GetValueAsObject(FName("TargetPlayer")));
	if (!TargetPlayer)
	{
		// No target player - set Chasing to false
		if (ChasingKey.IsSet())
		{
			BlackboardComp->SetValueAsBool(ChasingKey.SelectedKeyName, false);
		}
		return;
	}

	// Check distance to player
	FVector MotherLocation = MotherAI->GetActorLocation();
	FVector PlayerLocation = TargetPlayer->GetActorLocation();
	float Distance = FVector::Dist(MotherLocation, PlayerLocation);

	// Update Chasing: true if far enough (need to chase), false if close enough (can punish)
	bool bShouldChase = Distance > ProximityDistance;

	if (ChasingKey.IsSet())
	{
		BlackboardComp->SetValueAsBool(ChasingKey.SelectedKeyName, bShouldChase);
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("[BTService_MotherCheckPlayerProximity] Distance to player: %.2f, Chasing: %s"),
		Distance, bShouldChase ? TEXT("True") : TEXT("False"));
}

void UBTService_MotherCheckPlayerProximity::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
	UBlackboardData* BBAsset = GetBlackboardAsset();
	if (BBAsset)
	{
		// Resolve keys against the blackboard asset to ensure SelectedKeyName and type are valid at runtime
		ChasingKey.ResolveSelectedKey(*BBAsset);
	}
}
