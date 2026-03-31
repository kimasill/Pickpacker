// Fill out your copyright notice in the Description page of Project Settings.

#include "DroneActor.h"
#include "PatrolPointActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Blaster/AI/UPPSightPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/Character.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/GameState/PickpackerGameState.h"
#include "Blaster/Components/PlayerInventoryComponent.h"
#include "Blaster/Components/InteractionComponent.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/Station/StationActor.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Blaster/AI/MotherAIActor.h"
#include "Blaster/Library/PickpackerSuspicionLibrary.h"
#include "Blaster/Subsystem/SuspicionManagerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "AIController.h"
#include "Blaster/AI/DroneAIController.h"
#include "Blaster/AI/PPAIControllerBase.h"
#include "Blaster/AI/PPPatrolRouteComponent.h"
#include "Blaster/AI/PPBlackboardKeys.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

ADroneActor::ADroneActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);
	RootSphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("RootComponent"));
	RootComponent = RootSphereComponent;

    // Create components
    DroneMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("DroneMesh"));
	DroneMesh->SetupAttachment(RootComponent);
    DroneMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    DroneMesh->SetCollisionObjectType(ECC_Pawn);
    DroneMesh->SetCollisionResponseToAllChannels(ECR_Block);
    DroneMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

    DetectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DetectionSphere"));
    DetectionSphere->SetupAttachment(RootComponent);
    DetectionSphere->SetSphereRadius(DetectionRange);
    DetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DetectionSphere->SetCollisionObjectType(ECC_WorldDynamic);
    DetectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    DetectionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    PerceptionComp = CreateDefaultSubobject<UPPSightPerceptionComponent>(TEXT("Perception"));
    PerceptionComp->ApplySightParameters(DetectionRange, DetectionAngle);

    PatrolRouteComponent = CreateDefaultSubobject<UPPPatrolRouteComponent>(TEXT("PatrolRoute"));

    // Create weapon mesh
    WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
    WeaponMesh->SetupAttachment(DroneMesh, WeaponSocketName);
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Create Floating Pawn Movement and assign to APawn's MovementComponent
    MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingMovementComp"));
    if (MovementComponent)
    {
		MovementComponent->UpdatedComponent = RootComponent;
		MovementComponent->MaxSpeed = PatrolSpeed;
        MovementComponent->Acceleration = 2048.0f;
		MovementComponent->Deceleration = 2048.0f;
		MovementComponent->TurningBoost = 8.0f;
		
		// Assign to APawn's MovementComponent (required for AddMovementInput to work)
		// Note: APawn doesn't have a public MovementComponent, so we need to use the protected one
		// This will be set in BeginPlay
    }
}

void ADroneActor::BeginPlay()
{
    Super::BeginPlay();

    SpawnLocation = GetActorLocation();

    // Ensure MovementComponent is properly set up
    if (MovementComponent)
    {
        MovementComponent->SetUpdatedComponent(RootComponent);
        MovementComponent->MaxSpeed = PatrolSpeed;

        // APawn?? AddMovementInput?? ???????? MovementComponent?? ?????
        // UFloatingPawnMovement?? ??? ????????????, ???? ??? ????
    }

    // Get game state
    GameState = GetWorld()->GetGameState<APickpackerGameState>();

    // Setup detection sphere overlap
    if (DetectionSphere)
    {
        DetectionSphere->OnComponentBeginOverlap.AddDynamic(this, &ADroneActor::OnDetectionSphereOverlap);
        DetectionSphere->OnComponentEndOverlap.AddDynamic(this, &ADroneActor::OnDetectionSphereEndOverlap);
    }

    // Bind perception updated
    if (PerceptionComp)
    {
        PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &ADroneActor::OnTargetPerceptionUpdated);
    }

    if (PatrolRouteComponent)
    {
        PatrolRouteComponent->OnPatrolRouteChanged.AddDynamic(this, &ADroneActor::HandlePatrolRouteChanged);
    }

    if (HasAuthority())
    {
        InitializeAI();
        
        // SuspicionManager????
        if (UWorld* World = GetWorld())
        {
            if (UGameInstance* GameInstance = World->GetGameInstance())
            {
                if (USuspicionManagerSubsystem* SuspicionManager = GameInstance->GetSubsystem<USuspicionManagerSubsystem>())
                {
                    SuspicionManager->SubscribeToSuspicionEvents(this);
                    UE_LOG(LogTemp, Log, TEXT("[DroneActor] Subscribed to SuspicionManager"));
                }
            }
        }
    }
    else
    {
        if (bIsActive)
        {
            StartPatrol();
        }
    }
}

void ADroneActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsActive || !HasAuthority())
    {
        return;
    }

    if (CurrentState != EDroneState::Charging && CurrentState != EDroneState::Returning)
    {
        CurrentBatteryLevel = FMath::Max(0.0f, CurrentBatteryLevel - BatteryConsumptionRate * DeltaTime);
    }
    else if (CurrentState == EDroneState::Charging && bIsCharging)
    {
        CurrentBatteryLevel = FMath::Min(MaxBatteryLevel, CurrentBatteryLevel + BatteryChargingRate * DeltaTime);
    }

    if (AController* DroneController = GetController())
    {
        if (UBlackboardComponent* BlackboardComp = DroneController->FindComponentByClass<UBlackboardComponent>())
        {
            BlackboardComp->SetValueAsFloat(PPBlackboardKeys::BatteryLevel, CurrentBatteryLevel);
            BlackboardComp->SetValueAsBool(PPBlackboardKeys::BatteryLow, IsBatteryLow());
        }
    }

    // Update cooldowns
    if (CurrentAttackCooldown > 0.0f)
    {
        CurrentAttackCooldown -= DeltaTime;
    }

    if (WeaponFireTimer > 0.0f)
    {
        WeaponFireTimer -= DeltaTime;
        if (WeaponFireTimer <= 0.0f)
        {
            bCanFireWeapon = true;
        }
    }

    // Update detection timer
    if (CurrentState == EDroneState::Detecting && DetectedPlayers.Num() > 0 && DetectedPlayers[0].IsValid())
    {
        CurrentDetectionTime += DeltaTime;
    }

    if(OverlappedPlayers.Num() > 0)
    {
       RefreshDetectedPlayers();
	}

    CheckVisiblePlayersSuspiciousBehavior(DeltaTime);

#if !UE_BUILD_SHIPPING
    DrawPerceptionDebug();
#endif
}

void ADroneActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ADroneActor, CurrentState);
    DOREPLIFETIME(ADroneActor, DetectedPlayers);
    DOREPLIFETIME(ADroneActor, bIsActive);
    DOREPLIFETIME(ADroneActor, CurrentBatteryLevel);
    DOREPLIFETIME(ADroneActor, bIsCharging);
}

void ADroneActor::HandlePatrolRouteChanged()
{
    UpdateBlackboard();
}

void ADroneActor::UpdateBlackboard()
{
    if (!BehaviorTree)
    {
        return;
    }
    if (AController* DroneController = GetController())
    {
        if (UBlackboardComponent* BlackboardComp = DroneController->FindComponentByClass<UBlackboardComponent>())
        {
            // ????? ????
            BlackboardComp->SetValueAsFloat(PPBlackboardKeys::BatteryLevel, CurrentBatteryLevel);
            BlackboardComp->SetValueAsBool(PPBlackboardKeys::BatteryLow, IsBatteryLow());

            BlackboardComp->SetValueAsBool(PPBlackboardKeys::IsCharging, bIsCharging);

            BlackboardComp->SetValueAsBool(PPBlackboardKeys::ShouldUseWeapon, bUseWeapon);

            if (CurrentState != EDroneState::Patrol && DetectedPlayers.Num() > 0 && DetectedPlayers[0].IsValid())
            {
                BlackboardComp->SetValueAsObject(PPBlackboardKeys::DetectedPlayer, DetectedPlayers[0].Get());
            }
            else
            {
                BlackboardComp->ClearValue(PPBlackboardKeys::DetectedPlayer);
            }

            if (PatrolRouteComponent)
            {
                if (APatrolPointActor* CurrentPoint = PatrolRouteComponent->GetCurrentPatrolPoint())
                {
                    BlackboardComp->SetValueAsObject(PPBlackboardKeys::CurrentPatrolPoint, CurrentPoint);
                    BlackboardComp->SetValueAsVector(PPBlackboardKeys::PatrolPointLocation, CurrentPoint->GetActorLocation());
                }
                else
                {
                    BlackboardComp->ClearValue(PPBlackboardKeys::CurrentPatrolPoint);
                    BlackboardComp->ClearValue(PPBlackboardKeys::PatrolPointLocation);
                }
            }
            else
            {
                BlackboardComp->ClearValue(PPBlackboardKeys::CurrentPatrolPoint);
                BlackboardComp->ClearValue(PPBlackboardKeys::PatrolPointLocation);
            }

            if (ChargingStation)
            {
                BlackboardComp->SetValueAsObject(PPBlackboardKeys::ChargingStation, ChargingStation);
            }
        }
    }
}

void ADroneActor::InitializeAI()
{
    AAIController* AIController = Cast<AAIController>(GetController());

    if (!AIController)
    {
        AIController = GetWorld()->SpawnActor<ADroneAIController>(ADroneAIController::StaticClass());
        if (AIController)
        {
            AIController->Possess(this);
        }
    }

    if (AIController && BehaviorTree && BehaviorTree->BlackboardAsset)
    {
		if (APPAIControllerBase* PPController = Cast<APPAIControllerBase>(AIController))
		{
			if (!PPController->RunBehaviorTreeWithBlackboard(BehaviorTree, nullptr))
			{
				UE_LOG(LogTemp, Warning, TEXT("[DroneActor] Failed to initialize Blackboard or Behavior Tree"));
				return;
			}
		}
		else
		{
			UBlackboardComponent* BlackboardComp = nullptr;
			const bool bBBInit = AIController->UseBlackboard(BehaviorTree->BlackboardAsset, BlackboardComp);
			const bool bBTStarted = AIController->RunBehaviorTree(BehaviorTree);
			if (!bBBInit || !bBTStarted)
			{
				UE_LOG(LogTemp, Warning, TEXT("[DroneActor] Failed to initialize Blackboard or Behavior Tree"));
				return;
			}
		}

        UpdateBlackboard();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[DroneActor] Failed to initialize AI - Controller: %s, BehaviorTree: %s"),
            AIController ? TEXT("Valid") : TEXT("Invalid"),
            BehaviorTree ? TEXT("Valid") : TEXT("Invalid"));
    }
    if (bIsActive)
    {
        StartPatrol();
    }
}

void ADroneActor::SetDroneState(EDroneState NewState)
{
    if (CurrentState == NewState)
    {
        return;
    }

    EDroneState OldState = CurrentState;
    CurrentState = NewState;

    OnStateChanged.Broadcast(NewState);

    if (HasAuthority())
    {
        OnRep_State(OldState);
    }
}

#pragma region Patrol


void ADroneActor::StartPatrol()
{
    if (!bIsActive)
    {
        return;
    }

    SetDroneState(EDroneState::Patrol);
    if (PatrolRouteComponent)
    {
        PatrolRouteComponent->ResetToStart();
    }
}

void ADroneActor::StopPatrol()
{
    SetDroneState(EDroneState::Returning);
    // Return to spawn location
}


APatrolPointActor* ADroneActor::GetCurrentPatrolPoint() const
{
    return PatrolRouteComponent ? PatrolRouteComponent->GetCurrentPatrolPoint() : nullptr;
}

APatrolPointActor* ADroneActor::GetNextPatrolPoint()
{
    return PatrolRouteComponent ? PatrolRouteComponent->GetNextPatrolPoint() : nullptr;
}

void ADroneActor::MoveToNextPatrolPoint()
{
    if (!PatrolRouteComponent || PatrolRouteComponent->PatrolPoints.Num() == 0)
    {
        return;
    }
    PatrolRouteComponent->MoveToNextPatrolPoint();
    UE_LOG(LogTemp, Log, TEXT("[DroneActor] Moved to next patrol point. Index: %d"),
        PatrolRouteComponent ? PatrolRouteComponent->CurrentPatrolIndex : -1);
}

bool ADroneActor::HasReachedPatrolPoint(float Tolerance) const
{
    return PatrolRouteComponent ? PatrolRouteComponent->HasReachedPatrolPoint(Tolerance) : false;
}

float ADroneActor::GetCurrentPatrolPointWaitTime() const
{
    return PatrolRouteComponent ? PatrolRouteComponent->GetCurrentPatrolPointWaitTime() : 2.0f;
}
#pragma endregion

void ADroneActor::SetActive(bool bActive)
{
    bIsActive = bActive;
    if (!bActive)
    {
        SetDroneState(EDroneState::Returning);
        DetectedPlayers.Empty();
        LastProcessedSuspicionTime.Empty();
    }
}

void ADroneActor::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    if (!bIsActive || !HasAuthority() || !Actor)
    {
        return;
    }

    ACharacter* Character = Cast<ACharacter>(Actor);
    if (!Character)
    {
        return;
    }

    // Only react to player characters
    ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Character);
    if (!BlasterCharacter)
    {
        return;
    }

    if (Stimulus.WasSuccessfullySensed())
    {
        if (!IsCharacterInSight(Character))
        {
            return;
        }

        UE_LOG(LogTemp, VeryVerbose, TEXT("[DroneActor] Player %s detected within line of sight and angle."), *Character->GetName());

        AddDetectedPlayer(Character);
    }
    else
    {
        RemoveDetectedPlayer(Character);
        return;
    }
}

void ADroneActor::OnDetectionSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (ACharacter* Character = Cast<ACharacter>(OtherActor))
    {
		OverlappedPlayers.Add(Character);
        if (IsCharacterInSight(Character))
        {
            AddDetectedPlayer(Character);
        }
        else
        {
            RemoveDetectedPlayer(Character);
        }
    }
}

void ADroneActor::OnDetectionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character)
    {
        return;
    }
	OverlappedPlayers.Remove(Character);

    RemoveDetectedPlayer(Character);
}

void ADroneActor::AddDetectedPlayer(ACharacter* Character)
{
    if (!Character)
    {
        return;
    }

    bool bAlreadyDetected = false;
    for (const TWeakObjectPtr<ACharacter>& Detected : DetectedPlayers)
    {
        if (Detected.Get() == Character)
        {
            bAlreadyDetected = true;
            break;
        }
    }

    if (!bAlreadyDetected)
    {
        DetectedPlayers.Add(Character);
        OnPlayerDetected.Broadcast(Character);
        CurrentDetectionTime = 0.0f;
    }

    // 패트롤 중 단순 발견만으로는 Detecting 전환 안 함 (접근 X). 의심 행동 감지 시 ProcessPlayerSuspiciousBehavior에서 전환

    UpdateBlackboard();
}

bool ADroneActor::CanSeePlayer(ACharacter* Player) const
{
    if (!Player)
    {
        return false;
    }

    // Line trace to check line of sight (???? ?????)
    FVector StartLocation = GetActorLocation();
    FVector EndLocation = Player->GetActorLocation();

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.AddIgnoredActor(Player);
    QueryParams.bTraceComplex = true;

    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        StartLocation,
        EndLocation,
        ECC_Visibility,
        QueryParams
    );

    // If hit something, player is obstructed
    return !bHit;
}

void ADroneActor::RemoveDetectedPlayer(ACharacter* Character)
{
    if (!Character)
    {
        return;
    }

    for (int32 i = DetectedPlayers.Num() - 1; i >= 0; --i)
    {
        if (DetectedPlayers[i].Get() == Character)
        {
            DetectedPlayers.RemoveAt(i);
            OnPlayerLost.Broadcast(Character);
            UpdateBlackboard();
            break;
        }
    }
}

void ADroneActor::RefreshDetectedPlayers()
{
    if (!bIsActive || !HasAuthority() || !DetectionSphere)
    {
        return;
    }

    TArray<AActor*> OverlappingActors;
    DetectionSphere->GetOverlappingActors(OverlappingActors, ACharacter::StaticClass());

    TSet<ACharacter*> VisibleCharacters;
    for (AActor* Actor : OverlappingActors)
    {
        ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Actor);
        if (!BlasterCharacter)
        {
            continue;
        }

        if (IsCharacterInSight(BlasterCharacter))
        {
            VisibleCharacters.Add(BlasterCharacter);
        }
    }

    for (int32 i = DetectedPlayers.Num() - 1; i >= 0; --i)
    {
        ACharacter* Character = DetectedPlayers[i].Get();
        if (!Character)
        {
            DetectedPlayers.RemoveAt(i);
            continue;
        }

        if (!VisibleCharacters.Contains(Character))
        {
            DetectedPlayers.RemoveAt(i);
            OnPlayerLost.Broadcast(Character);
            UpdateBlackboard();
        }
    }

    for (ACharacter* Character : VisibleCharacters)
    {
        AddDetectedPlayer(Character);
    }
}

#pragma region Suspicion

void ADroneActor::AddSuspicion(float Points)
{
    if (GameState)
    {
        GameState->AddTeamSuspicion(Points);
    }
}

void ADroneActor::AttackPlayer(ACharacter* Player)
{
    if (!Player || CurrentAttackCooldown > 0.0f)
    {
        return;
    }

    // Apply damage or penalty
    // For now, just add more suspicion
    AddSuspicion(AttackDamage);

    // Set attack cooldown
    CurrentAttackCooldown = AttackCooldown;

    // Return to chase state
    SetDroneState(EDroneState::Chasing);
}

void ADroneActor::OnAttackCooldownFinished()
{
    // Can attack again
}

void ADroneActor::DrawPerceptionDebug()
{
    // ???? ????
    const FColor SightColor = FColor::Green;
    const FColor LoseSightColor = FColor::Yellow;
    const FColor FOVEdgeColor = FColor::Cyan;

    const FVector Origin = GetActorLocation();
    UAISenseConfig_Sight* SightCfg = PerceptionComp ? PerceptionComp->GetSightConfig() : nullptr;
    const float SightR = SightCfg ? SightCfg->SightRadius : 0.f;
    const float LoseSightR = SightCfg ? SightCfg->LoseSightRadius : 0.f;
    const float HalfAngleDeg = SightCfg ? SightCfg->PeripheralVisionAngleDegrees * 0.5f : 0.f;
    const float HalfAngleRad = FMath::DegreesToRadians(HalfAngleDeg);

    // ?? ???
    if (SightR > 0.f)
    {
        DrawDebugSphere(GetWorld(), Origin, SightR, 24, SightColor, false, 0.f, 0, 1.f);
    }
    if (LoseSightR > SightR)
    {
        DrawDebugSphere(GetWorld(), Origin, LoseSightR, 24, LoseSightColor, false, 0.f, 0, 0.5f);
    }

    // FOV ?�??: Forward ???????? ???? ??? ????
    const FVector Forward = GetActorForwardVector();
    const FVector Right = GetActorRightVector();
    const FVector Up = FVector::UpVector;

    // ?????? ???
    FVector EdgeRight = (Forward.RotateAngleAxis(HalfAngleDeg, Up)).GetSafeNormal();
    // ???? ???
    FVector EdgeLeft = (Forward.RotateAngleAxis(-HalfAngleDeg, Up)).GetSafeNormal();

    DrawDebugLine(GetWorld(), Origin, Origin + EdgeRight * SightR, FOVEdgeColor, false, 0.f, 0, 1.f);
    DrawDebugLine(GetWorld(), Origin, Origin + EdgeLeft * SightR, FOVEdgeColor, false, 0.f, 0, 1.f);
    DrawDebugLine(GetWorld(), Origin, Origin + Forward * SightR, SightColor, false, 0.f, 0, 1.f);

    // ?????? ?�???? ???
    for (const TWeakObjectPtr<ACharacter>& Detected : DetectedPlayers)
    {
        if (Detected.IsValid())
        {
            DrawDebugSphere(GetWorld(), Detected->GetActorLocation(), 32.f, 16, FColor::Red, false, 0.f, 0, 2.f);
            DrawDebugLine(GetWorld(), Origin, Detected->GetActorLocation(), FColor::Red, false, 0.f, 0, 1.5f);
        }
    }
}

void ADroneActor::OnRep_State(EDroneState OldState)
{
    // Replication callback
    OnStateChanged.Broadcast(CurrentState);
}

void ADroneActor::OnRep_BatteryLevel(float OldBatteryLevel)
{
    // Battery level changed
}

bool ADroneActor::IsCharacterInSight(ACharacter* Character) const
{
    if (!Character)
    {
        return false;
    }

    if (!CanSeePlayer(Character))
    {
        return false;
    }

    FVector ToPlayer = (Character->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    FVector Forward = GetActorForwardVector();
    float DotProduct = FVector::DotProduct(Forward, ToPlayer);
    float Angle = FMath::RadiansToDegrees(FMath::Acos(DotProduct));


    return Angle <= DetectionAngle * 0.5f;
}

void ADroneActor::StartCharging()
{
    if (!ChargingStation)
    {
        return;
    }

    bIsCharging = true;
    SetDroneState(EDroneState::Charging);
}

void ADroneActor::StopCharging()
{
    bIsCharging = false;
}

void ADroneActor::FireWeapon(ACharacter* Target)
{
    if (!Target || !bCanFireWeapon || !bUseWeapon)
    {
        return;
    }

    // Check if target is in range
    float Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
    if (Distance > WeaponFireRange)
    {
        return;
    }

    // Apply damage
    if (ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Target))
    {
        // Apply damage through character's damage system
        UGameplayStatics::ApplyDamage(BlasterCharacter, WeaponDamage, GetController(), this, UDamageType::StaticClass());

        // Add suspicion
        AddSuspicion(WeaponDamage);
    }

    // Set fire cooldown
    bCanFireWeapon = false;
    WeaponFireTimer = WeaponFireCooldown;

    // TODO: Play fire animation, sound, muzzle flash, etc.
}

bool ADroneActor::AttachObject(AActor* ObjectToAttach)
{
    if (!ObjectToAttach)
    {
        return false;
    }

    // Detach existing object first
    if (AttachedObject)
    {
        DetachObject();
    }

    // Attach new object
    if (DroneMesh)
    {
        ObjectToAttach->AttachToComponent(DroneMesh, FAttachmentTransformRules::KeepWorldTransform, AttachmentSocketName);
        AttachedObject = ObjectToAttach;
        return true;
    }

    return false;
}

void ADroneActor::DetachObject()
{
    if (AttachedObject)
    {
        AttachedObject->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        AttachedObject = nullptr;
    }
}

void ADroneActor::InteractWithDoor(AActor* Door)
{
    if (!Door)
    {
        return;
    }

    // Check distance
    float Distance = FVector::Dist(GetActorLocation(), Door->GetActorLocation());
    if (Distance > DoorInteractionRange)
    {
        return;
    }

    // Check if door implements interactable interface
    if (Door->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
    {
        // Try to interact with door
        // This would need to be implemented based on your door system
        // For now, we'll just call the interface method
        if (IInteractableInterface* Interface = Cast<IInteractableInterface>(Door))
        {
            // Door should open automatically when drone approaches
            // This is handled by the door's interaction system
        }
    }
}

void ADroneActor::StartChasing(ACharacter* Target)
{
    if (!Target || !HasAuthority())
    {
        return;
    }

    // ???? ???? ??? (??????)
    bool bAlreadyDetected = false;
    for (const TWeakObjectPtr<ACharacter>& Detected : DetectedPlayers)
    {
        if (Detected.Get() == Target)
        {
            bAlreadyDetected = true;
            break;
        }
    }

    if (!bAlreadyDetected)
    {
        DetectedPlayers.Add(Target);
    }

    SetDroneState(EDroneState::Chasing);

    UE_LOG(LogTemp, Warning, TEXT("[DroneActor] Started chasing player: %s"), *Target->GetName());
}

void ADroneActor::OnSuspicionEventReceived(const FSuspicionEventData& EventData)
{
    if (!bIsActive || !HasAuthority() || !EventData.Player)
    {
        return;
    }

    ABlasterCharacter* BlasterCharacter = EventData.Player;


    // ???? �?? ??? ???
    ProcessPlayerSuspiciousBehavior(BlasterCharacter, EventData.Behavior);
}

void ADroneActor::CheckVisiblePlayersSuspiciousBehavior(float DeltaTime)
{
    if (!bIsActive || !HasAuthority())
    {
        return;
    }

    // �? ???? ???
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastSuspiciousBehaviorCheckTime < SuspiciousBehaviorCheckInterval)
    {
        return;
    }
    LastSuspiciousBehaviorCheckTime = CurrentTime;

    // SuspicionManager???? ?�???? ???? ???
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GameInstance = World->GetGameInstance())
        {
            if (USuspicionManagerSubsystem* SuspicionManager = GameInstance->GetSubsystem<USuspicionManagerSubsystem>())
            {
                // ?�? ???? ??? ?�???? �?
                if (!PerceptionComp)
                {
                    return;
                }

                TArray<AActor*> PerceivedActors;
                PerceptionComp->GetKnownPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);

                for (AActor* Actor : PerceivedActors)
                {
                    ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Actor);
                    if (!BlasterCharacter)
                    {
                        continue;
                    }

                    if(!IsCharacterInSight(BlasterCharacter)) {
                        UE_LOG(LogTemp, VeryVerbose, TEXT("[DroneActor] Suspicion check ignored - Player: %s not in line of sight"),
							*BlasterCharacter->GetName());
                        continue;
					}

                    // SuspicionManager???? ?�?????? ???? ??? ?? ???? ????????
                    ESuspiciousBehavior Behavior = SuspicionManager->GetPlayerSuspiciousBehavior(BlasterCharacter);

                    if (Behavior != ESuspiciousBehavior::None)
                    {
                        // ???? �?? ??? ???
                        ProcessPlayerSuspiciousBehavior(BlasterCharacter, Behavior);
                    }
                }
            }
        }
    }
}

void ADroneActor::ProcessPlayerSuspiciousBehavior(ABlasterCharacter* BlasterCharacter, ESuspiciousBehavior Behavior)
{
    if (!BlasterCharacter || Behavior == ESuspiciousBehavior::None || !HasAuthority())
    {
        return;
    }

    // 의심 행동 감지 시 Detecting으로 전환하여 플레이어 접근 시작
    AddDetectedPlayer(BlasterCharacter);
    SetDroneState(EDroneState::Detecting);

    if (!IsCharacterInSight(BlasterCharacter)) {
        UE_LOG(LogTemp, VeryVerbose, TEXT("[DroneActor] Suspicion processing ignored - Player: %s not in line of sight"),
            *BlasterCharacter->GetName());
			return;
    }

    // ??? �?? ????: ???? ?�?????? ???? ???? �?? ?�? ???? ??? �?????? ????
    float CurrentTime = GetWorld()->GetTimeSeconds();
    float* LastProcessedTime = LastProcessedSuspicionTime.Find(BlasterCharacter);
    if (LastProcessedTime && (CurrentTime - *LastProcessedTime) < SuspicionProcessCooldown)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("[DroneActor] Duplicate suspicion processing ignored - Player: %s, Behavior: %s (cooldown: %.2f)"),
            *BlasterCharacter->GetName(), *UEnum::GetValueAsString(Behavior), SuspicionProcessCooldown);
        return;
    }

    // ?????? �?? ?�? ???????
    LastProcessedSuspicionTime.Add(BlasterCharacter, CurrentTime);

    // ????? ??? ??? ???
    ABlasterPlayerState* BlasterPlayerState = BlasterCharacter->GetPlayerState<ABlasterPlayerState>();
    if (BlasterPlayerState)
    {
        BlasterPlayerState->AddPersonalSuspicion(50.0f);

        // ??? ???
        FString BehaviorName = UEnum::GetValueAsString(Behavior);
        FString WarningMessage = FString::Format(
            TEXT("???: {0}?? ???????? ???? ????????????! ({1}, ??? ??? +50)"),
            { BlasterCharacter->GetName(), BehaviorName }
        );
        UE_LOG(LogTemp, Warning, TEXT("[DroneActor] %s"), *WarningMessage);


        // ??? ????? ?????
#if !UE_BUILD_SHIPPING
        FString Msg = FString::Format(
            TEXT("Suspicion Event: Player {0}, Behavior {1}"),
            { BlasterCharacter->GetName(), UEnum::GetValueAsString(Behavior) }
        );
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                -1,
                5.0f,
                FColor::Red,
                Msg
            );
        }
#endif

        // ???? AI???? ??? ????
        if (GameState)
        {
            AMotherAIActor* MotherAI = nullptr;
            for (TActorIterator<AMotherAIActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
            {
                MotherAI = *ActorItr;
                break;
            }
            if (MotherAI)
            {
                MotherAI->SendWarning(WarningMessage);
            }
        }
    }
}

TArray<ACharacter*> ADroneActor::GetDetectedPlayers() const
{
    TArray<ACharacter*> Result;
    for (const TWeakObjectPtr<ACharacter>& Detected : DetectedPlayers)
    {
        if (Detected.IsValid())
        {
            Result.Add(Detected.Get());
        }
    }
    return Result;
}

