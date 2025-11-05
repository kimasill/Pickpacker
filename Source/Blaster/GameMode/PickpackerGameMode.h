// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Blaster/PickpackerTypes/PickpackerTypes.h"
#include "PickpackerGameMode.generated.h"

class UAnchorRuntimeSubsystem;
class APlayerState;
class APickpackerGameState;
class UPCGDungeonSubSystem; // forward declaration

/**
 * Pickpacker Game Mode - Manages the warehouse simulation game
 * Players are robots working under "Mother" AI surveillance
 */
UCLASS()
class BLASTER_API APickpackerGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	APickpackerGameMode();

	virtual void BeginPlay() override;
	virtual void HandleMatchStart();

	/**
	 * Initialize the level with variant data
	 * @param LevelVariantData - Data asset containing level configuration
	 * @param Seed - Random seed for deterministic generation
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Level")
	void InitializeLevel(class UDA_LevelVariant* LevelVariantData, int32 Seed = 0);

	/**
	 * Start the warehouse simulation
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Gameplay")
	void StartWarehouseSimulation();

	/**
	 * End the warehouse simulation
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Gameplay")
	void EndWarehouseSimulation();

	/**
	 * Get current level variant data
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Level")
	class UDA_LevelVariant* GetCurrentLevelVariant() const { return CurrentLevelVariant; }

	/**
	 * Get current seed
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Level")
	int32 GetCurrentSeed() const { return CurrentSeed; }

	/**
	 * Check if simulation is running
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Gameplay")
	bool IsSimulationRunning() const { return bSimulationRunning; }

	/**
	 * Called by clients' PlayerState to signal PCG readiness (Server Only)
	 */
	void RegisterClientPCGReady(APlayerState* PlayerState);

	/**
	 * Set mission configuration
	 * @param MissionId - The ID of the mission
	 * @param CustomSeed - Custom seed for the mission (optional)
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Gameplay")
	void SetMissionConfig(const FString& MissionId, int32 CustomSeed = 0);

protected:
	/**
	 * Called when match state is set
	 */
	virtual void OnMatchStateSet() override;

	/**
	 * Called when PCG generation is complete
	 */
	void OnPCGGenerationComplete(bool bSuccess);

	/**
	 * Start gameplay after initialization
	 */
	void StartGameplay();

private:
	/** Current level variant data */
	UPROPERTY()
	class UDA_LevelVariant* CurrentLevelVariant = nullptr;

	/** Current random seed */
	UPROPERTY()
	int32 CurrentSeed = 0;

	/** Whether simulation is currently running */
	UPROPERTY()
	bool bSimulationRunning = false;

	/** Anchor runtime subsystem reference */
	UPROPERTY()
	UAnchorRuntimeSubsystem* AnchorSubsystem = nullptr;

	// Mission settings
	UPROPERTY()
	FSeedSet CurrentMissionConfig;

	// PCG subsystem (not reflected)
	UPCGDungeonSubSystem* PCGDungeonSubsystem = nullptr;

	// Cached game state
	UPROPERTY()
	APickpackerGameState* PickpackerGameState = nullptr;

	// Generation flag
	bool bPCGGenerationInProgress = false;
};