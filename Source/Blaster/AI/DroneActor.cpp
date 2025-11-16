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
    if (CurrentState == EDroneState::Detecting && DetectedPlayer.IsValid())
    {
        CurrentDetectionTime += DeltaTime;
    }

    // 시야 안의 플레이어의 의심 행동 상태 지속 체크
    CheckVisiblePlayersSuspiciousBehavior(DeltaTime);

    DrawPerceptionDebug();
}

void ADroneActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ADroneActor, CurrentState);
    DOREPLIFETIME(ADroneActor, DetectedPlayer);
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

            // 감지된 플레이어
            if (DetectedPlayer.IsValid())
            {
                BlackboardComp->SetValueAsObject("DetectedPlayer", DetectedPlayer.Get());
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
        DetectedPlayer = nullptr;
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
		
		// 플레이어를 감지 목록에 추가 (Tick에서 지속적으로 체크)
        if (CurrentState == EDroneState::Patrol || CurrentState == EDroneState::Returning)
        {
            DetectedPlayer = Character;
            // 상태는 Patrol 유지 (Detecting으로 변경하지 않음)
            CurrentDetectionTime = 0.0f;
        }
    }
    else
    {
        // Lost sight - Patrol 상태 유지
        if (DetectedPlayer.Get() == Character)
        {
            DetectedPlayer = nullptr;
            // 상태는 Patrol 유지 (이미 Patrol 상태이므로 변경 불필요)
            OnPlayerLost.Broadcast(nullptr);
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

bool ADroneActor::IsPlayerSuspicious(ACharacter* Player) const
{
    if (!Player)
    {
        return false;
    }

    ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Player);
    if (!BlasterCharacter)
    {
        return false;
    }

    // Check if player has items in inventory (suspicious)
    if (UPlayerInventoryComponent* InventoryComponent = BlasterCharacter->GetPlayerInventoryComponent())
    {
        if (InventoryComponent->GetInventoryCount() > 0)
        {
            return true;
        }
    }

    // Check if player is carrying a parcel (suspicious if it's an item)
    if (UInteractionComponent* InteractionComponent = BlasterCharacter->GetInteractionComponent())
    {
        if (AParcelActor* CarriedParcel = InteractionComponent->GetCarriedParcel())
        {
            if (CarriedParcel->IsItem())
            {
                return true;
            }
        }
    }

    // Add more suspicion checks here (running, hiding, etc.)

    return false;
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
    if (DetectedPlayer.IsValid())
    {
        DrawDebugSphere(GetWorld(), DetectedPlayer->GetActorLocation(), 32.f, 16, FColor::Red, false, 0.f, 0, 2.f);
        DrawDebugLine(GetWorld(), Origin, DetectedPlayer->GetActorLocation(), FColor::Red, false, 0.f, 0, 1.5f);
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

    DetectedPlayer = Target;
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
    
    // 1) 현재 플레이어가 내 시야에 있는가?
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
    FString Msg = FString::Printf(TEXT("Suspicion Event: Player %s, Behavior %s"), 
		*BlasterCharacter->GetName(), *UEnum::GetValueAsString(EventData.Behavior));
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            5.0f,
            FColor::Red,
            Msg
        );
    }
    // 2) YES → 의심 행동 즉시 감지
    UE_LOG(LogTemp, Warning, TEXT("[DroneActor] Suspicion event detected - Player: %s, Behavior: %s, Distance: %.2f"), 
        *BlasterCharacter->GetName(), *UEnum::GetValueAsString(EventData.Behavior),
        FVector::Dist(GetActorLocation(), BlasterCharacter->GetActorLocation()));

    // 개인별 의심 수치 추가
    ABlasterPlayerState* BlasterPlayerState = BlasterCharacter->GetPlayerState<ABlasterPlayerState>();
    if (BlasterPlayerState)
    {
        // 중복 체크 방지: 같은 행동을 연속으로 감지하지 않도록
        static TMap<ABlasterCharacter*, ESuspiciousBehavior> LastDetectedBehavior;
        ESuspiciousBehavior* LastBehavior = LastDetectedBehavior.Find(BlasterCharacter);
        
        if (!LastBehavior || *LastBehavior != EventData.Behavior)
        {
            BlasterPlayerState->AddPersonalSuspicion(50.0f);
            
            // 경고 출력
            FString BehaviorName = UEnum::GetValueAsString(EventData.Behavior);
            FString WarningMessage = FString::Printf(TEXT("경고: %s의 의심스러운 행동이 감지되었습니다! (%s, 의심 수치 +50)"), 
                *BlasterCharacter->GetName(), *BehaviorName);
            UE_LOG(LogTemp, Warning, TEXT("[DroneActor] %s"), *WarningMessage);
            
            // 디버그 로그
            UE_LOG(LogTemp, Log, TEXT("[DroneActor] DEBUG - Event received: Player: %s, Behavior: %s, Location: %s, Distance: %.2f, Angle: %.2f"), 
                *BlasterCharacter->GetName(), *BehaviorName, 
                *BlasterCharacter->GetActorLocation().ToString(),
                FVector::Dist(GetActorLocation(), BlasterCharacter->GetActorLocation()),
                Angle);
            
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
            
            // 마지막 감지 행동 저장
            LastDetectedBehavior.Add(BlasterCharacter, EventData.Behavior);
        }
    }
}

void ADroneActor::CheckVisiblePlayersSuspiciousBehavior(float DeltaTime)
{
    // 이벤트를 놓쳤을 때를 위한 백업 체크 (대략 0.25초 간격)
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
                        // 이벤트 핸들러와 동일한 로직 사용
                        FSuspicionEventData EventData(BlasterCharacter, Behavior, CurrentTime);
                        OnSuspicionEventReceived(EventData);
                    }
                }
            }
        }
    }
}

