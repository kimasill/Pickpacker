// Fill out your copyright notice in the Description page of Project Settings.

#include "PickpackerGameMode.h"
#include "Blaster/Subsystem/AnchorRuntimeSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

APickpackerGameMode::APickpackerGameMode()
{
	CurrentLevelVariant = nullptr;
	CurrentSeed = 0;
	bSimulationRunning = false;
	AnchorSubsystem = nullptr;
}

void APickpackerGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Get anchor subsystem reference
	UWorld* World = GetWorld();
	if (World)
	{
		AnchorSubsystem = World->GetSubsystem<UAnchorRuntimeSubsystem>();
	}

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] BeginPlay - Anchor subsystem: %s"), 
		AnchorSubsystem ? TEXT("Found") : TEXT("Not Found"));
}

void APickpackerGameMode::HandleMatchStart()
{
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] HandleMatchStart"));

	// Start warehouse simulation
	StartWarehouseSimulation();
}

void APickpackerGameMode::InitializeLevel(UDA_LevelVariant* LevelVariantData, int32 Seed)
{
	if (!LevelVariantData)
	{
		UE_LOG(LogTemp, Error, TEXT("[PickpackerGameMode] LevelVariantData is null"));
		return;
	}

	CurrentLevelVariant = LevelVariantData;
	CurrentSeed = Seed;

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Initializing level: %s with seed: %d"), 
		*LevelVariantData->LevelName, Seed);

	// Initialize anchor subsystem
	if (AnchorSubsystem)
	{
		AnchorSubsystem->InitializeAnchors(LevelVariantData, Seed);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[PickpackerGameMode] Anchor subsystem not found"));
	}

	// Notify Blueprint
	OnLevelInitialized();
}

void APickpackerGameMode::StartWarehouseSimulation()
{
	if (bSimulationRunning)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameMode] Simulation already running"));
		return;
	}

	bSimulationRunning = true;

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Starting warehouse simulation"));

	// Notify Blueprint
	OnSimulationStarted();
}

void APickpackerGameMode::EndWarehouseSimulation()
{
	if (!bSimulationRunning)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameMode] Simulation not running"));
		return;
	}

	bSimulationRunning = false;

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Ending warehouse simulation"));

	// Notify Blueprint
	OnSimulationEnded();
}

void APickpackerGameMode::RegisterClientPCGReady(APlayerState* PlayerState)
{
	if (!HasAuthority())
	{
		return;
	}

	if (PlayerState)
	{
		UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Client PCG ready: %s"), *PlayerState->GetPlayerName());
	}
}