// Fill out your copyright notice in the Description page of Project Settings.

#include "MotherAIActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Blaster/GameState/PickpackerGameState.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Blaster/AI/DroneActor.h"
#include "Blaster/Subsystem/SuspicionManagerSubsystem.h"
#include "Blaster/AI/MotherAIController.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Blaster/PickpackerTypes/PickpackerTypes.h"
#include "GameFramework/PawnMovementComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

AMotherAIActor::AMotherAIActor()
{
	PrimaryActorTick.bCanEverTick = false; // Behavior Tree가 Tick을 대체
	bReplicates = true;
	SetReplicateMovement(true);
	// Character 설정
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AMotherAIController::StaticClass();

	// Character는 기본적으로 CapsuleComponent를 RootComponent로 가집니다.
	// SkeletalMeshComponent는 GetMesh()로 접근 가능합니다.
	GetCapsuleComponent()->SetCapsuleHalfHeight(88.0f);
	GetCapsuleComponent()->SetCapsuleRadius(34.0f);
	
	// SkeletalMesh는 블루프린트에서 설정하거나 여기서 설정할 수 있습니다.
	// GetMesh()->SetSkeletalMesh(...);

	StatusWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("StatusWidget"));
	StatusWidget->SetupAttachment(RootComponent);
	StatusWidget->SetWidgetSpace(EWidgetSpace::Screen);
	StatusWidget->SetDrawAtDesiredSize(true);

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

	// Initialize values
	CurrentState = EMotherAIState::Normal;
	RestPeriodDuration = 60.0f;
	RestPeriodInterval = 300.0f;
	AlertSuspicionThreshold = 30.0f;
	AggressiveSuspicionThreshold = 70.0f;
	MaxDrones = 5;

	if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
	{
		MovementComp->bOrientRotationToMovement = true; // 이동 방향으로 회전
		MovementComp->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // 회전 속도 설정
	}
	PunishmentMontage = nullptr;
	InspectionMontage = nullptr;
}

void AMotherAIActor::BeginPlay()
{
	Super::BeginPlay();

	// Get game state
	GameState = GetWorld()->GetGameState<APickpackerGameState>();

	// Bind perception updated
	if (PerceptionComp)
	{
		PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AMotherAIActor::OnTargetPerceptionUpdated);
	}

	// Subscribe to suspicion changes
	if (GameState)
	{
		GameState->OnSuspicionChanged.AddDynamic(this, &AMotherAIActor::OnSuspicionChanged_Handler);
	}
	// SuspicionManager에 구독 (즉시 처벌용)
	if (HasAuthority())
	{
		if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
		{
			if (USuspicionManagerSubsystem* SuspicionManager = GameInstance->GetSubsystem<USuspicionManagerSubsystem>())
			{
				SuspicionManager->SubscribeToSuspicionEvents(this);
				UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Subscribed to SuspicionManager for immediate punishment"));
			}
		}

		// 모든 플레이어의 PersonalSuspicion 변경 이벤트 구독 (의심 수치 100 도달 감지용)
		// BeginPlay 시점에는 플레이어가 아직 생성되지 않았을 수 있으므로, 
		// 첫 번째 틱에서 구독하거나 타이머로 지연 구독
		GetWorld()->GetTimerManager().SetTimer(
			SubscribeToPlayersTimerHandle,
			this,
			&AMotherAIActor::SubscribeToAllPlayerStates,
			1.0f,
			false
		);

		// 레벨에 미리 스폰된 드론들을 ActiveDrones에 추가
		for (TActorIterator<ADroneActor> DroneItr(GetWorld()); DroneItr; ++DroneItr)
		{
			ADroneActor* ExistingDrone = *DroneItr;
			if (ExistingDrone && !ActiveDrones.Contains(ExistingDrone))
			{
				ActiveDrones.Add(ExistingDrone);
				UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Found pre-spawned drone: %s"), *ExistingDrone->GetName());
			}
		}
	}

	// 통제 타워 액터가 설정되지 않았으면 현재 위치를 통제 타워로 사용
	if (!ControlTowerActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MotherAIActor] ControlTowerActor not set, using current location"));
	}

	// 초기 상태: 통제 타워에서 대기
	if (HasAuthority())
	{
		SetAIState(EMotherAIState::AtControlTower);
		FVector TowerLocation = GetControlTowerLocation();
		if (!TowerLocation.IsNearlyZero())
		{
			SetActorLocation(TowerLocation);
		}

		// 첫 번째 점검 스케줄링
		ScheduleNextInspection();
	}

	// Initial state update
	UpdateAIState();
}

void AMotherAIActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Behavior Tree가 대부분의 로직을 처리하므로 여기서는 드론 관리만 수행
	// Note: Tick is disabled (PrimaryActorTick.bCanEverTick = false), but if enabled, only manage drones
	if (!HasAuthority())
	{
		return;
	}

	// Manage drones periodically
	static float DroneManageTimer = 0.0f;
	DroneManageTimer += DeltaTime;
	if (DroneManageTimer >= 5.0f) // Every 5 seconds
	{
		ManageDrones();
		DroneManageTimer = 0.0f;
	}
}

void AMotherAIActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMotherAIActor, CurrentState);
	DOREPLIFETIME(AMotherAIActor, ActiveDrones);
	DOREPLIFETIME(AMotherAIActor, DetectedPlayers);
}

void AMotherAIActor::SetAIState(EMotherAIState NewState)
{
	if (CurrentState == NewState)
	{
		return;
	}

	EMotherAIState OldState = CurrentState;
	CurrentState = NewState;

	OnStateChanged.Broadcast(NewState);

	ApplySpeedForState(NewState);

	if (HasAuthority())
	{
		OnRep_State(OldState);
	}
}

float AMotherAIActor::GetSuspicionLevel() const
{
	if (GameState)
	{
		return GameState->GetSuspicionLevel();
	}
	return 0.0f;
}

void AMotherAIActor::StartRestPeriod()
{
	if (CurrentState == EMotherAIState::RestPeriod)
	{
		return;
	}

	SetAIState(EMotherAIState::RestPeriod);

	// Deactivate all drones
	for (ADroneActor* Drone : ActiveDrones)
	{
		if (Drone)
		{
			Drone->SetActive(false);
		}
	}

	// Set timer to end rest period
	if (HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(
			RestPeriodTimer,
			this,
			&AMotherAIActor::OnRestPeriodTimerFinished,
			RestPeriodDuration,
			false
		);
	}

	OnRestPeriodStarted.Broadcast(RestPeriodDuration);
	SendWarning(TEXT("Rest period started. Surveillance reduced."));
}

void AMotherAIActor::EndRestPeriod()
{
	if (CurrentState != EMotherAIState::RestPeriod)
	{
		return;
	}

	// Reactivate drones
	for (ADroneActor* Drone : ActiveDrones)
	{
		if (Drone)
		{
			Drone->SetActive(true);
		}
	}

	// Update state based on suspicion
	UpdateAIState();

	OnRestPeriodEnded.Broadcast();
	SendWarning(TEXT("Rest period ended. Full surveillance resumed."));
}

ADroneActor* AMotherAIActor::SpawnDrone(const FVector& Location)
{
	if (!DroneClass || ActiveDrones.Num() >= MaxDrones)
	{
		return nullptr;
	}

	if (!HasAuthority())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ADroneActor* NewDrone = GetWorld()->SpawnActor<ADroneActor>(DroneClass, Location, FRotator::ZeroRotator, SpawnParams);
	if (NewDrone)
	{
		ActiveDrones.Add(NewDrone);
	}

	return NewDrone;
}

void AMotherAIActor::SendWarning(const FString& Message)
{
	OnWarningSent.Broadcast(Message);

	// Broadcast to all players (can be implemented with UI system)
	UE_LOG(LogTemp, Warning, TEXT("[Mother AI] Warning: %s"), *Message);
}

void AMotherAIActor::PunishPlayers(float SuspicionPoints)
{
	if (GameState)
	{
		GameState->AddTeamSuspicion(SuspicionPoints);
	}

	// Spawn additional drones if needed
	if (ActiveDrones.Num() < MaxDrones && DroneSpawnLocations.Num() > 0)
	{
		int32 DronesToSpawn = FMath::Min(MaxDrones - ActiveDrones.Num(), 2);
		for (int32 i = 0; i < DronesToSpawn; ++i)
		{
			if (DroneSpawnLocations.Num() > 0)
			{
				int32 RandomIndex = FMath::RandRange(0, DroneSpawnLocations.Num() - 1);
				SpawnDrone(DroneSpawnLocations[RandomIndex]);
			}
		}
	}
}

void AMotherAIActor::UpdateAIState()
{
	if (CurrentState == EMotherAIState::RestPeriod)
	{
		return; // Don't update during rest period
	}

	float Suspicion = GetSuspicionLevel();

	if (Suspicion >= AggressiveSuspicionThreshold)
	{
		SetAIState(EMotherAIState::Aggressive);
	}
	else if (Suspicion >= AlertSuspicionThreshold)
	{
		SetAIState(EMotherAIState::Alert);
	}
	else
	{
		SetAIState(EMotherAIState::Normal);
	}
}

void AMotherAIActor::ManageDrones()
{
	if (CurrentState == EMotherAIState::RestPeriod)
	{
		return;
	}

	float Suspicion = GetSuspicionLevel();

	// Calculate desired number of drones based on suspicion
	int32 DesiredDrones = FMath::Clamp(
		FMath::RoundToInt(Suspicion * MaxDrones),
		1,
		MaxDrones
	);

	// Remove excess drones
	while (ActiveDrones.Num() > DesiredDrones)
	{
		ADroneActor* Drone = ActiveDrones.Last();
		if (Drone)
		{
			Drone->Destroy();
		}
		ActiveDrones.RemoveAt(ActiveDrones.Num() - 1);
	}

	// Spawn additional drones if needed
	while (ActiveDrones.Num() < DesiredDrones && DroneSpawnLocations.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, DroneSpawnLocations.Num() - 1);
		SpawnDrone(DroneSpawnLocations[RandomIndex]);
	}
}

void AMotherAIActor::OnRestPeriodTimerFinished()
{
	EndRestPeriod();

	// Schedule next rest period
	GetWorld()->GetTimerManager().SetTimer(
		RestPeriodIntervalTimer,
		this,
		&AMotherAIActor::OnRestPeriodIntervalTimerFinished,
		RestPeriodInterval,
		false
	);
}

void AMotherAIActor::OnRep_State(EMotherAIState OldState)
{
	OnStateChanged.Broadcast(CurrentState);
}

void AMotherAIActor::PlayPunishmentMontage()
{
	if(PunishmentMontage && GetMesh() && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Play(PunishmentMontage);
	}
}

void AMotherAIActor::PlayInspectionMontage()
{
	if(InspectionMontage && GetMesh() && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Play(InspectionMontage);
	}
}

void AMotherAIActor::Multicast_Punishment_Implementation()
{
	PlayPunishmentMontage();
}

void AMotherAIActor::Multicast_PunishmentEnd_Implementation(bool bInterrupted)
{
	OnPunishmentFinished.Broadcast(bInterrupted);
}


void AMotherAIActor::Multicast_InspectionEnd_Implementation(bool interrupted)
{
}

void AMotherAIActor::Multicast_Inspection_Implementation()
{
	PlayInspectionMontage();
}

void AMotherAIActor::OnSuspicionChanged_Handler(float NewSuspicionLevel)
{
	UpdateAIState();
}

void AMotherAIActor::RequestPunishment(ACharacter* Player)
{
	if (!Player || !HasAuthority())
	{
		return;
	}

	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Player);
	if (!BlasterCharacter)
	{
		return;
	}

	ABlasterPlayerState* SuspictionPlayerState = BlasterCharacter->GetPlayerState<ABlasterPlayerState>();
	if (!SuspictionPlayerState || !SuspictionPlayerState->IsSuspicionMaxed())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MotherAIActor] Requesting punishment for player: %s"), *Player->GetName());

	// 플레이어에게 접근 시작
	TargetPlayer = Player;
	bIsApproachingPlayer = true;
	SetAIState(EMotherAIState::ChasingPlayer);

	// Blackboard 업데이트 (Behavior Tree용)
	if (AMotherAIController* MonterController = Cast<AMotherAIController>(GetController()))
	{
		if (UBlackboardComponent* Blackboard = MonterController->GetBlackboardComponent())
		{
			Blackboard->SetValueAsObject(FName("TargetPlayer"), BlasterCharacter);
			Blackboard->SetValueAsBool(FName("ShouldPunish"), true);
			Blackboard->SetValueAsBool(FName("Chasing"), true);
			
			// CanSeeTarget을 즉시 설정 (비헤이비어 트리 진입 시 올바른 노드가 실행되도록)
			// 플레이어를 볼 수 있는지 확인하고 설정
			bool bCanSee = CanSeePlayer(BlasterCharacter);
			Blackboard->SetValueAsBool(FName("CanSeeTarget"), bCanSee);
			UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Set CanSeeTarget to %s on punishment request"), 
				bCanSee ? TEXT("True") : TEXT("False"));
		}
	}

	// 경고 메시지
	FString WarningMessage = FString::Printf(TEXT("경고: %s의 의심 수치가 최대치에 도달했습니다. 제제를 시행합니다."), 
		*BlasterCharacter->GetName());
	SendWarning(WarningMessage);
}

void AMotherAIActor::ExecutePunishment(ACharacter* Player)
{
	if (!Player || !HasAuthority())
	{
		return;
	}

	// 이미 처벌 중이면 중복 실행 방지
	if (bIsExecutingPunishment)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MotherAIActor] Punishment already in progress, ignoring duplicate call"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MotherAIActor] Executing punishment for player: %s"), *Player->GetName());

	// 처벌 시작 플래그 설정
	bIsExecutingPunishment = true;

	// TargetPlayer 설정 (OnPunishmentHit에서 사용)
	TargetPlayer = Player;
	bIsApproachingPlayer = true;

	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Player);
	if (!BlasterCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MotherAIActor] Failed to cast player to BlasterCharacter"));
		bIsExecutingPunishment = false;
		return;
	}

	// 거리 확인 및 조정
	FVector MotherLocation = GetActorLocation();
	FVector PlayerLocation = Player->GetActorLocation();
	float CurrentDistance = FVector::Dist(MotherLocation, PlayerLocation);

	UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Current distance to player: %.2f, target distance: %.2f"), 
		CurrentDistance, PunishmentDistance);

	// 거리가 맞지 않으면 조정
	if (FMath::Abs(CurrentDistance - PunishmentDistance) > 10.0f)
	{
		// 목표 위치 계산 (플레이어로부터 PunishmentDistance만큼 떨어진 위치)
		FVector DirectionToPlayer = (PlayerLocation - MotherLocation).GetSafeNormal();
		FVector TargetLocation = PlayerLocation - DirectionToPlayer * PunishmentDistance;
		
		// 목표 위치로 이동
		SetActorLocation(TargetLocation);
		UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Adjusted position to maintain punishment distance"));
	}

	// Mother AI를 플레이어를 향하도록 회전
	FVector ToPlayer = (PlayerLocation - GetActorLocation()).GetSafeNormal();
	ToPlayer.Z = 0.0f;
	if (!ToPlayer.IsNearlyZero())
	{
		// 기본은 +X 정면. 메쉬가 +Y 정면이면 Yaw에 +90도 보정
		const FRotator LookAtRotation = ToPlayer.Rotation();
		const FRotator AdjustedRotation(0.f, LookAtRotation.Yaw, 0.f); // +Y를 정면으로 만들기
		SetActorRotation(AdjustedRotation);
	}

	// 플레이어를 멈추고 Mother를 향하도록 회전 (카메라 회전 시작)
	BlasterCharacter->SetBeingPunished(true, this);
	UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Player %s movement disabled, starting camera rotation"), *Player->GetName());

	// 카메라 회전 대기 후 처벌 모션 실행
	GetWorld()->GetTimerManager().SetTimer(
		PunishmentCameraRotationTimer,
		this,
		&AMotherAIActor::StartPunishmentMontage,
		CameraRotationWaitTime,
		false
	);
}

void AMotherAIActor::StartPunishmentMontage()
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Camera rotation complete, starting punishment montage"));

	FOnMontageEnded MontageEndedDelegate;
	MontageEndedDelegate.BindUObject(this, &AMotherAIActor::OnPunishmentEnd);
	if (GetMesh() && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_SetEndDelegate(MontageEndedDelegate, PunishmentMontage);
	}

	Multicast_Punishment();
	PlayPunishmentMontage();
}

void AMotherAIActor::OnPunishmentHit()
{
	if (!HasAuthority())
	{
		return;
	}

	// TargetPlayer 멤버 변수에서 플레이어 가져오기
	if (!TargetPlayer.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MotherAIActor] OnPunishmentHit: TargetPlayer is invalid"));
		return;
	}

	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(TargetPlayer.Get());
	if (!BlasterCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MotherAIActor] OnPunishmentHit: Failed to cast TargetPlayer to BlasterCharacter"));
		return;
	}

	ABlasterPlayerState* BlasterPlayerState = BlasterCharacter->GetPlayerState<ABlasterPlayerState>();
	if (!BlasterPlayerState)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MotherAIActor] OnPunishmentHit: Failed to get BlasterPlayerState"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MotherAIActor] OnPunishmentHit: Applying punishment to %s"), *BlasterCharacter->GetName());

	// 목숨 감소
	BlasterPlayerState->LoseLife();

	// 의심 수치 초기화 (직접 설정)
	float OldSuspicion = BlasterPlayerState->GetPersonalSuspicion();
	BlasterPlayerState->AddPersonalSuspicion(-OldSuspicion); // Reset to 0
	BlasterPlayerState->OnPersonalSuspicionChanged.Broadcast(0.0f, OldSuspicion);

	UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] OnPunishmentHit: Life lost, suspicion reset. Remaining lives: %d"), BlasterPlayerState->GetLives());
}

void AMotherAIActor::OnPunishmentEnd(UAnimMontage* Montage, bool bInterrupted)
{
	if (!HasAuthority())
	{
		return;
	}

	// TargetPlayer 멤버 변수에서 플레이어 가져오기
	ABlasterCharacter* BlasterCharacter = nullptr;
	if (TargetPlayer.IsValid())
	{
		BlasterCharacter = Cast<ABlasterCharacter>(TargetPlayer.Get());
	}

	// 경고 메시지 (플레이어가 유효한 경우)
	if (BlasterCharacter)
	{
		ABlasterPlayerState* BlasterPlayerState = BlasterCharacter->GetPlayerState<ABlasterPlayerState>();
		if (BlasterPlayerState)
		{
			FString WarningMessage = FString::Printf(TEXT("제제 완료: %s의 목숨이 감소했습니다. (남은 목숨: %d)"),
				*BlasterCharacter->GetName(), BlasterPlayerState->GetLives());
			SendWarning(WarningMessage);
		}

		// 플레이어 움직임 복구
		BlasterCharacter->SetBeingPunished(false, nullptr);
		UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Player %s movement restored after punishment"), *BlasterCharacter->GetName());
	}
	Multicast_PunishmentEnd(bInterrupted);
	OnPunishmentFinished.Broadcast(bInterrupted);

	UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] OnPunishmentEnd: Punishment animation ended"));

	// AI 움직임 재개
	if (AMotherAIController* MotherController = Cast<AMotherAIController>(GetController()))
	{
		// 움직임 재개는 Behavior Tree가 자동으로 처리하므로 여기서는 특별한 처리가 필요 없음
		UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Movement will resume via Behavior Tree"));
	}

	// 점검 중인지 확인
	bool bWasInspecting = (CurrentState == EMotherAIState::Inspecting);

	// Blackboard 업데이트: TargetPlayer 제거
	if (AMotherAIController* MotherController = Cast<AMotherAIController>(GetController()))
	{
		if (UBlackboardComponent* Blackboard = MotherController->GetBlackboardComponent())
		{
			Blackboard->SetValueAsObject(FName("TargetPlayer"), nullptr);
			// 점검 중이었다면 점검을 계속하기 위해 ShouldInspect는 true로 유지
			if (bWasInspecting)
			{
				// 점검 위치로 복귀하도록 TargetLocation 설정
				Blackboard->SetValueAsVector(FName("TargetLocation"), CurrentInspectionLocation);
				
				UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Punishment completed during inspection, returning to inspection location"));
			}	

		}
	}

	// 처벌 완료 플래그 해제
	bIsExecutingPunishment = false;

	// 점검 중이 아니었다면 통제 타워로 복귀
	if (!bWasInspecting)
	{
		ReturnToControlTower();
	}
	else
	{
		// 점검 중이었다면 점검 상태 유지 (TargetPlayer만 제거)
		bIsApproachingPlayer = false;
		TargetPlayer = nullptr;
		// AI 상태는 Inspecting으로 유지
	}
}

void AMotherAIActor::OnInspectionEnd()
{
	// 점검 종료 처리: 점검 모션 이후 호출 함수
	if (!HasAuthority())
	{
		return;
	}
	if (CurrentState != EMotherAIState::Inspecting)
	{
		return;
	}

	Multicast_InspectionEnd(false);
}

void AMotherAIActor::UpdatePlayerLocationFromDrone(ACharacter* Player, const FVector& NewLocation)
{
	if (!Player || !HasAuthority())
	{
		return;
	}

	// Check if this player is the target
	if (AMotherAIController* MotherController = Cast<AMotherAIController>(GetController()))
	{
		if (UBlackboardComponent* Blackboard = MotherController->GetBlackboardComponent())
		{
			ACharacter* LocalTargetPlayer = Cast<ACharacter>(Blackboard->GetValueAsObject(FName("TargetPlayer")));
			if (LocalTargetPlayer == Player)
			{
				// Update TargetLocation in blackboard (drone reported new location)
				Blackboard->SetValueAsVector(FName("TargetLocation"), NewLocation);
				UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Drone updated player location: %s"), *NewLocation.ToString());
			}
		}
	}
}

void AMotherAIActor::OnSuspicionEventReceived(const FSuspicionEventData& EventData)
{
	if (!HasAuthority() || !EventData.Player)
	{
		return;
	}

	ABlasterCharacter* BlasterCharacter = EventData.Player;

	// 마더가 플레이어를 볼 수 있는지 확인
	if (!CanSeePlayer(BlasterCharacter))
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MotherAIActor] Directly detected suspicious behavior: %s from player %s"), 
		*UEnum::GetValueAsString(EventData.Behavior), *BlasterCharacter->GetName());

	// 점검 중이어도 플레이어 위반행동 감지 시 즉시 처벌
	// Blackboard 업데이트 (Behavior Tree가 우선순위를 변경하도록)
	if (AMotherAIController* MotherController = Cast<AMotherAIController>(GetController()))
	{
		if (UBlackboardComponent* Blackboard = MotherController->GetBlackboardComponent())
		{
			// TargetPlayer 설정으로 Behavior Tree가 처벌 우선순위로 전환
			Blackboard->SetValueAsObject(FName("TargetPlayer"), BlasterCharacter);
			SetAIState(EMotherAIState::ChasingPlayer);
			
			// 점검 중이었다면 ShouldInspect는 유지하되, 처벌 후 복귀할 수 있도록
			// (OnPunishmentEnd에서 처리)
		}
	} 
	ABlasterPlayerState* BlasterPlayerState = BlasterCharacter->GetPlayerState<ABlasterPlayerState>();
	if (BlasterPlayerState)
	{
		BlasterPlayerState->AddPersonalSuspicion(100.0f);
		FString BehaviorName = UEnum::GetValueAsString(EventData.Behavior);		
	}
}

void AMotherAIActor::TriggerInspection()
{
	if (!HasAuthority() || InspectionActorLocations.Num() == 0)
	{
		return;
	}

	// 현재 점검할 시설 선택 (마더에 저장된 인덱스 사용)
	if (CurrentInspectionIndex >= InspectionActorLocations.Num())
	{
		CurrentInspectionIndex = 0;
	}
	
	if (InspectionActorLocations[CurrentInspectionIndex])
	{
		CurrentInspectionActor = InspectionActorLocations[CurrentInspectionIndex];
		CurrentInspectionLocation = CurrentInspectionActor->GetActorLocation();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[MotherAIActor] InspectionActorLocations[%d] is null"), CurrentInspectionIndex);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Triggering inspection at facility %d"), CurrentInspectionIndex);
	
	// Blackboard 업데이트 (Behavior Tree가 이동 및 몽타주 재생 처리)
	if (AMotherAIController* MotherController = Cast<AMotherAIController>(GetController()))
	{
		if (UBlackboardComponent* Blackboard = MotherController->GetBlackboardComponent())
		{
			Blackboard->SetValueAsBool(FName("ShouldInspect"), true);
			Blackboard->SetValueAsVector(FName("InspectionLocation"), CurrentInspectionLocation);
			Blackboard->SetValueAsVector(FName("TargetLocation"), CurrentInspectionLocation);
			Blackboard->SetValueAsInt(FName("InspectionCounter"), InspectionCounter);
			Blackboard->SetValueAsObject(FName("InspectionActor"), CurrentInspectionActor);
			UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Blackboard updated: ShouldInspect=true, InspectionLocation=%s, InspectionActor=%s"), 
				*CurrentInspectionLocation.ToString(), *GetNameSafe(CurrentInspectionActor));
		}
	}
}

void AMotherAIActor::StartInspection()
{
	if (!HasAuthority())
	{
		return;
	}

	SetAIState(EMotherAIState::Inspecting);
	Multicast_Inspection();
	PlayInspectionMontage();
	this->InspectionStartTime = GetWorld()->GetTimeSeconds();

	UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Starting inspection montage at facility %d"), CurrentInspectionIndex);
}

void AMotherAIActor::CompleteInspection()
{
	if (!HasAuthority())
	{
		return;
	}

	if (CurrentState != EMotherAIState::Inspecting)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Completed inspection at facility %d"), CurrentInspectionIndex);

	// 다음 점검 인덱스로 이동 (마지막 지점부터 계속)
	CurrentInspectionIndex++;
	if (CurrentInspectionIndex >= InspectionActorLocations.Num())
	{
		CurrentInspectionIndex = 0;
	}

	// Blackboard 업데이트 (Behavior Tree용)
	if (AMotherAIController* MotherController = Cast<AMotherAIController>(GetController()))
	{
		if (UBlackboardComponent* Blackboard = MotherController->GetBlackboardComponent())
		{
			Blackboard->SetValueAsBool(FName("ShouldInspect"), false);
		}
	}

	// 통제 타워로 복귀
	ReturnToControlTower();

	// 다음 점검 스케줄링
	ScheduleNextInspection();
}

void AMotherAIActor::ReturnToControlTower()
{
	if (!HasAuthority())
	{
		return;
	}

	SetAIState(EMotherAIState::AtControlTower);
	bIsApproachingPlayer = false;
	TargetPlayer = nullptr;

	// Movement Component가 활성화되어 있는지 확인
	if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
	{
		if (MovementComp->MovementMode == MOVE_None)
		{
			MovementComp->SetMovementMode(MOVE_Walking);
			UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Movement component reactivated in ReturnToControlTower"));
		}
	}

	// Blackboard 업데이트 (Behavior Tree용)
	if (AMotherAIController* MotherController = Cast<AMotherAIController>(GetController()))
	{
		// 현재 실행 중인 MoveTo 태스크를 중단
		MotherController->StopMovement();

		if (UBlackboardComponent* Blackboard = MotherController->GetBlackboardComponent())
		{
			Blackboard->SetValueAsObject(FName("TargetPlayer"), nullptr);
			FVector TowerLocation = GetControlTowerLocation();
			Blackboard->SetValueAsVector(FName("TargetLocation"), TowerLocation);

			UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Blackboard updated for return to control tower: %s"),
				*TowerLocation.ToString());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Returning to control tower"));
}

void AMotherAIActor::MoveToLocation(const FVector& TargetLocation, float Speed)
{
	FVector CurrentLocation = GetActorLocation();
	FVector Direction = (TargetLocation - CurrentLocation).GetSafeNormal();
	float Distance = FVector::Dist(CurrentLocation, TargetLocation);

	if (Distance > 10.0f)
	{
		FVector NewLocation = CurrentLocation + Direction * Speed * GetWorld()->GetDeltaSeconds();
		SetActorLocation(NewLocation);

		// 목표를 바라보기
		FRotator LookAtRotation = FRotationMatrix::MakeFromX(Direction).Rotator();
		SetActorRotation(LookAtRotation);
	}
}

bool AMotherAIActor::CanSeePlayer(ACharacter* Player) const
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

	// 플레이어가 앉기 상태인지 확인 (책상 아래 등에 숨어있는 경우)
	// TODO: 단순 앉기가 아니라 오브젝트에 가려져야함
	if (BlasterCharacter->bIsCrouched)
	{
		// 앉기 상태일 때는 감지하지 않음
		return false;
	}

	FVector MotherLocation = GetActorLocation();
	FVector PlayerLocation = Player->GetActorLocation();
	FVector ToPlayer = (PlayerLocation - MotherLocation).GetSafeNormal();
	FVector Forward = GetActorForwardVector();

	// 거리 확인
	float Distance = FVector::Dist(MotherLocation, PlayerLocation);
	if (Distance > DetectionRange)
	{
		return false;
	}

	// 각도 확인
	float DotProduct = FVector::DotProduct(Forward, ToPlayer);
	float Angle = FMath::RadiansToDegrees(FMath::Acos(DotProduct));
	if (Angle > DetectionAngle * 0.5f)
	{
		return false;
	}

	// Line of sight 확인 (오브젝트에 가려져 있는지 확인)
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(Player);
	QueryParams.bTraceComplex = true; // 복잡한 충돌 체크

	// 마더의 눈 위치에서 플레이어의 머리 위치로 트레이스
	FVector TraceStart = MotherLocation + FVector(0, 0, 150.0f); // 마더의 눈 높이
	FVector TraceEnd = PlayerLocation + FVector(0, 0, 100.0f); // 플레이어의 머리 높이

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams
	);

	// 오브젝트에 가려져 있으면 못봄
	if (bHit && HitResult.GetActor() != Player)
	{
		return false;
	}

	return true;
}

bool AMotherAIActor::HasReachedLocation(const FVector& TargetLocation, float Tolerance) const
{
	float Distance = FVector::Dist(GetActorLocation(), TargetLocation);
	return Distance <= Tolerance;
}

void AMotherAIActor::ScheduleNextInspection()
{
	if (!HasAuthority() || InspectionTimes.Num() == 0)
	{
		return;
	}

	// 게임 시간 시스템에서 현재 게임 시간 가져오기
	APickpackerGameState* PickpackerGameState = GetWorld()->GetGameState<APickpackerGameState>();
	if (!PickpackerGameState)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MotherAIActor] PickpackerGameState not found, cannot schedule inspection"));
		return;
	}

	float CurrentGameHour = PickpackerGameState->GetCurrentGameHour();
	
	// 다음 점검 시간 찾기
	float NextInspectionHour = MAX_FLT;
	for (float InspectionTime : InspectionTimes)
	{
		float TimeUntilInspection = InspectionTime - CurrentGameHour;
		if (TimeUntilInspection < 0.0f)
		{
			TimeUntilInspection += 24.0f; // 다음 날
		}
		
		if (TimeUntilInspection < NextInspectionHour)
		{
			NextInspectionHour = TimeUntilInspection;
		}
	}

	if (NextInspectionHour < MAX_FLT)
	{
		// 게임 시간을 실제 시간(초)으로 변환
		float RealTimeUntilInspection = PickpackerGameState->ConvertGameHoursToRealSeconds(NextInspectionHour);
		
		// 타이머로 Blackboard 업데이트 함수 호출 (StartInspection 직접 호출하지 않음)
		GetWorld()->GetTimerManager().SetTimer(
			InspectionTimerHandle,
			this,
			&AMotherAIActor::TriggerInspection,
			RealTimeUntilInspection,
			false
		);

		UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Scheduled next inspection in %.2f game hours (%.2f real seconds)"), 
			NextInspectionHour, RealTimeUntilInspection);
	}
}

void AMotherAIActor::ApplySpeedForState(EMotherAIState NewState)
{
	if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
	{
		float Desired = WalkSpeed;
		if (NewState == EMotherAIState::ChasingPlayer)
		{
			Desired = ChaseSpeed;
		}
		else if (NewState == EMotherAIState::Searching)
		{
			// 탐색 상태에서는 느리게 이동 (긴장감)
			Desired = WalkSpeed;
		}
		CurrentDesiredSpeed = Desired;
		MovementComp->MaxWalkSpeed = Desired;

		MovementComp->RotationRate = (NewState == EMotherAIState::ChasingPlayer)
			? FRotator(0.f, 720.f, 0.f) // 추격 시 빠른 회전
			: FRotator(0.f, 540.f, 0.f); // 기본 회전
	}
}

FVector AMotherAIActor::GetControlTowerLocation() const
{
	if (ControlTowerActor)
	{
		return ControlTowerActor->GetActorLocation();
	}
	return GetActorLocation(); // Fallback to current location
}

TArray<FVector> AMotherAIActor::GetInspectionLocations() const
{
	TArray<FVector> Locations;
	for (AActor* InspectionActor : InspectionActorLocations)
	{
		if (InspectionActor)
		{
			Locations.Add(InspectionActor->GetActorLocation());
		}
	}
	return Locations;
}

void AMotherAIActor::OnRestPeriodIntervalTimerFinished()
{
	// 통제 타워에 있을 때만 휴식 시작
	if (CurrentState == EMotherAIState::AtControlTower)
	{
		StartRestPeriod();
	}
	else
	{
		// 다른 상태면 다음 휴식 시간 재스케줄
		GetWorld()->GetTimerManager().SetTimer(
			RestPeriodIntervalTimer,
			this,
			&AMotherAIActor::OnRestPeriodIntervalTimerFinished,
			RestPeriodInterval,
			false
		);
	}
}

void AMotherAIActor::SubscribeToAllPlayerStates()
{
	if (!HasAuthority())
	{
		return;
	}

	// 모든 플레이어 상태 찾기
	for (TActorIterator<APlayerState> PlayerStateItr(GetWorld()); PlayerStateItr; ++PlayerStateItr)
	{
		ABlasterPlayerState* BlasterPlayerState = Cast<ABlasterPlayerState>(*PlayerStateItr);
		if (BlasterPlayerState && !SubscribedPlayerStates.Contains(BlasterPlayerState))
		{
			BlasterPlayerState->OnPersonalSuspicionChanged.AddDynamic(this, &AMotherAIActor::OnPlayerSuspicionChanged);
			SubscribedPlayerStates.Add(BlasterPlayerState);
			UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Subscribed to player state: %s"), *BlasterPlayerState->GetPlayerName());
		}
	}

	// 플레이어가 아직 생성되지 않았을 수 있으므로, 주기적으로 다시 시도
	GetWorld()->GetTimerManager().SetTimer(
		SubscribeToPlayersTimerHandle,
		this,
		&AMotherAIActor::SubscribeToAllPlayerStates,
		5.0f,
		false
	);
}

void AMotherAIActor::OnPlayerSuspicionChanged(float NewSuspicion, float OldSuspicion)
{
	if (!HasAuthority())
	{
		return;
	}

	// 의심 수치가 100에 도달했는지 확인
	if (NewSuspicion >= 100.0f && OldSuspicion < 100.0f)
	{
		// 구독 중인 플레이어 상태 중에서 의심 수치가 100인 플레이어 찾기
		for (ABlasterPlayerState* SuspictionState : SubscribedPlayerStates)
		{
			if (SuspictionState && SuspictionState->GetPersonalSuspicion() >= 100.0f)
			{
				ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(SuspictionState->GetPawn());
				if (BlasterCharacter)
				{
					UE_LOG(LogTemp, Warning, TEXT("[MotherAIActor] Player %s suspicion reached 100, requesting punishment"), 
						*SuspictionState->GetPlayerName());
					RequestPunishment(BlasterCharacter);
					break; // 한 번에 하나만 처리
				}
			}
		}
	}
}

void AMotherAIActor::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!HasAuthority() || !Actor)
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

		UE_LOG(LogTemp, VeryVerbose, TEXT("[MotherAIActor] Player %s detected within line of sight and angle."), *Character->GetName());
		
		// 플레이어를 감지 목록에 추가 (여러 명 감지 가능)
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
			UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Player %s added to detected list"), *Character->GetName());
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
				UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Player %s removed from detected list"), *Character->GetName());
				break;
			}
		}
	}
}

TArray<ACharacter*> AMotherAIActor::GetDetectedPlayers() const
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














