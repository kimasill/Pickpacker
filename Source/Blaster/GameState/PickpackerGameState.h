// Copyright notice
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Blaster/PickpackerTypes/PickpackerTypes.h"
#include "PickpackerGameState.generated.h"

class UPCGDungeonSubSystem;

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

	/** Set the seed set for dungeon generation (Server Only) */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker")
	void SetSeedSet(const FSeedSet& NewSeedSet);

	/** Get current seed set */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker")
	const FSeedSet& GetSeedSet() const { return SeedSet; }

	/** Check if dungeon has been generated */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker")
	bool IsDungeonGenerated() const { return bDungeonGenerated; }

	/** Get PCG Dungeon Subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker")
	UPCGDungeonSubSystem* GetPCGDungeonSubSystem();

	// Server: signal clients to run local PCG now (temporary dev flow)
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|PCG")
	void TriggerClientPCGRun();

	// Readiness helpers
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|PCG")
	int32 GetClientsPCGReadyCount() const { return ClientsPCGReadyCount; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|PCG")
	int32 GetExpectedClientCount() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|PCG")
	int32 GetExpectedClientCountSnapshot() const { return ExpectedClientCountSnapshot; }

	/**
	 * Server-side: handle a client reporting PCG ready.
	 * Increments count, finalizes PCG if all ready, and starts gameplay.
	 */
	UFUNCTION()
	void HandleClientPCGReadyFor(class APlayerState* ReportingPS);

	// Backward-compatible path (deprecated): keep for callers not passing PS
	UFUNCTION()
	void HandleClientPCGReady();

public:
	// Broadcast when all clients reported PCG ready (temporary dev flow)
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllClientsPCGReady);
	UPROPERTY(BlueprintAssignable, Category = "Pickpacker|PCG")
	FOnAllClientsPCGReady OnAllClientsPCGReady;

protected:
	/** Called when seed set is replicated to clients */
	UFUNCTION()
	void OnRep_SeedSet();

	/** Called when dungeon generation status changes */
	UFUNCTION()
	void OnRep_DungeonGenerated();

	// On clients: when PCG run counter changes, run local PCG once
	UFUNCTION()
	void OnRep_PCGRunCounter();

private:
	/** Replicated seed set for dungeon generation */
	UPROPERTY(ReplicatedUsing = OnRep_SeedSet)
	FSeedSet SeedSet;

	/** Whether dungeon has been generated */
	UPROPERTY(ReplicatedUsing = OnRep_DungeonGenerated)
	bool bDungeonGenerated = false;

	// Bumps each time server requests clients to run PCG
	UPROPERTY(ReplicatedUsing = OnRep_PCGRunCounter)
	int32 PCGRunCounter = 0;

public:
	// Server-side: number of clients reported ready (public for direct access in RPC)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Pickpacker|PCG")
	int32 ClientsPCGReadyCount = 0;

private:

	/** Cached PCG subsystem reference */
	UPROPERTY()
	UPCGDungeonSubSystem* PCGDungeonSubSystem = nullptr;

	// Avoid double finalization/spawn
	UPROPERTY()
	bool bAllClientsPCGReadyTriggered = false;

	// Track which players have reported, to avoid double counting
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<APlayerState>> ReadyPlayers;

	// Snapshot of expected client count taken when triggering a PCG run on the server
	UPROPERTY(Transient)
	int32 ExpectedClientCountSnapshot = -1;

	friend class APickpackerPlayerController;
	friend class APickpackerPlayerState;
};
