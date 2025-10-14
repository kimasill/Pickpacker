// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Blaster/DataTables/PickpackerDataTables.h"
#include "PCGAnchorSystem.generated.h"

/**
 * PCG Anchor System for managing spawn points and objectives
 */
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API UPCGAnchorSystem : public UObject
{
	GENERATED_BODY()

public:
	UPCGAnchorSystem();

	/**
	 * Initialize the anchor system with data tables
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG Anchor")
	void Initialize(UDataTable* ObjectivesTable, UDataTable* SpawnersTable);

	/**
	 * Set data tables
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG Anchor")
	void SetDataTables(UDataTable* ObjectivesTable, UDataTable* SpawnersTable);

	/**
	 * Set the world context used for scanning/spawning
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG Anchor")
	void SetWorldContext(UWorld* InWorld);

	/**
	 * Process PCG anchors and spawn actors
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG Anchor")
	bool ProcessAnchors(const TArray<FPCGAnchorData>& Anchors);

	/**
	 * Validate anchor placement (minimum requirements)
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG Anchor")
	bool ValidateAnchorPlacement(const TArray<FPCGAnchorData>& Anchors);

	/**
	 * Scan world for actors spawned by PCG graph and build anchor data from tags
	 * Expected tags:
	 *  - PCG.Anchor.Objective
	 *  - PCG.Anchor.Extract
	 *  - PCG.Spawner.Enemy / PCG.Spawner.Hazard / PCG.Spawner.Item
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG Anchor")
	TArray<FPCGAnchorData> ScanWorldForPCGTags() const;

	/**
	 * Get objective count by type
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PCG Anchor")
	int32 GetObjectiveCount(EPCGAnchorType AnchorType) const;

	/**
	 * Get spawner count by type
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PCG Anchor")
	int32 GetSpawnerCount(EPCGAnchorType AnchorType) const;

protected:
	/**
	 * Spawn objective actor
	 */
	AActor* SpawnObjectiveActor(const FPCGAnchorData& Anchor, const FObjectiveRow& Row);

	/**
	 * Spawn spawner actor
	 */
	AActor* SpawnSpawnerActor(const FPCGAnchorData& Anchor, const FSpawnerRow& Row);

	/**
	 * Log validation results
	 */
	void LogValidationResults(const TArray<FPCGAnchorData>& Anchors);

private:
	UPROPERTY()
	UDataTable* ObjectivesDataTable = nullptr;

	UPROPERTY()
	UDataTable* SpawnersDataTable = nullptr;

	UPROPERTY()
	UWorld* World = nullptr;

	/**
	 * If true, this system will spawn runtime actors from anchors. If false (default),
	 * it will not spawn and will assume PCG graph already spawned them; only validation/logging occurs.
	 */
	UPROPERTY(EditAnywhere, Category = "PCG Anchor")
	bool bAllowRuntimeSpawnBySystem = false;

	// Tracking counters
	int32 ObjectiveCount = 0;
	int32 ExtractCount = 0;
	int32 EnemySpawnCount = 0;
	int32 HazardSpawnCount = 0;
	int32 ItemSpawnCount = 0;

	// Validation flags
	bool bHasMinimumObjectives = false;
	bool bHasMinimumExtracts = false;
};


