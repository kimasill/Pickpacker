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
}

void APickpackerGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APickpackerGameState, LevelVariant);
	DOREPLIFETIME(APickpackerGameState, RandomSeed);
	DOREPLIFETIME(APickpackerGameState, TeamSuspicion);
	DOREPLIFETIME(APickpackerGameState, bSimulationRunning);
}

void APickpackerGameState::BeginPlay()
{
	Super::BeginPlay();

	// Get anchor subsystem reference
	UWorld* World = GetWorld();
	if (World)
	{
		AnchorSubsystem = World->GetSubsystem<UAnchorRuntimeSubsystem>();
	}

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] BeginPlay - Anchor subsystem: %s"), 
		AnchorSubsystem ? TEXT("Found") : TEXT("Not Found"));
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