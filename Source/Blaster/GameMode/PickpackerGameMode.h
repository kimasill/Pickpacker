// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Blaster/PickpackerTypes/PickpackerTypes.h"
#include "Blaster/DataAssets/DA_LevelVariant.h"
#include "PickpackerGameMode.generated.h"

class UAnchorRuntimeSubsystem;
class APlayerState;

/**
 * Pickpacker Game Mode - Manages the warehouse simulation game
 * Players are robots working under "Mother" AI surveillance
 */
UCLASS()
class BLASTER_API APickpackerGameMode : public AGameModeBase
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
	void InitializeLevel(UDA_LevelVariant* LevelVariantData, int32 Seed = 0);

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
	UDA_LevelVariant* GetCurrentLevelVariant() const { return CurrentLevelVariant; }

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

protected:
	/**
	 * Called when level initialization is complete
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pickpacker|Level")
	void OnLevelInitialized();

	/**
	 * Called when simulation starts
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pickpacker|Gameplay")
	void OnSimulationStarted();

	/**
	 * Called when simulation ends
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pickpacker|Gameplay")
	void OnSimulationEnded();

private:
	/** Current level variant data */
	UPROPERTY()
	UDA_LevelVariant* CurrentLevelVariant = nullptr;

	/** Current random seed */
	UPROPERTY()
	int32 CurrentSeed = 0;

	/** Whether simulation is currently running */
	UPROPERTY()
	bool bSimulationRunning = false;

	/** Anchor runtime subsystem reference */
	UPROPERTY()
	UAnchorRuntimeSubsystem* AnchorSubsystem = nullptr;
};