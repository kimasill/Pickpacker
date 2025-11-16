// Fill out your copyright notice in the Description page of Project Settings.

#include "MotherAIActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Blaster/GameState/PickpackerGameState.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Blaster/AI/DroneActor.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

AMotherAIActor::AMotherAIActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

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

	// Start rest period interval timer
	if (HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(
			RestPeriodIntervalTimer,
			this,
			&AMotherAIActor::OnRestPeriodIntervalTimerFinished,
			RestPeriodInterval,
			false
		);
	}

	// Initial state update
	UpdateAIState();
}

void AMotherAIActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority())
	{
		return;
	}

	// 플레이어에게 접근 중이면 업데이트
	if (bIsApproachingPlayer && TargetPlayer.IsValid())
	{
		UpdateApproach(DeltaTime);
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

void AMotherAIActor::OnRestPeriodIntervalTimerFinished()
{
	StartRestPeriod();
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

	ABlasterPlayerState* PlayerState = BlasterCharacter->GetPlayerState<ABlasterPlayerState>();
	if (!PlayerState || !PlayerState->IsSuspicionMaxed())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MotherAIActor] Requesting punishment for player: %s"), *Player->GetName());

	// 플레이어에게 접근 시작
	TargetPlayer = Player;
	bIsApproachingPlayer = true;

	// 경고 메시지
	FString WarningMessage = FString::Printf(TEXT("경고: %s의 의심 수치가 최대치에 도달했습니다. 제제를 시행합니다."), 
		*BlasterCharacter->GetName());
	SendWarning(WarningMessage);
}

void AMotherAIActor::UpdateApproach(float DeltaTime)
{
	if (!TargetPlayer.IsValid())
	{
		bIsApproachingPlayer = false;
		return;
	}

	FVector CurrentLocation = GetActorLocation();
	FVector TargetLocation = TargetPlayer->GetActorLocation();
	FVector Direction = (TargetLocation - CurrentLocation).GetSafeNormal();
	float Distance = FVector::Dist(CurrentLocation, TargetLocation);

	// 제제 거리 내에 도달했으면 제제 실행
	if (Distance <= PunishmentDistance)
	{
		ExecutePunishment(TargetPlayer.Get());
		bIsApproachingPlayer = false;
		TargetPlayer = nullptr;
		return;
	}

	// 플레이어에게 접근
	FVector NewLocation = CurrentLocation + Direction * ApproachSpeed * DeltaTime;
	SetActorLocation(NewLocation);

	// 플레이어를 바라보기
	FRotator LookAtRotation = FRotationMatrix::MakeFromX(Direction).Rotator();
	SetActorRotation(LookAtRotation);
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
}


