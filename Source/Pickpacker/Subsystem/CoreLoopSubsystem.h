// CoreLoopSubsystem - Manages the Run-based core gameplay loop
// Base (Orders & Packing) → Train (Destination Selection) → Underground (Farming & NPC) → Base

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PickpackerTypes/CoreLoopTypes.h"
#include "CoreLoopSubsystem.generated.h"

class APickpackerGameState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCoreLoopPhaseChanged, ECoreLoopPhase, OldPhase, ECoreLoopPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunStarted, const FRunState&, RunState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunEnded, const FRunState&, FinalRunState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTripCompleted, int32, TripCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrainDestinationSelected, const FTrainDestination&, Destination);

/**
 * Core Loop Subsystem – orchestrates the run-based game loop.
 *
 * Flow:
 *  1. StartRun() – initialises volatile run state
 *  2. Base Phase – order waves, packing, submission
 *  3. TransitionToTrain() – team votes on destination
 *  4. SelectTrainDestination() – loads underground zone
 *  5. TransitionToUnderground() – farming, NPC quests, combat
 *  6. ReturnToBase() – trains back, increments trip count
 *  7. Repeat 2-6 until escape conditions are met
 *  8. TransitionToEscape() – final gate / ending evaluation
 *
 * This subsystem is server-authoritative; clients react via
 * replicated GameState phase and delegates.
 */
UCLASS()
class PICKPACKER_API UCoreLoopSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UCoreLoopSubsystem();

	// --- USubsystem interface --------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// --- Run lifecycle ---------------------------------------------------

	/** Start a new run (server only) */
	UFUNCTION(BlueprintCallable, Category = "CoreLoop")
	void StartRun(int32 InitialCredits = 30);

	/** End the current run (server only) */
	UFUNCTION(BlueprintCallable, Category = "CoreLoop")
	void EndRun(const FString& Reason);

	/** Is a run currently active? */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "CoreLoop")
	bool IsRunActive() const { return RunState.bRunActive; }

	// --- Phase transitions -----------------------------------------------

	/** Transition to Base phase */
	UFUNCTION(BlueprintCallable, Category = "CoreLoop")
	void TransitionToBase();

	/** Transition to Train phase */
	UFUNCTION(BlueprintCallable, Category = "CoreLoop")
	void TransitionToTrain();

	/** Select destination and transition to Underground */
	UFUNCTION(BlueprintCallable, Category = "CoreLoop")
	void SelectTrainDestination(const FTrainDestination& Destination);

	/** Transition to Underground phase */
	UFUNCTION(BlueprintCallable, Category = "CoreLoop")
	void TransitionToUnderground();

	/** Return from Underground to Base */
	UFUNCTION(BlueprintCallable, Category = "CoreLoop")
	void ReturnToBase();

	/** Begin escape / ending sequence */
	UFUNCTION(BlueprintCallable, Category = "CoreLoop")
	void TransitionToEscape();

	// --- Queries ---------------------------------------------------------

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "CoreLoop")
	ECoreLoopPhase GetCurrentPhase() const { return RunState.CurrentPhase; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "CoreLoop")
	const FRunState& GetRunState() const { return RunState; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "CoreLoop")
	int32 GetCompletedTrips() const { return RunState.CompletedTrips; }

	// --- Destination management ------------------------------------------

	/** Register available train destinations (call during level setup) */
	UFUNCTION(BlueprintCallable, Category = "CoreLoop|Train")
	void RegisterDestination(const FTrainDestination& Destination);

	/** Get all registered (and unlocked) destinations */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "CoreLoop|Train")
	TArray<FTrainDestination> GetAvailableDestinations() const;

	/** Currently selected destination */
	UPROPERTY(BlueprintReadOnly, Category = "CoreLoop|Train")
	FTrainDestination CurrentDestination;

	// --- Events ----------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "CoreLoop|Events")
	FOnCoreLoopPhaseChanged OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "CoreLoop|Events")
	FOnRunStarted OnRunStarted;

	UPROPERTY(BlueprintAssignable, Category = "CoreLoop|Events")
	FOnRunEnded OnRunEnded;

	UPROPERTY(BlueprintAssignable, Category = "CoreLoop|Events")
	FOnTripCompleted OnTripCompleted;

	UPROPERTY(BlueprintAssignable, Category = "CoreLoop|Events")
	FOnTrainDestinationSelected OnTrainDestinationSelected;

protected:
	/** Set phase and broadcast */
	void SetPhase(ECoreLoopPhase NewPhase);

	/** Sync phase to GameState for replication */
	void SyncToGameState();

	/** Check authority */
	bool HasAuthority() const;

private:
	FRunState RunState;
	TArray<FTrainDestination> RegisteredDestinations;

	/** Check if a destination is unlocked by world flags */
	bool IsDestinationUnlocked(const FTrainDestination& Dest) const;
};
