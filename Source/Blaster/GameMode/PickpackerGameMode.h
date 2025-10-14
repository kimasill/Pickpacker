// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Blaster/Subsystem/PCGDungeonSubSystem.h"
#include "PickpackerGameMode.generated.h"

/**
 * Pickpacker Game Mode - Handles match start and PCG generation
 */
UCLASS()
class BLASTER_API APickpackerGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	APickpackerGameMode();

	virtual void BeginPlay() override;
	virtual void OnMatchStateSet() override;

	/**
	 * Handle match start - Generate dungeon and start gameplay
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker")
	void HandleMatchStart();

	/**
	 * Set mission configuration
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker")
	void SetMissionConfig(const FString& MissionId, int32 CustomSeed = 0);

	/**
	 * Get current mission configuration
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker")
	const FSeedSet& GetMissionConfig() const { return CurrentMissionConfig; }

protected:
	/**
	 * Called when PCG generation completes
	 */
	UFUNCTION()
	void OnPCGGenerationComplete(bool bSuccess);

	/**
	 * Start the actual gameplay after PCG generation
	 */
	void StartGameplay();

private:
	/**
	 * Current mission configuration
	 */
	UPROPERTY()
	FSeedSet CurrentMissionConfig;

	/**
	 * Whether PCG generation is in progress
	 */
	UPROPERTY()
	bool bPCGGenerationInProgress = false;

	/**
	 * Cached PCG subsystem reference
	 */
	UPROPERTY()
	UPCGDungeonSubSystem* PCGDungeonSubsystem = nullptr;

	/**
	 * Cached game state reference
	 */
	UPROPERTY()
	class APickpackerGameState* PickpackerGameState = nullptr;
};


