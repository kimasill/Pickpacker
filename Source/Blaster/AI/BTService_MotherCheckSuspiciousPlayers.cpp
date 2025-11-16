// Fill out your copyright notice in the Description page of Project Settings.

#include "BTService_MotherCheckSuspiciousPlayers.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "Blaster/AI/MotherAIActor.h"
#include "Blaster/AI/MotherAIController.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/Subsystem/SuspicionManagerSubsystem.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Blaster/PickpackerTypes/PickpackerTypes.h"

UBTService_MotherCheckSuspiciousPlayers::UBTService_MotherCheckSuspiciousPlayers()
{
	NodeName = TEXT("Check Suspicious Players");
	bCreateNodeInstance = true;
	Interval = 0.5f;
	RandomDeviation = 0.0f;
	LastCheckTime = 0.0f;
}

void UBTService_MotherCheckSuspiciousPlayers::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
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

	// Check for suspicious players
	if (UGameInstance* GameInstance = OwnerComp.GetWorld()->GetGameInstance())
	{
		if (USuspicionManagerSubsystem* SuspicionManager = GameInstance->GetSubsystem<USuspicionManagerSubsystem>())
		{
			// Find nearby players
			TArray<AActor*> OverlappingActors;
			UKismetSystemLibrary::SphereOverlapActors(
				OwnerComp.GetWorld(),
				MotherAI->GetActorLocation(),
				DetectionRange,
				TArray<TEnumAsByte<EObjectTypeQuery>>(),
				ABlasterCharacter::StaticClass(),
				TArray<AActor*>(),
				OverlappingActors
			);

			ACharacter* FoundSuspiciousPlayer = nullptr;

			for (AActor* Actor : OverlappingActors)
			{
				ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Actor);
				if (!BlasterCharacter)
				{
					continue;
				}

				// Check if Mother AI can see player
				if (!MotherAI->CanSeePlayer(BlasterCharacter))
				{
					continue;
				}

				// Check suspicious behavior
				ESuspiciousBehavior Behavior = SuspicionManager->GetPlayerSuspiciousBehavior(BlasterCharacter);
				if (Behavior != ESuspiciousBehavior::None)
				{
					FoundSuspiciousPlayer = BlasterCharacter;
					break;
				}
			}

			// Update blackboard
			BlackboardComp->SetValueAsObject(TargetPlayerKey.SelectedKeyName, FoundSuspiciousPlayer);
		}
	}
}

