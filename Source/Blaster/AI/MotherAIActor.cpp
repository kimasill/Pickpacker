// Fill out your copyright notice in the Description page of Project Settings.

#include "MotherAIActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
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

AMotherAIActor::AMotherAIActor()
{
	PrimaryActorTick.bCanEverTick = false; // Behavior Tree가 Tick을 대체
	bReplicates = true;
	
	// Pawn 설정
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AMotherAIController::StaticClass();

	// Create components
	MotherMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MotherMesh"));
	RootComponent = MotherMesh;

	StatusWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("StatusWidget"));
	StatusWidget->SetupAttachment(RootComponent);
	StatusWidget->SetWidgetSpace(EWidgetSpace::Screen);
	StatusWidget->SetDrawAtDesiredSize(true);

	// Initialize values
	CurrentState = EMotherAIState::Normal;
	RestPeriodDuration = 60.0f;
	RestPeriodInterval = 300.0f;
	AlertSuspicionThreshold = 30.0f;
	AggressiveSuspicionThreshold = 70.0f;
	MaxDrones = 5;
}

void AMotherAIActor::BeginPlay()
{
	Super::BeginPlay();

	// Get game state
	GameState = GetWorld()->GetGameState<APickpackerGameState>();

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
	}

	// 통제 타워 위치가 설정되지 않았으면 현재 위치 사용
	if (ControlTowerLocation.IsNearlyZero())
	{
		ControlTowerLocation = GetActorLocation();
	}

	// 초기 상태: 통제 타워에서 대기
	if (HasAuthority())
	{
		SetAIState(EMotherAIState::AtControlTower);
		SetActorLocation(ControlTowerLocation);

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
		}
	}

	// 경고 메시지
	FString WarningMessage = FString::Printf(TEXT("경고: %s의 의심 수치가 최대치에 도달했습니다. 제제를 시행합니다."), 
		*BlasterCharacter->GetName());
	SendWarning(WarningMessage);
}

void AMotherAIActor::UpdateApproach(float DeltaTime)
{
	// DEPRECATED: Behavior Tree의 BTTask_MotherMoveToLocation과 BTTask_MotherExecutePunishment로 대체됨
	// 이 함수는 더 이상 호출되지 않음
	UE_LOG(LogTemp, VeryVerbose, TEXT("[MotherAIActor] UpdateApproach called (deprecated - use Behavior Tree)"));
}

void AMotherAIActor::ExecutePunishment(ACharacter* Player)
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

	ABlasterPlayerState* BlasterPlayerState = BlasterCharacter->GetPlayerState<ABlasterPlayerState>();
	if (!BlasterPlayerState)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MotherAIActor] Executing punishment for player: %s"), *Player->GetName());

	// 목숨 감소
	BlasterPlayerState->LoseLife();

	// 의심 수치 초기화 (직접 설정)
	if (HasAuthority())
	{
		float OldSuspicion = BlasterPlayerState->GetPersonalSuspicion();
		BlasterPlayerState->AddPersonalSuspicion(-OldSuspicion); // Reset to 0
		BlasterPlayerState->OnPersonalSuspicionChanged.Broadcast(0.0f, OldSuspicion);
	}

	// 경고 메시지
	FString WarningMessage = FString::Printf(TEXT("제제 완료: %s의 목숨이 감소했습니다. (남은 목숨: %d)"), 
		*BlasterCharacter->GetName(), BlasterPlayerState->GetLives());
	SendWarning(WarningMessage);

	// 처벌 후 통제 타워로 복귀
	ReturnToControlTower();
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

	// Blackboard 업데이트 (Behavior Tree용)
	if (AMotherAIController* MotherController = Cast<AMotherAIController>(GetController()))
	{
		if (UBlackboardComponent* Blackboard = MotherController->GetBlackboardComponent())
		{
			Blackboard->SetValueAsObject(FName("TargetPlayer"), BlasterCharacter);
			SetAIState(EMotherAIState::ChasingPlayer);
		}
	}

	// 즉시 처벌 (의심 스택 쌓지 않음)
	ExecutePunishment(BlasterCharacter);

	// 경고 메시지
	FString WarningMessage = FString::Printf(TEXT("즉시 제제: %s의 의심스러운 행동이 직접 감지되었습니다. (%s)"), 
		*BlasterCharacter->GetName(), *UEnum::GetValueAsString(EventData.Behavior));
	SendWarning(WarningMessage);
}

void AMotherAIActor::StartInspection()
{
	if (!HasAuthority() || InspectionLocations.Num() == 0)
	{
		return;
	}

	// 현재 점검할 시설 선택
	CurrentInspectionIndex = FMath::RandRange(0, InspectionLocations.Num() - 1);
	CurrentInspectionLocation = InspectionLocations[CurrentInspectionIndex];

	SetAIState(EMotherAIState::Inspecting);
	InspectionStartTime = GetWorld()->GetTimeSeconds();

	UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Starting inspection at facility %d"), CurrentInspectionIndex);
	
	// Blackboard 업데이트 (Behavior Tree용)
	if (AMotherAIController* MotherController = Cast<AMotherAIController>(GetController()))
	{
		if (UBlackboardComponent* Blackboard = MotherController->GetBlackboardComponent())
		{
			Blackboard->SetValueAsBool(FName("ShouldInspect"), true);
			Blackboard->SetValueAsVector(FName("InspectionLocation"), CurrentInspectionLocation);
		}
	}
}

void AMotherAIActor::CompleteInspection()
{
	if (CurrentState != EMotherAIState::Inspecting)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Completed inspection at facility %d"), CurrentInspectionIndex);

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

	// Blackboard 업데이트 (Behavior Tree용)
	if (AMotherAIController* MotherController = Cast<AMotherAIController>(GetController()))
	{
		if (UBlackboardComponent* Blackboard = MotherController->GetBlackboardComponent())
		{
			Blackboard->SetValueAsObject(FName("TargetPlayer"), nullptr);
			Blackboard->SetValueAsBool(FName("ShouldInspect"), false);
			Blackboard->SetValueAsVector(FName("TargetLocation"), ControlTowerLocation);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Returning to control tower"));
}

// 레거시 함수들 - Behavior Tree로 대체됨
// 이 함수들은 더 이상 사용되지 않지만, 참고용으로 유지됨
void AMotherAIActor::UpdateInspection(float DeltaTime)
{
	// DEPRECATED: Behavior Tree의 BTTask_MotherMoveToLocation과 BTTask_MotherWaitAtLocation으로 대체됨
	// 이 함수는 더 이상 호출되지 않음
	UE_LOG(LogTemp, VeryVerbose, TEXT("[MotherAIActor] UpdateInspection called (deprecated - use Behavior Tree)"));
}

void AMotherAIActor::UpdateReturnToTower(float DeltaTime)
{
	// DEPRECATED: Behavior Tree의 BTTask_MotherMoveToLocation으로 대체됨
	// 이 함수는 더 이상 호출되지 않음
	UE_LOG(LogTemp, VeryVerbose, TEXT("[MotherAIActor] UpdateReturnToTower called (deprecated - use Behavior Tree)"));
}

void AMotherAIActor::CheckForSuspiciousPlayers(float DeltaTime)
{
	// DEPRECATED: Behavior Tree의 BTService_MotherCheckSuspiciousPlayers로 대체됨
	// 이 함수는 더 이상 호출되지 않음
	UE_LOG(LogTemp, VeryVerbose, TEXT("[MotherAIActor] CheckForSuspiciousPlayers called (deprecated - use Behavior Tree)"));
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

	// Line of sight 확인
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(Player);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		MotherLocation,
		PlayerLocation,
		ECC_Visibility,
		QueryParams
	);

	return !bHit || HitResult.GetActor() == Player;
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

	// 게임시간 가져오기 (간단히 World의 TimeSeconds 사용, 실제로는 게임시간 시스템 필요)
	float CurrentGameTime = GetWorld()->GetTimeSeconds();
	
	// 다음 점검 시간 찾기
	float NextInspectionTime = MAX_FLT;
	for (float InspectionTime : InspectionTimes)
	{
		// 하루를 24시간으로 가정하고 게임시간 계산
		// 실제 게임시간 시스템이 있으면 그걸 사용해야 함
		float GameDayLength = 1440.0f; // 24분 = 하루 (게임시간)
		float CurrentHour = FMath::Fmod(CurrentGameTime, GameDayLength) / 60.0f;
		
		float TimeUntilInspection = InspectionTime - CurrentHour;
		if (TimeUntilInspection < 0.0f)
		{
			TimeUntilInspection += 24.0f; // 다음 날
		}
		
		float RealTimeUntilInspection = TimeUntilInspection * 60.0f; // 분을 초로 변환
		if (RealTimeUntilInspection < NextInspectionTime)
		{
			NextInspectionTime = RealTimeUntilInspection;
		}
	}

	if (NextInspectionTime < MAX_FLT)
	{
		GetWorld()->GetTimerManager().SetTimer(
			InspectionTimerHandle,
			this,
			&AMotherAIActor::StartInspection,
			NextInspectionTime,
			false
		);

		UE_LOG(LogTemp, Log, TEXT("[MotherAIActor] Scheduled next inspection in %.2f seconds"), NextInspectionTime);
	}
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


