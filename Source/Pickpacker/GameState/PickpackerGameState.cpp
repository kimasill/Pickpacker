// Fill out your copyright notice in the Description page of Project Settings.

#include "PickpackerGameState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Subsystem/AnchorRuntimeSubsystem.h"
#include "GameMode/PickpackerGameMode.h"

APickpackerGameState::APickpackerGameState()
{
	LevelVariant = nullptr;
	RandomSeed = 0;
	TeamSuspicion = 0.0f;
	bSimulationRunning = false;
	MaxSuspicion = 100.0f;
	AnchorSubsystem = nullptr;
	GameStartTime = 0.0f;
	GameTimeSpeed = 1.0f;
	DayLengthInSeconds = 1440.0f; // 24분 = 하루
	PrimaryActorTick.bCanEverTick = true; // 게임 시간 업데이트를 위해 Tick 활성화
	TeamCredits = 0;
	LastReplicatedTeamCredits = 0;

	EscapeProgressComponent = CreateDefaultSubobject<UEscapeProgressComponent>(TEXT("EscapeProgressComponent"));
}

void APickpackerGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APickpackerGameState, LevelVariant);
	DOREPLIFETIME(APickpackerGameState, RandomSeed);
	DOREPLIFETIME(APickpackerGameState, TeamSuspicion);
	DOREPLIFETIME(APickpackerGameState, bSimulationRunning);
	DOREPLIFETIME(APickpackerGameState, ActiveOrders);
	DOREPLIFETIME(APickpackerGameState, CurrentOrderWaveNumber);
	DOREPLIFETIME(APickpackerGameState, TeamCredits);
	DOREPLIFETIME(APickpackerGameState, OrderTimesPayloads);
	DOREPLIFETIME(APickpackerGameState, CoreLoopPhase);
	DOREPLIFETIME(APickpackerGameState, CompletedTrips);
}

void APickpackerGameState::BeginPlay()
{
	Super::BeginPlay();

	// Get anchor subsystem reference
	UWorld* World = GetWorld();
	if (World)
	{
		AnchorSubsystem = World->GetSubsystem<UAnchorRuntimeSubsystem>();
		// 게임 시작 시간 기록
		GameStartTime = World->GetTimeSeconds();
	}

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] BeginPlay - Anchor subsystem: %s, GameStartTime: %.2f"), 
		AnchorSubsystem ? TEXT("Found") : TEXT("Not Found"), GameStartTime);
}

void APickpackerGameState::SetLevelVariant(UDA_LevelVariant* NewLevelVariant)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameState] SetLevelVariant called without authority"));
		return;
	}

	LevelVariant = NewLevelVariant;
	
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Level variant set: %s"), 
		LevelVariant ? *LevelVariant->LevelName : TEXT("None"));
}

void APickpackerGameState::SetRandomSeed(int32 NewSeed)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameState] SetRandomSeed called without authority"));
		return;
	}

	RandomSeed = NewSeed;
	
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Random seed set: %d"), RandomSeed);
}

void APickpackerGameState::AddTeamSuspicion(float SuspicionPoints)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameState] AddTeamSuspicion called without authority"));
		return;
	}

	float OldSuspicion = TeamSuspicion;
	TeamSuspicion = FMath::Clamp(TeamSuspicion + SuspicionPoints, 0.0f, MaxSuspicion);

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Team suspicion: %.2f -> %.2f (+%.2f)"), 
		OldSuspicion, TeamSuspicion, SuspicionPoints);

	// Broadcast suspicion change
	OnSuspicionChanged.Broadcast(GetSuspicionLevel());
}

float APickpackerGameState::GetSuspicionLevel() const
{
	return FMath::Clamp(TeamSuspicion / MaxSuspicion, 0.0f, 1.0f);
}

void APickpackerGameState::SetSimulationRunning(bool bRunning)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameState] SetSimulationRunning called without authority"));
		return;
	}

	bSimulationRunning = bRunning;
	
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Simulation running: %s"), 
		bRunning ? TEXT("True") : TEXT("False"));

	// Broadcast simulation state change
	OnSimulationStateChanged.Broadcast(bRunning);
}

void APickpackerGameState::SetActiveOrders(const TArray<FActiveOrderState>& NewOrders)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameState] SetActiveOrders called without authority"));
		return;
	}

	ActiveOrders = NewOrders;
	OnOrdersUpdated.Broadcast(ActiveOrders);

	EnsureOrderTimesUpdateTimer();
}

void APickpackerGameState::SetCurrentOrderWaveNumber(int32 NewWaveNumber)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameState] SetCurrentOrderWaveNumber called without authority"));
		return;
	}

	CurrentOrderWaveNumber = FMath::Max(0, NewWaveNumber);
	OnOrderWaveStarted.Broadcast(CurrentOrderWaveNumber);
}

void APickpackerGameState::SetTeamCredits(int32 NewCredits)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameState] SetTeamCredits called without authority"));
		return;
	}

	const int32 OldCredits = TeamCredits;
	TeamCredits = FMath::Max(0, NewCredits);
	LastReplicatedTeamCredits = TeamCredits;

	OnCreditsChanged.Broadcast(TeamCredits, TeamCredits - OldCredits);

	if (TeamCredits <= 0 && OldCredits > 0)
	{
		if (APickpackerGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APickpackerGameMode>() : nullptr)
		{
			GameMode->OnGameOver(TEXT("Team credits depleted"));
		}
	}
}

void APickpackerGameState::ApplyCreditDelta(int32 Delta, const FString& Reason)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameState] ApplyCreditDelta called without authority"));
		return;
	}

	if (Delta == 0)
	{
		return;
	}

	const int32 OldCredits = TeamCredits;
	TeamCredits = FMath::Max(0, TeamCredits + Delta);
	LastReplicatedTeamCredits = TeamCredits;

	FCreditTransaction Transaction;
	Transaction.TransactionId = FGuid::NewGuid();
	Transaction.Delta = Delta;
	Transaction.BalanceAfter = TeamCredits;
	Transaction.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Transaction.Reason = FText::FromString(Reason);
	CreditHistory.Add(Transaction);

	while (CreditHistory.Num() > MaxStoredCreditTransactions)
	{
		CreditHistory.RemoveAt(0);
	}

	OnCreditsChanged.Broadcast(TeamCredits, TeamCredits - OldCredits);

	if (TeamCredits <= 0 && OldCredits > 0)
	{
		if (APickpackerGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APickpackerGameMode>() : nullptr)
		{
			GameMode->OnGameOver(TEXT("Team credits depleted"));
		}
	}
}

UAnchorRuntimeSubsystem* APickpackerGameState::GetAnchorSubsystem()
{
	if (!AnchorSubsystem && GetWorld())
	{
		AnchorSubsystem = GetWorld()->GetSubsystem<UAnchorRuntimeSubsystem>();
	}
	return AnchorSubsystem;
}

void APickpackerGameState::OnRep_LevelVariant()
{
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Level variant replicated: %s"), 
		LevelVariant ? *LevelVariant->LevelName : TEXT("None"));
}

void APickpackerGameState::OnRep_RandomSeed()
{
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Random seed replicated: %d"), RandomSeed);
}

void APickpackerGameState::OnRep_TeamSuspicion()
{
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Team suspicion replicated: %.2f"), TeamSuspicion);
	
	// Broadcast suspicion change
	OnSuspicionChanged.Broadcast(GetSuspicionLevel());
}

void APickpackerGameState::OnRep_SimulationRunning()
{
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Simulation running replicated: %s"), 
		bSimulationRunning ? TEXT("True") : TEXT("False"));

	// Broadcast simulation state change
	OnSimulationStateChanged.Broadcast(bSimulationRunning);
}

void APickpackerGameState::OnRep_ActiveOrders()
{
	OnOrdersUpdated.Broadcast(ActiveOrders);
	// Clients also update remaining times for UI responsiveness
	BroadcastOrderRemainingTimes();
}

void APickpackerGameState::OnRep_CurrentOrderWaveNumber()
{
	OnOrderWaveStarted.Broadcast(CurrentOrderWaveNumber);
}

void APickpackerGameState::OnRep_TeamCredits()
{
	const int32 Delta = TeamCredits - LastReplicatedTeamCredits;
	LastReplicatedTeamCredits = TeamCredits;
	OnCreditsChanged.Broadcast(TeamCredits, Delta);
}

void APickpackerGameState::OnRep_OrderTimesPayloads()
{
	OnOrderTimesUpdated.Broadcast(OrderTimesPayloads);
}

float APickpackerGameState::GetCurrentGameHour() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	float ElapsedRealSeconds = (World->GetTimeSeconds() - GameStartTime) * GameTimeSpeed;
	float ElapsedGameDays = ElapsedRealSeconds / DayLengthInSeconds;
	float CurrentGameHour = FMath::Fmod(ElapsedGameDays * 24.0f, 24.0f);
	
	return CurrentGameHour;
}

float APickpackerGameState::ConvertGameHoursToRealSeconds(float GameHours) const
{
	// 게임 시간 1시간 = 실제 시간 (DayLengthInSeconds / 24.0) 초
	return GameHours * (DayLengthInSeconds / 24.0f) / GameTimeSpeed;
}

float APickpackerGameState::ConvertRealSecondsToGameHours(float RealSeconds) const
{
	// 실제 시간을 게임 시간으로 변환
	return (RealSeconds * GameTimeSpeed) / (DayLengthInSeconds / 24.0f);
}

void APickpackerGameState::SetGameTimeSpeed(float Speed)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameState] SetGameTimeSpeed called without authority"));
		return;
	}

	GameTimeSpeed = FMath::Max(0.0f, Speed);
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Game time speed set to %.2f"), GameTimeSpeed);
}

void APickpackerGameState::SetDayLengthInSeconds(float Seconds)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameState] SetDayLengthInSeconds called without authority"));
		return;
	}

	DayLengthInSeconds = FMath::Max(1.0f, Seconds);
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Day length set to %.2f seconds"), DayLengthInSeconds);
}

void APickpackerGameState::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APickpackerGameState::SetCoreLoopPhase(ECoreLoopPhase NewPhase)
{
	if (!HasAuthority())
	{
		return;
	}

	if (CoreLoopPhase == NewPhase)
	{
		return;
	}

	const ECoreLoopPhase OldPhase = CoreLoopPhase;
	CoreLoopPhase = NewPhase;

	OnCoreLoopPhaseChanged.Broadcast(OldPhase, NewPhase);

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] CoreLoopPhase: %d -> %d"),
		static_cast<int32>(OldPhase), static_cast<int32>(NewPhase));
}

void APickpackerGameState::SetCompletedTrips(int32 NewTrips)
{
	if (!HasAuthority())
	{
		return;
	}
	CompletedTrips = FMath::Max(0, NewTrips);
}

void APickpackerGameState::OnRep_CoreLoopPhase(ECoreLoopPhase OldPhase)
{
	OnCoreLoopPhaseChanged.Broadcast(OldPhase, CoreLoopPhase);
}

void APickpackerGameState::EnsureOrderTimesUpdateTimer()
{
	if (!HasAuthority()) return;
	UWorld* World = GetWorld();
	if (!World) return;

	if (ActiveOrders.Num() == 0)
	{
		StopOrderTimesUpdateTimer();
		return;
	}

	FTimerManager& TM = World->GetTimerManager();
	if (!TM.IsTimerActive(OrderTimesUpdateTimerHandle))
	{
		TM.SetTimer(OrderTimesUpdateTimerHandle, this, &APickpackerGameState::BroadcastOrderRemainingTimes, FMath::Max(0.05f, OrderTimesUpdateInterval), true);
	}
}

void APickpackerGameState::StopOrderTimesUpdateTimer()
{
	if (!HasAuthority()) return;
	UWorld* World = GetWorld();
	if (!World) return;
	World->GetTimerManager().ClearTimer(OrderTimesUpdateTimerHandle);
}

void APickpackerGameState::BroadcastOrderRemainingTimes()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FOrderTimesUpdatePayload Payload;
	const float Now = World->GetTimeSeconds();
	bool bHasActive = false;
	OrderTimesPayloads.Empty();

	for (const FActiveOrderState& Order : ActiveOrders)
	{
		if (Order.bCompleted || Order.bFailed)
		{
			continue;
		}
		float Remaining = -1.f;
		if (Order.ExpireTime > 0.f)
		{
			Remaining = FMath::Max(0.f, Order.ExpireTime - Now);
		}
		Payload.RemainingSeconds = Remaining;
		Payload.OrderId = Order.OrderId; 
		OrderTimesPayloads.Add(Payload);
		bHasActive = true;
	}
	if (!bHasActive)
	{
		StopOrderTimesUpdateTimer();
	}
	OnOrderTimesUpdated.Broadcast(OrderTimesPayloads);
}
