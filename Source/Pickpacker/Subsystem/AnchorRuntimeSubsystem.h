// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "PickpackerTypes/CoreLoopTypes.h"
#include "AnchorRuntimeSubsystem.generated.h"

class UDA_LevelVariant;
class AActor;

/** Wrapper for actor arrays to be used as TMap values in UPROPERTY */
USTRUCT(BlueprintType)
struct FTaggedActors
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<AActor>> Actors;
};

/**
 * Anchor Runtime Subsystem - Manages anchor scanning and randomization for fixed levels
 * Replaces PCG system with tag-based anchor scanning and random placement
 */
UCLASS()
class PICKPACKER_API UAnchorRuntimeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UAnchorRuntimeSubsystem();

	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Initialize the anchor system with level variant data
	 * @param LevelVariantData - Data asset containing anchor groups and rules
	 * @param Seed - Random seed for deterministic generation
	 */
	UFUNCTION(BlueprintCallable, Category = "Anchor System")
	void InitializeAnchors(UDA_LevelVariant* LevelVariantData, int32 Seed = 0);

	/**
	 * Scan all anchors in the level and perform randomization
	 * Server-only operation that replicates results to clients
	 */
	UFUNCTION(BlueprintCallable, Category = "Anchor System")
	void ScanAndRandomizeAnchors();

	UFUNCTION(BlueprintCallable, Category = "Anchor System")
	void ApplyRouteSelectionResult(const FRouteSelectionResult& InRouteSelectionResult);

	/**
	 * Get all actors with specific anchor tags
	 * @param AnchorTag - Gameplay tag to search for
	 * @param OutActors - Array to populate with found actors
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Anchor System")
	void GetAnchorsByTag(const FGameplayTag& AnchorTag, TArray<AActor*>& OutActors) const;

	/**
	 * Check if anchor system is initialized
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Anchor System")
	bool IsInitialized() const { return bInitialized; }

	/**
	 * Get current seed used for randomization
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Anchor System")
	int32 GetCurrentSeed() const { return CurrentSeed; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Anchor System")
	const FRouteSelectionResult& GetCurrentRouteSelectionResult() const { return CurrentRouteSelectionResult; }

protected:
	/**
	 * Internal function to scan level for anchor actors
	 */
	void ScanLevelForAnchors();

	/**
	 * Perform randomization based on level variant rules
	 */
	void PerformRandomization();

	/**
	 * Apply randomization results to actors
	 */
	void ApplyRandomizationResults();

private:
	/** Whether the system is initialized */
	UPROPERTY()
	bool bInitialized = false;

	/** Current random seed */
	UPROPERTY()
	int32 CurrentSeed = 0;

	/** Level variant data asset */
	UPROPERTY()
	UDA_LevelVariant* LevelVariantData = nullptr;

	/** All anchor actors found in the level */
	UPROPERTY()
	TArray<TObjectPtr<AActor>> AllAnchorActors;

	/** Anchors grouped by tag */
	UPROPERTY()
	TMap<FGameplayTag, FTaggedActors> AnchorsByTag;

	/** Randomization results */
	UPROPERTY()
	TMap<TObjectPtr<AActor>, bool> ActivationResults;

	/** Randomization results */
	UPROPERTY()
	TMap<TObjectPtr<AActor>, TObjectPtr<AActor>> ReplacementResults;

	UPROPERTY()
	FRouteSelectionResult CurrentRouteSelectionResult;
};
