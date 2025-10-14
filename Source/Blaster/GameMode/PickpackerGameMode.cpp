// Fill out your copyright notice in the Description page of Project Settings.

#include "PickpackerGameMode.h"
#include "../GameState/PickpackerGameState.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

APickpackerGameMode::APickpackerGameMode()
{
	// Set default game state class
	GameStateClass = APickpackerGameState::StaticClass();
	
	// Initialize with default mission config
	CurrentMissionConfig = FSeedSet();
}

void APickpackerGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Cache subsystem references
	PCGDungeonSubsystem = GetWorld()->GetSubsystem<UPCGDungeonSubSystem>();
	PickpackerGameState = Cast<APickpackerGameState>(GameState);

	if (!PCGDungeonSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[PickpackerGameMode] Failed to get PCG Dungeon Subsystem"));
	}

	// Optionally bind to generation complete (if subsystem fires it later)
	// if (PCGDungeonSubsystem)
	// {
	//     PCGDungeonSubsystem->OnPCGGenerationComplete.AddDynamic(this, &APickpackerGameMode::OnPCGGenerationComplete);
	// }
}

void APickpackerGameMode::OnMatchStateSet()
{
	Super::OnMatchStateSet();

	// Handle match start when match state changes to InProgress
	if (MatchState == MatchState::InProgress)
	{
		HandleMatchStart();
	}
}

void APickpackerGameMode::HandleMatchStart()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameMode] Only server can handle match start"));
		return;
	}

	if (bPCGGenerationInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameMode] PCG generation already in progress"));
		return;
	}

	if (!PCGDungeonSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[PickpackerGameMode] PCG Dungeon Subsystem not available"));
		return;
	}

	if (!PickpackerGameState)
	{
		UE_LOG(LogTemp, Error, TEXT("[PickpackerGameMode] Pickpacker Game State not available"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Starting match with mission: %s, seed: %d"), 
		*CurrentMissionConfig.MissionId, CurrentMissionConfig.Seed);

	// Set seed set in game state for replication
	PickpackerGameState->SetSeedSet(CurrentMissionConfig);

	// Start PCG generation
	bPCGGenerationInProgress = true;

	// Trigger PCG generation. The subsystem will execute the PCG graph (server-only)
	// and then scan world for PCG-tagged actors; it no longer spawns actors itself.
	PCGDungeonSubsystem->GenerateDungeon(CurrentMissionConfig);

	// For T1-01 immediate path, treat as sync success and proceed.
	OnPCGGenerationComplete(true);
}

void APickpackerGameMode::SetMissionConfig(const FString& MissionId, int32 CustomSeed)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameMode] Only server can set mission config"));
		return;
	}

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
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PickpackerGameMode] PCG generation failed - cannot start gameplay"));
		// TODO: Handle generation failure (restart, fallback, etc.)
	}
}

void APickpackerGameMode::StartGameplay()
{
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Starting Pickpacker gameplay"));

	// TODO: Implement actual gameplay start logic
	// - Enable player controls
	// - Start mission objectives
	// - Begin timers
	// - Enable UI elements
	
	// For now, just log that gameplay has started
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Gameplay started successfully"));
}

