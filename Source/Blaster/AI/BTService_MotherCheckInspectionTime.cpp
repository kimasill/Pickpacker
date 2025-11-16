// Fill out your copyright notice in the Description page of Project Settings.

#include "BTService_MotherCheckInspectionTime.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "Blaster/AI/MotherAIActor.h"
#include "Blaster/AI/MotherAIController.h"

UBTService_MotherCheckInspectionTime::UBTService_MotherCheckInspectionTime()
{
	NodeName = TEXT("Check Inspection Time");
	bCreateNodeInstance = true;
	Interval = 1.0f; // Check every second
	RandomDeviation = 0.0f;
}

void UBTService_MotherCheckInspectionTime::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

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

	// Check if inspection timer has fired (handled by MotherAI)
	// This service just checks if we should start inspection based on state
	bool bShouldInspect = (MotherAI->GetAIState() == EMotherAIState::Inspecting);
	
	BlackboardComp->SetValueAsBool(ShouldInspectKey.SelectedKeyName, bShouldInspect);

	// If inspecting, set inspection location
	if (bShouldInspect)
	{
		// Get current inspection location from MotherAI
		FVector InspectionLocation = MotherAI->GetCurrentInspectionLocation();
		if (!InspectionLocation.IsNearlyZero())
		{
			BlackboardComp->SetValueAsVector(InspectionLocationKey.SelectedKeyName, InspectionLocation);
		}
	}
}

