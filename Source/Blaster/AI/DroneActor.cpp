// Fill out your copyright notice in the Description page of Project Settings.

#include "DroneActor.h"
#include "PatrolPointActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Perception/AIPerceptionComponent.h"
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

    // AI Perception sight
    PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
    SightConfig->SightRadius = DetectionRange;
    SightConfig->LoseSightRadius = DetectionRange * 1.2f;
    SightConfig->PeripheralVisionAngleDegrees = DetectionAngle;
    SightConfig->SetMaxAge(2.0f);
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

    PerceptionComp->ConfigureSense(*SightConfig);
    PerceptionComp->SetDominantSense(UAISense_Sight::StaticClass());

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
        
        // APawn의 AddMovementInput이 작동하려면 MovementComponent가 필요함
        // UFloatingPawnMovement는 이미 생성되었으므로, 직접 사용 가능
    }

    // Get game state
    GameState = GetWorld()->GetGameState<APickpackerGameState>();

    // Setup detection sphere overlap
    if (DetectionSphere)
    {
        DetectionSphere->OnComponentBeginOverlap.AddDynamic(this, &ADroneActor::OnDetectionSphereOverlap);
    }

    // Bind perception updated
    if (PerceptionComp)
    {
        PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &ADroneActor::OnTargetPerceptionUpdated);
    }

    if (HasAuthority())
    {
        InitializeAI();
        
        // SuspicionManager에 구독
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
            // 배터리 레벨
            BlackboardComp->SetValueAsFloat("BatteryLevel", CurrentBatteryLevel);
            BlackboardComp->SetValueAsBool("BatteryLow", IsBatteryLow());
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

    // Update detection timer (Behavior Tree에서도 사용 가능하도록 데이터만 업데이트)
    if (CurrentState == EDroneState::Detecting && DetectedPlayers.Num() > 0 && DetectedPlayers[0].IsValid())
    {
        CurrentDetectionTime += DeltaTime;
    }

    // 시야 안의 플레이어의 의심 행동 상태 지속 체크 (tick에서만 처리)
    CheckVisiblePlayersSuspiciousBehavior(DeltaTime);

    DrawPerceptionDebug();
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
            // 배터리 레벨
            BlackboardComp->SetValueAsFloat("BatteryLevel", CurrentBatteryLevel);
            BlackboardComp->SetValueAsBool("BatteryLow", IsBatteryLow());
            
            // 충전 중
            BlackboardComp->SetValueAsBool("IsCharging", bIsCharging);

            // 무기 사용
            BlackboardComp->SetValueAsBool("ShouldUseWeapon", bUseWeapon);

            // 감지된 플레이어 (첫 번째 플레이어, 호환성 유지)
            if (DetectedPlayers.Num() > 0 && DetectedPlayers[0].IsValid())
            {
                BlackboardComp->SetValueAsObject("DetectedPlayer", DetectedPlayers[0].Get());
            }
            else
            {
                BlackboardComp->ClearValue("DetectedPlayer");
            }

            // 현재 순찰 포인트
            if (PatrolPoints.IsValidIndex(CurrentPatrolIndex) && PatrolPoints[CurrentPatrolIndex])
            {
                APatrolPointActor* CurrentPoint = PatrolPoints[CurrentPatrolIndex];
                BlackboardComp->SetValueAsObject("CurrentPatrolPoint", CurrentPoint);
                // 위치도 Vector로 저장 (Move To Task가 사용)
                BlackboardComp->SetValueAsVector("PatrolPointLocation", CurrentPoint->GetActorLocation());
            }
            else
            {
                BlackboardComp->ClearValue("CurrentPatrolPoint");
                BlackboardComp->ClearValue("PatrolPointLocation");
            }

            // 충전장치
            if (ChargingStation)
            {
                BlackboardComp->SetValueAsObject("ChargingStation", ChargingStation);
            }
        }
    }
}

void ADroneActor::InitializeAI()
{
    // AI Controller 가져오기 또는 생성
    AAIController* AIController = Cast<AAIController>(GetController());

    if (!AIController)
    {
        // Controller가 없으면 생성
        AIController = GetWorld()->SpawnActor<AAIController>(AAIController::StaticClass());
        if (AIController)
        {
            AIController->Possess(this);
        }
    }

    if (AIController && BehaviorTree && BehaviorTree->BlackboardAsset)
    {
        UBlackboardComponent* BlackboardComp = nullptr;
		const bool bBBInit = AIController->UseBlackboard(BehaviorTree->BlackboardAsset, BlackboardComp);

        const bool bBTStarted = AIController->RunBehaviorTree(BehaviorTree);

        if(!bBBInit || !bBTStarted)
        {
            UE_LOG(LogTemp, Warning, TEXT("[DroneActor] Failed to initialize Blackboard or Behavior Tree"));
            return;
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
    CurrentPatrolIndex = 0;
}

void ADroneActor::StopPatrol()
{
    SetDroneState(EDroneState::Returning);
    // Return to spawn location
}


APatrolPointActor* ADroneActor::GetCurrentPatrolPoint() const
{
    if (PatrolPoints.IsValidIndex(CurrentPatrolIndex))
    {
        return PatrolPoints[CurrentPatrolIndex];
    }
    return nullptr;
}

APatrolPointActor* ADroneActor::GetNextPatrolPoint()
{
    if (PatrolPoints.Num() == 0)
    {
        return nullptr;
    }

    // 다음 인덱스 계산 (순환)
    int32 NextIndex = (CurrentPatrolIndex + 1) % PatrolPoints.Num();

    if (PatrolPoints.IsValidIndex(NextIndex))
    {
        return PatrolPoints[NextIndex];
    }

    return nullptr;
}

void ADroneActor::MoveToNextPatrolPoint()
{
    if (PatrolPoints.Num() == 0)
    {
        return;
    }

    // 다음 인덱스로 이동 (순환)
    CurrentPatrolIndex = (CurrentPatrolIndex + 1) % PatrolPoints.Num();

    // Blackboard 업데이트
    UpdateBlackboard();

    UE_LOG(LogTemp, Log, TEXT("[DroneActor] Moved to next patrol point. Index: %d"), CurrentPatrolIndex);
}

bool ADroneActor::HasReachedPatrolPoint(float Tolerance) const
{
    APatrolPointActor* CurrentPoint = GetCurrentPatrolPoint();
    if (!CurrentPoint)
    {
        return false;
    }

    float Distance = FVector::Dist(GetActorLocation(), CurrentPoint->GetActorLocation());
    return Distance <= Tolerance;
}

float ADroneActor::GetCurrentPatrolPointWaitTime() const
{
    APatrolPointActor* CurrentPoint = GetCurrentPatrolPoint();
    if (CurrentPoint)
    {
        return CurrentPoint->WaitTime;
    }
    return 2.0f; // 기본값
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
        // Check line of sight (투시 불가능)
        if (!CanSeePlayer(Character))
        {
            return;
        }

        // Check if player is in detection angle (전방 120도)
        FVector ToPlayer = (Character->GetActorLocation() - GetActorLocation()).GetSafeNormal();
        FVector Forward = GetActorForwardVector();
        float DotProduct = FVector::DotProduct(Forward, ToPlayer);
        float Angle = FMath::RadiansToDegrees(FMath::Acos(DotProduct));
        
        if (Angle > DetectionAngle * 0.5f)
        {
            // Player is outside detection angle
            return;
        }

		UE_LOG(LogTemp, VeryVerbose, TEXT("[DroneActor] Player %s detected within line of sight and angle."), *Character->GetName());
		
		// 플레이어를 감지 목록에 추가 (여러 명 감지 가능)
        if (CurrentState == EDroneState::Patrol || CurrentState == EDroneState::Returning)
        {
            // 이미 목록에 있는지 확인
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
                // 상태는 Patrol 유지 (Detecting으로 변경하지 않음)
                if (DetectedPlayers.Num() == 1)
                {
                    CurrentDetectionTime = 0.0f;
                }
            }
        }
    }
    else
    {
        // Lost sight - 감지 목록에서 제거
        for (int32 i = DetectedPlayers.Num() - 1; i >= 0; --i)
        {
            if (DetectedPlayers[i].Get() == Character)
            {
                DetectedPlayers.RemoveAt(i);
                OnPlayerLost.Broadcast(Character);
                break;
            }
        }
    }
}

void ADroneActor::OnDetectionSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (ACharacter* Character = Cast<ACharacter>(OtherActor))
    {
        // Fallback overlap path: mimic perception
        FAIStimulus DummyStimulus; // not used, we just reuse logic path
        OnTargetPerceptionUpdated(Character, DummyStimulus);
    }
}

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
    // 색상 정의
    const FColor SightColor = FColor::Green;
    const FColor LoseSightColor = FColor::Yellow;
    const FColor FOVEdgeColor = FColor::Cyan;

    const FVector Origin = GetActorLocation();
    const float SightR = SightConfig ? SightConfig->SightRadius : 0.f;
    const float LoseSightR = SightConfig ? SightConfig->LoseSightRadius : 0.f;
    const float HalfAngleDeg = SightConfig ? SightConfig->PeripheralVisionAngleDegrees * 0.5f : 0.f;
    const float HalfAngleRad = FMath::DegreesToRadians(HalfAngleDeg);

    // 기본 반경
    if (SightR > 0.f)
    {
        DrawDebugSphere(GetWorld(), Origin, SightR, 24, SightColor, false, 0.f, 0, 1.f);
    }
    if (LoseSightR > SightR)
    {
        DrawDebugSphere(GetWorld(), Origin, LoseSightR, 24, LoseSightColor, false, 0.f, 0, 0.5f);
    }

    // FOV 시각화: Forward 기준으로 양쪽 경계 벡터
    const FVector Forward = GetActorForwardVector();
    const FVector Right = GetActorRightVector();
    const FVector Up = FVector::UpVector;

    // 오른쪽 경계
    FVector EdgeRight = (Forward.RotateAngleAxis(HalfAngleDeg, Up)).GetSafeNormal();
    // 왼쪽 경계
    FVector EdgeLeft = (Forward.RotateAngleAxis(-HalfAngleDeg, Up)).GetSafeNormal();

    DrawDebugLine(GetWorld(), Origin, Origin + EdgeRight * SightR, FOVEdgeColor, false, 0.f, 0, 1.f);
    DrawDebugLine(GetWorld(), Origin, Origin + EdgeLeft * SightR, FOVEdgeColor, false, 0.f, 0, 1.f);
    DrawDebugLine(GetWorld(), Origin, Origin + Forward * SightR, SightColor, false, 0.f, 0, 1.f);

    // 감지된 플레이어 표시
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


bool ADroneActor::CanSeePlayer(ACharacter* Player) const
{
    if (!Player)
    {
        return false;
    }

    // Line trace to check line of sight (투시 불가능)
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

    // 감지 목록에 추가 (없으면)
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
    
    // 시야 및 각도 확인 (공통 로직은 ProcessPlayerSuspiciousBehavior에서 처리)
    if (!CanSeePlayer(BlasterCharacter))
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("[DroneActor] Suspicion event received but player %s not in line of sight"), 
            *BlasterCharacter->GetName());
        return;
    }

    // Detection angle 확인
    FVector ToPlayer = (BlasterCharacter->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    FVector Forward = GetActorForwardVector();
    float DotProduct = FVector::DotProduct(Forward, ToPlayer);
    float Angle = FMath::RadiansToDegrees(FMath::Acos(DotProduct));
    
    if (Angle > DetectionAngle * 0.5f)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("[DroneActor] Suspicion event received but player %s outside detection angle"), 
            *BlasterCharacter->GetName());
        return;
    }

    // 공통 처리 함수 호출
    ProcessPlayerSuspiciousBehavior(BlasterCharacter, EventData.Behavior);
}

void ADroneActor::CheckVisiblePlayersSuspiciousBehavior(float DeltaTime)
{
    if (!bIsActive || !HasAuthority())
    {
        return;
    }

    // 체크 간격 확인
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastSuspiciousBehaviorCheckTime < SuspiciousBehaviorCheckInterval)
    {
        return;
    }
    LastSuspiciousBehaviorCheckTime = CurrentTime;

    // SuspicionManager에서 플레이어 상태 확인
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GameInstance = World->GetGameInstance())
        {
            if (USuspicionManagerSubsystem* SuspicionManager = GameInstance->GetSubsystem<USuspicionManagerSubsystem>())
            {
                // 시야 안의 모든 플레이어 체크
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

                    // Line of sight 확인
                    if (!CanSeePlayer(BlasterCharacter))
                    {
                        continue;
                    }

                    // Detection angle 확인
                    FVector ToPlayer = (BlasterCharacter->GetActorLocation() - GetActorLocation()).GetSafeNormal();
                    FVector Forward = GetActorForwardVector();
                    float DotProduct = FVector::DotProduct(Forward, ToPlayer);
                    float Angle = FMath::RadiansToDegrees(FMath::Acos(DotProduct));
                    
                    if (Angle > DetectionAngle * 0.5f)
                    {
                        continue;
                    }

                    // SuspicionManager에서 플레이어의 현재 의심 행동 상태 가져오기
                    ESuspiciousBehavior Behavior = SuspicionManager->GetPlayerSuspiciousBehavior(BlasterCharacter);
                    
                    if (Behavior != ESuspiciousBehavior::None)
                    {
                        // 공통 처리 함수 호출
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

    // 시야 및 각도 확인 (이미 확인했지만 안전을 위해 다시 확인)
    if (!CanSeePlayer(BlasterCharacter))
    {
        return;
    }

    FVector ToPlayer = (BlasterCharacter->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    FVector Forward = GetActorForwardVector();
    float DotProduct = FVector::DotProduct(Forward, ToPlayer);
    float Angle = FMath::RadiansToDegrees(FMath::Acos(DotProduct));
    
    if (Angle > DetectionAngle * 0.5f)
    {
        return;
    }

    // 중복 처리 방지: 같은 플레이어의 같은 행동을 짧은 시간 내에 다시 처리하지 않음
    float CurrentTime = GetWorld()->GetTimeSeconds();
    float* LastProcessedTime = LastProcessedSuspicionTime.Find(BlasterCharacter);
    if (LastProcessedTime && (CurrentTime - *LastProcessedTime) < SuspicionProcessCooldown)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("[DroneActor] Duplicate suspicion processing ignored - Player: %s, Behavior: %s (cooldown: %.2f)"), 
            *BlasterCharacter->GetName(), *UEnum::GetValueAsString(Behavior), SuspicionProcessCooldown);
        return;
    }

    // 마지막 처리 시간 업데이트
    LastProcessedSuspicionTime.Add(BlasterCharacter, CurrentTime);

    // 개인별 의심 수치 추가
    ABlasterPlayerState* BlasterPlayerState = BlasterCharacter->GetPlayerState<ABlasterPlayerState>();
    if (BlasterPlayerState)
    {
        BlasterPlayerState->AddPersonalSuspicion(50.0f);
        
        // 경고 출력
        FString BehaviorName = UEnum::GetValueAsString(Behavior);
        FString WarningMessage = FString::Printf(TEXT("경고: %s의 의심스러운 행동이 감지되었습니다! (%s, 의심 수치 +50)"), 
            *BlasterCharacter->GetName(), *BehaviorName);
        UE_LOG(LogTemp, Warning, TEXT("[DroneActor] %s"), *WarningMessage);
        
        // 디버그 로그
        UE_LOG(LogTemp, Log, TEXT("[DroneActor] Suspicion detected - Player: %s, Behavior: %s, Location: %s, Distance: %.2f, Angle: %.2f"), 
            *BlasterCharacter->GetName(), *BehaviorName, 
            *BlasterCharacter->GetActorLocation().ToString(),
            FVector::Dist(GetActorLocation(), BlasterCharacter->GetActorLocation()),
            Angle);
        
        // 화면 디버그 메시지
        FString Msg = FString::Printf(TEXT("Suspicion Event: Player %s, Behavior %s"), 
            *BlasterCharacter->GetName(), *UEnum::GetValueAsString(Behavior));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                -1,
                5.0f,
                FColor::Red,
                Msg
            );
        }
        
        // 마더 AI에게 경고 전달
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

