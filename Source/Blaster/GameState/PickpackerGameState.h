// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Blaster/Subsystem/PCGDungeonSubSystem.h"
#include "PickpackerGameState.generated.h"

/**
 * Pickpacker Game State - Manages seed replication and dungeon state
 */
UCLASS()
class BLASTER_API APickpackerGameState : public AGameState
{
	GENERATED_BODY()

public:
	APickpackerGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/**
	 * Set the seed set for dungeon generation (Server Only)
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker")
	void SetSeedSet(const FSeedSet& NewSeedSet);

	/**
	 * Get current seed set
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker")
	const FSeedSet& GetSeedSet() const { return SeedSet; }

	/**
	 * Check if dungeon has been generated
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker")
	bool IsDungeonGenerated() const { return bDungeonGenerated; }

	/**
	 * Get PCG Dungeon Subsystem
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker")
	UPCGDungeonSubSystem* GetPCGDungeonSubsystem();

protected:
	/**
	 * Called when seed set is replicated to clients
	 */
	UFUNCTION()
	void OnRep_SeedSet();

	/**
	 * Called when dungeon generation status changes
	 */
	UFUNCTION()
	void OnRep_DungeonGenerated();

private:
	/**
	 * Replicated seed set for dungeon generation
	 */
	UPROPERTY(ReplicatedUsing = OnRep_SeedSet)
	FSeedSet SeedSet;

	/**
	 * Whether dungeon has been generated
	 */
	UPROPERTY(ReplicatedUsing = OnRep_DungeonGenerated)
	bool bDungeonGenerated = false;

	/**
	 * Cached PCG subsystem reference
	 */
	UPROPERTY()
	UPCGDungeonSubSystem* PCGDungeonSubsystem = nullptr;
};

