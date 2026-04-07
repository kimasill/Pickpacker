// CoreLoopSubsystem.cpp

#include "CoreLoopSubsystem.h"
#include "GameState/PickpackerGameState.h"
#include "Components/EscapeProgressComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"

UCoreLoopSubsystem::UCoreLoopSubsystem()
{
}

void UCoreLoopSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[CoreLoopSubsystem] Initialized"));
}

void UCoreLoopSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("[CoreLoopSubsystem] Deinitialized. Trips=%d, Phase=%d"),
		RunState.CompletedTrips, static_cast<int32>(RunState.CurrentPhase));
	Super::Deinitialize();
}

bool UCoreLoopSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only create in game worlds, not editor previews
	UWorld* World = Cast<UWorld>(Outer);
	if (!World)
	{
		return false;
	}
	return World->IsGameWorld();
}

// =========================================================================
// Run lifecycle
// =========================================================================

void UCoreLoopSubsystem::StartRun(int32 InitialCredits)
{
	if (!HasAuthority())
	{
		return;
	}

	RunState = FRunState();
	RunState.bRunActive = true;
	RunState.TeamCredits = InitialCredits;
	RunState.CurrentPhase = ECoreLoopPhase::Base;

	SyncToGameState();

	OnRunStarted.Broadcast(RunState);
	OnPhaseChanged.Broadcast(ECoreLoopPhase::None, ECoreLoopPhase::Base);

	UE_LOG(LogTemp, Log, TEXT("[CoreLoopSubsystem] Run started. Credits=%d"), InitialCredits);
}

void UCoreLoopSubsystem::EndRun(const FString& Reason)
{
	if (!HasAuthority() || !RunState.bRunActive)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[CoreLoopSubsystem] Run ended: %s (Trips=%d)"), *Reason, RunState.CompletedTrips);

	RunState.bRunActive = false;
	OnRunEnded.Broadcast(RunState);

	SyncToGameState();
}

// =========================================================================
// Phase transitions
// =========================================================================

void UCoreLoopSubsystem::TransitionToBase()
{
	if (!HasAuthority() || !RunState.bRunActive)
	{
		return;
	}

	SetPhase(ECoreLoopPhase::Base);
}

void UCoreLoopSubsystem::TransitionToTrain()
{
	if (!HasAuthority() || !RunState.bRunActive)
	{
		return;
	}

	SetPhase(ECoreLoopPhase::Train);
}

void UCoreLoopSubsystem::SelectTrainDestination(const FTrainDestination& Destination)
{
	if (!HasAuthority() || !RunState.bRunActive)
	{
		return;
	}

	CurrentDestination = Destination;
	RunState.TargetZoneTag = Destination.AvailableItemTags.IsEmpty()
		? FGameplayTag()
		: Destination.AvailableItemTags.First();

	OnTrainDestinationSelected.Broadcast(Destination);

	UE_LOG(LogTemp, Log, TEXT("[CoreLoopSubsystem] Destination selected: %s (Difficulty=%d)"),
		*Destination.DestinationId.ToString(), static_cast<int32>(Destination.Difficulty));
}

void UCoreLoopSubsystem::TransitionToUnderground()
{
	if (!HasAuthority() || !RunState.bRunActive)
	{
		return;
	}

	SetPhase(ECoreLoopPhase::Underground);
}

void UCoreLoopSubsystem::ReturnToBase()
{
	if (!HasAuthority() || !RunState.bRunActive)
	{
		return;
	}

	RunState.CompletedTrips++;
	OnTripCompleted.Broadcast(RunState.CompletedTrips);

	UE_LOG(LogTemp, Log, TEXT("[CoreLoopSubsystem] Trip completed. Total trips=%d"), RunState.CompletedTrips);

	SetPhase(ECoreLoopPhase::Base);
}

void UCoreLoopSubsystem::TransitionToEscape()
{
	if (!HasAuthority() || !RunState.bRunActive)
	{
		return;
	}

	SetPhase(ECoreLoopPhase::Escape);
}

// =========================================================================
// Destination management
// =========================================================================

void UCoreLoopSubsystem::RegisterDestination(const FTrainDestination& Destination)
{
	// Avoid duplicates
	for (const FTrainDestination& Existing : RegisteredDestinations)
	{
		if (Existing.DestinationId == Destination.DestinationId)
		{
			return;
		}
	}
	RegisteredDestinations.Add(Destination);

	UE_LOG(LogTemp, Log, TEXT("[CoreLoopSubsystem] Destination registered: %s"), *Destination.DestinationId.ToString());
}

TArray<FTrainDestination> UCoreLoopSubsystem::GetAvailableDestinations() const
{
	TArray<FTrainDestination> Result;
	for (const FTrainDestination& Dest : RegisteredDestinations)
	{
		if (IsDestinationUnlocked(Dest))
		{
			Result.Add(Dest);
		}
	}
	return Result;
}

// =========================================================================
// Internal
// =========================================================================

void UCoreLoopSubsystem::SetPhase(ECoreLoopPhase NewPhase)
{
	if (RunState.CurrentPhase == NewPhase)
	{
		return;
	}

	const ECoreLoopPhase OldPhase = RunState.CurrentPhase;
	RunState.CurrentPhase = NewPhase;

	SyncToGameState();

	OnPhaseChanged.Broadcast(OldPhase, NewPhase);

	UE_LOG(LogTemp, Log, TEXT("[CoreLoopSubsystem] Phase: %d -> %d"),
		static_cast<int32>(OldPhase), static_cast<int32>(NewPhase));
}

void UCoreLoopSubsystem::SyncToGameState()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APickpackerGameState* GS = Cast<APickpackerGameState>(World->GetGameState());
	if (!GS)
	{
		return;
	}

	// Sync core loop phase to game state for client replication
	GS->SetCoreLoopPhase(RunState.CurrentPhase);
	GS->SetCompletedTrips(RunState.CompletedTrips);
}

bool UCoreLoopSubsystem::HasAuthority() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	AGameModeBase* GM = World->GetAuthGameMode();
	return GM != nullptr;
}

bool UCoreLoopSubsystem::IsDestinationUnlocked(const FTrainDestination& Dest) const
{
	if (!Dest.UnlockFlag.IsValid())
	{
		return true; // No flag requirement
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	APickpackerGameState* GS = Cast<APickpackerGameState>(World->GetGameState());
	if (!GS)
	{
		return false;
	}

	UEscapeProgressComponent* EscapeProgress = GS->GetEscapeProgressComponent();
	if (!EscapeProgress)
	{
		return false;
	}

	return EscapeProgress->GetWorldFlag(Dest.UnlockFlag) != 0;
}
