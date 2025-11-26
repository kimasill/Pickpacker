// Fill out your copyright notice in the Description page of Project Settings.

#include "PickpackerGameState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Blaster/Subsystem/AnchorRuntimeSubsystem.h"

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
}

void APickpackerGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APickpackerGameState, LevelVariant);
	DOREPLIFETIME(APickpackerGameState, RandomSeed);
	DOREPLIFETIME(APickpackerGameState, TeamSuspicion);
	DOREPLIFETIME(APickpackerGameState, bSimulationRunning);
	DOREPLIFETIME(APickpackerGameState, ActiveOrders);
	DOREPLIFETIME(APickpackerGameState, TeamCredits);
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
}

void APickpackerGameState::OnRep_TeamCredits()
{
	const int32 Delta = TeamCredits - LastReplicatedTeamCredits;
	LastReplicatedTeamCredits = TeamCredits;
	OnCreditsChanged.Broadcast(TeamCredits, Delta);
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

	if(HasAuthority())
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}
		float CurrentGameHour = GetCurrentGameHour();
		int32 Hours = FMath::FloorToInt(CurrentGameHour);
		int32 Minutes = FMath::FloorToInt((CurrentGameHour - Hours) * 60.0f);

		// 실제 시간 2초에 한번
		
		if(FMath::Fmod(World->GetTimeSeconds(), 2.0f) < DeltaTime)
		{

			FString TimeString = FString::Printf(TEXT("Game Time: %02d:%02d"), Hours, Minutes);
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.1f, // 다음 프레임까지 유지
				FColor::Green,
				TimeString
			);
		}

	}
}
