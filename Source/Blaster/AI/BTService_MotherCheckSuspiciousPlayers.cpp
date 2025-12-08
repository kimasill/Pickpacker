// Fill out your copyright notice in the Description page of Project Settings.

#include "BTService_MotherCheckSuspiciousPlayers.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "AIController.h"
#include "Blaster/AI/MotherAIActor.h"
#include "Blaster/AI/MotherAIController.h"
#include "Blaster/AI/DroneActor.h"
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

    // Constrain selectors to expected types. These are editor-time filters; runtime resolution is done in InitializeFromAsset.
    TargetPlayerKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_MotherCheckSuspiciousPlayers, TargetPlayerKey), ACharacter::StaticClass());
    CanSeeTargetKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_MotherCheckSuspiciousPlayers, CanSeeTargetKey));
    ChasingKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_MotherCheckSuspiciousPlayers, ChasingKey));
}

void UBTService_MotherCheckSuspiciousPlayers::InitializeFromAsset(UBehaviorTree& Asset)
{
    Super::InitializeFromAsset(Asset);

    UBlackboardData* BBAsset = GetBlackboardAsset();
    if (BBAsset)
    {
        // Resolve keys against the blackboard asset to ensure SelectedKeyName and type are valid at runtime
        TargetPlayerKey.ResolveSelectedKey(*BBAsset);
        CanSeeTargetKey.ResolveSelectedKey(*BBAsset);
        ChasingKey.ResolveSelectedKey(*BBAsset);
    }
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

    // Track visibility and location updates for the current TargetPlayer
    ACharacter* TargetPlayer = nullptr;
    if (TargetPlayerKey.IsSet())
    {
        TargetPlayer = Cast<ACharacter>(BlackboardComp->GetValueAsObject(TargetPlayerKey.SelectedKeyName));
    }
    else
    {
        // Fallback to literal name if not bound (avoids silent failure during debugging)
        TargetPlayer = Cast<ACharacter>(BlackboardComp->GetValueAsObject(FName("TargetPlayer")));
    }

    bool bCanSeeTarget = false;
    FVector DetectedPlayerLocation = FVector::ZeroVector;

    if (TargetPlayer)
    {
        // Check Mother's DetectedPlayers list
        {
            const TArray<ACharacter*> MotherDetectedPlayers = MotherAI->GetDetectedPlayers();
            for (ACharacter* Detected : MotherDetectedPlayers)
            {
                if (Detected == TargetPlayer)
                {
                    bCanSeeTarget = true;
                    DetectedPlayerLocation = Detected->GetActorLocation();
                    break;
                }
            }
        }

        // Check all drones' DetectedPlayers lists
        if (!bCanSeeTarget)
        {
            const TArray<ADroneActor*>& ActiveDrones = MotherAI->GetActiveDrones();
            for (ADroneActor* Drone : ActiveDrones)
            {
                if (!Drone) continue;
                const TArray<ACharacter*>& DroneDetectedPlayers = Drone->GetDetectedPlayers();
                for (ACharacter* Detected : DroneDetectedPlayers)
                {
                    if (Detected == TargetPlayer)
                    {
                        bCanSeeTarget = true;
                        DetectedPlayerLocation = Detected->GetActorLocation();
                        break;
                    }
                }
                if (bCanSeeTarget) break;
            }
        }

        // Direct visibility check as a final fallback (line of sight + angle + crouch filter)
        if (!bCanSeeTarget && MotherAI->CanSeePlayer(TargetPlayer))
        {
            bCanSeeTarget = true;
            DetectedPlayerLocation = TargetPlayer->GetActorLocation();
        }

        // Only write if the key resolved from the BB asset
        if (CanSeeTargetKey.IsSet())
        {
            BlackboardComp->SetValueAsBool(CanSeeTargetKey.SelectedKeyName, bCanSeeTarget);
        }

        // Update TargetLocation if player is detected
        if (bCanSeeTarget)
        {
            BlackboardComp->SetValueAsVector(FName("TargetLocation"), DetectedPlayerLocation);
            // Switch to chasing state when target is visible
            if (ChasingKey.IsSet())
            {
                BlackboardComp->SetValueAsBool(ChasingKey.SelectedKeyName, true);
            }
            MotherAI->SetAIState(EMotherAIState::ChasingPlayer);

            UE_LOG(LogTemp, VeryVerbose, TEXT("[BTService_MotherCheckSuspiciousPlayers] Target player detected, updating location: %s"), 
                *DetectedPlayerLocation.ToString());
        }
    }
    else
    {
        // No target player - clear CanSeeTarget and Chasing
        if (CanSeeTargetKey.IsSet())
        {
            BlackboardComp->SetValueAsBool(CanSeeTargetKey.SelectedKeyName, false);
        }
        if (ChasingKey.IsSet())
        {
            BlackboardComp->SetValueAsBool(ChasingKey.SelectedKeyName, false);
        }
    }
}

