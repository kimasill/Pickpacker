// Fill out your copyright notice in the Description page of Project Settings.

#include "PickpackerGameMode.h"
#include "Blaster/GameState/PickpackerGameState.h"
#include "Blaster/Escape/EscapeZoneActor.h"
#include "Engine/World.h"
#include "Blaster/DataAssets/DA_LevelVariant.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

APickpackerGameMode::APickpackerGameMode()
{
	GameStateClass = APickpackerGameState::StaticClass();
	CurrentMissionConfig = FSeedSet();
}

void APickpackerGameMode::BeginPlay()
{
	Super::BeginPlay();

	PickpackerGameState = Cast<APickpackerGameState>(GameState);

	// Subscribe to suspicion changes
	if (PickpackerGameState)
	{
		PickpackerGameState->OnSuspicionChanged.AddDynamic(this, &APickpackerGameMode::OnSuspicionChanged);
	}
}

void APickpackerGameMode::OnMatchStateSet()
{
	Super::OnMatchStateSet();
	if (MatchState == MatchState::InProgress)
	{
		HandleMatchStart();
	}
}

void APickpackerGameMode::HandleMatchStart()
{
	if (!HasAuthority()) return;
	if (bPCGGenerationInProgress) return;
	if (!PickpackerGameState) return;

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Starting match with mission: %s, seed: %d"),
		*CurrentMissionConfig.MissionId, CurrentMissionConfig.Seed);

	// Replicate seed via game state
	PickpackerGameState->SetRandomSeed(CurrentMissionConfig.Seed);

	bPCGGenerationInProgress = true;

	// If PCG subsystem exists in this project, trigger it here. Otherwise proceed immediately.
	// if (PCGDungeonSubsystem) { PCGDungeonSubsystem->GenerateDungeon(CurrentMissionConfig); }

	OnPCGGenerationComplete(true);
}

void APickpackerGameMode::SetMissionConfig(const FString& MissionId, int32 CustomSeed)
{
	if (!HasAuthority()) return;
	CurrentMissionConfig.MissionId = MissionId;
	CurrentMissionConfig.Seed = CustomSeed > 0 ? CustomSeed : FMath::RandRange(1, 999999);

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Mission config set - MissionId: %s, Seed: %d"),
		*CurrentMissionConfig.MissionId, CurrentMissionConfig.Seed);
}

void APickpackerGameMode::OnPCGGenerationComplete(bool bSuccess)
{
	bPCGGenerationInProgress = false;
	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] PCG generation successful - starting gameplay"));
		StartGameplay();
	}
}

void APickpackerGameMode::StartGameplay()
{
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Gameplay started"));

	// Start checking game end conditions periodically
	if (HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(
			GameEndCheckTimer,
			this,
			&APickpackerGameMode::CheckGameEndConditions,
			1.0f,
			true
		);
	}
}

void APickpackerGameMode::OnPlayerEscaped(ACharacter* Player)
{
	if (!HasAuthority() || !Player)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Player escaped: %s"), *Player->GetName());

	// Check if all players escaped
	CheckGameEndConditions();
}

void APickpackerGameMode::OnAllPlayersEscaped()
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] All players escaped - VICTORY!"));

	// End game with victory
	OnGameOver(TEXT("All players escaped!"));
}

void APickpackerGameMode::OnGameOver(const FString& Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Game Over: %s"), *Reason);

	// Stop game end check timer
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(GameEndCheckTimer);
	}

	// End simulation
	EndWarehouseSimulation();

	// Broadcast game over event (can be handled by UI)
	OnGameOverEvent.Broadcast(Reason);
}

void APickpackerGameMode::CheckGameEndConditions()
{
	if (!HasAuthority() || !PickpackerGameState)
	{
		return;
	}

	// Check if suspicion reached maximum
	float SuspicionLevel = PickpackerGameState->GetSuspicionLevel();
	if (SuspicionLevel >= 1.0f)
	{
		OnGameOver(TEXT("Suspicion reached maximum!"));
		return;
	}

	// Check if all players escaped (handled by EscapeZoneActor)
	// This is checked when players escape, not here
}

void APickpackerGameMode::OnSuspicionChanged(float NewSuspicionLevel)
{
	if (!HasAuthority())
	{
		return;
	}

	// Check if suspicion reached maximum
	if (NewSuspicionLevel >= 1.0f)
	{
		OnGameOver(TEXT("Suspicion reached maximum!"));
	}
}
// ===== Implementations required by header (to fix LNK2019) =====
void APickpackerGameMode::InitializeLevel(UDA_LevelVariant* LevelVariantData, int32 Seed)
{
	if (!HasAuthority()) return;
	CurrentLevelVariant = LevelVariantData;
	CurrentSeed = (Seed > 0) ? Seed : FMath::RandRange(1, 999999);

	if (APickpackerGameState* GS = Cast<APickpackerGameState>(GameState))
	{
		GS->SetLevelVariant(LevelVariantData);
		GS->SetRandomSeed(CurrentSeed);
	}

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] InitializeLevel done. Seed=%d, Variant=%s"), CurrentSeed, LevelVariantData ? *LevelVariantData->GetName() : TEXT("None"));
}

void APickpackerGameMode::StartWarehouseSimulation()
{
	if (!HasAuthority()) return;
	if (APickpackerGameState* GS = Cast<APickpackerGameState>(GameState))
	{
		GS->SetSimulationRunning(true);
	}
	bSimulationRunning = true;
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Simulation started"));
}

void APickpackerGameMode::EndWarehouseSimulation()
{
	if (!HasAuthority()) return;
	if (APickpackerGameState* GS = Cast<APickpackerGameState>(GameState))
	{
		GS->SetSimulationRunning(false);
	}
	bSimulationRunning = false;
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Simulation ended"));
}

void APickpackerGameMode::RegisterClientPCGReady(APlayerState* PlayerState)
{
	const TCHAR* NameOrNone = TEXT("None");
	if (PlayerState)
	{
		const FString& Nm = PlayerState->GetPlayerName();
		NameOrNone = *Nm;
	}
	UE_LOG(LogTemp, Verbose, TEXT("[PickpackerGameMode] Client PCG ready reported: %s"), NameOrNone);
}
