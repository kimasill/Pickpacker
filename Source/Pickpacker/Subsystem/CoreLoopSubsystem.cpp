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
	RunState.RunId = FGuid::NewGuid();
	RunState.TeamCredits = InitialCredits;
	RunState.TeamSuspicion = 0.0f;
	RunState.CurrentPhase = ECoreLoopPhase::Base;
	RunState.CurrentStage = ERunProgressStage::GameStart;

	if (UWorld* World = GetWorld())
	{
		if (APickpackerGameState* GS = Cast<APickpackerGameState>(World->GetGameState()))
		{
			if (UEscapeProgressComponent* EscapeProgress = GS->GetEscapeProgressComponent())
			{
				for (const FWorldFlagEntry& WorldFlag : EscapeProgress->GetWorldFlags())
				{
					if (!WorldFlag.Flag.IsValid())
					{
						continue;
					}

					FEndingFlagState EndingFlagState;
					EndingFlagState.FlagId = WorldFlag.Flag.GetTagName();
					EndingFlagState.bUnlocked = WorldFlag.Value > 0;
					EndingFlagState.bLocked = WorldFlag.Value < 0;
					RunState.EndingFlags.Add(EndingFlagState);
				}
			}
		}
	}

	SyncToGameState();

	OnRunStarted.Broadcast(RunState);
	OnPhaseChanged.Broadcast(ECoreLoopPhase::None, ECoreLoopPhase::Base);
	SetRunStage(ERunProgressStage::Lobby);

	UE_LOG(LogTemp, Log, TEXT("[CoreLoopSubsystem] Run started. Credits=%d"), InitialCredits);
}

bool UCoreLoopSubsystem::RestoreRunState(const FRunState& SavedRunState, const FTrainDestination& SavedDestination)
{
	if (!HasAuthority() || !SavedRunState.bRunActive)
	{
		return false;
	}

	RunState = SavedRunState;
	CurrentDestination = SavedDestination;

	SyncToGameState();

	if (!CurrentDestination.DestinationId.IsNone())
	{
		OnTrainDestinationSelected.Broadcast(CurrentDestination);
	}

	UE_LOG(LogTemp, Log, TEXT("[CoreLoopSubsystem] Restored run. Phase=%d Stage=%d Trips=%d"),
		static_cast<int32>(RunState.CurrentPhase),
		static_cast<int32>(RunState.CurrentStage),
		RunState.CompletedTrips);

	return true;
}

void UCoreLoopSubsystem::EndRun(const FString& Reason)
{
	if (!HasAuthority() || !RunState.bRunActive)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[CoreLoopSubsystem] Run ended: %s (Trips=%d)"), *Reason, RunState.CompletedTrips);

	SetRunStage(ERunProgressStage::Result);
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

	SetRunStage(ERunProgressStage::Underworld);
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

	SetRunStage(ERunProgressStage::Return);
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

void UCoreLoopSubsystem::SetRunStage(ERunProgressStage NewStage)
{
	if (!HasAuthority() || !RunState.bRunActive || RunState.CurrentStage == NewStage)
	{
		return;
	}

	const ERunProgressStage OldStage = RunState.CurrentStage;
	RunState.CurrentStage = NewStage;

	SyncToGameState();
	OnRunStageChanged.Broadcast(OldStage, NewStage);

	UE_LOG(LogTemp, Log, TEXT("[CoreLoopSubsystem] Stage: %d -> %d"),
		static_cast<int32>(OldStage), static_cast<int32>(NewStage));
}

void UCoreLoopSubsystem::AssignMissionDefinition(const FMissionDefinition& MissionDefinition)
{
	if (!HasAuthority() || !RunState.bRunActive)
	{
		return;
	}

	RunState.ActiveMission = MissionDefinition;
	SetRunStage(ERunProgressStage::ReceiveMission);

	SyncToGameState();
	OnMissionAssigned.Broadcast(RunState.ActiveMission);

	UE_LOG(LogTemp, Log, TEXT("[CoreLoopSubsystem] Mission assigned: %s"),
		*RunState.ActiveMission.MissionId.ToString());
}

void UCoreLoopSubsystem::DepositStorageItem(const FGameplayTag& ItemTag, int32 Quantity, FName SlotId)
{
	if (!HasAuthority() || !RunState.bRunActive || !ItemTag.IsValid() || Quantity <= 0)
	{
		return;
	}

	const FName ResolvedSlotId = SlotId.IsNone()
		? FName(*ItemTag.ToString())
		: SlotId;
	const FName SessionScope = FName(*RunState.RunId.ToString(EGuidFormats::DigitsWithHyphens));

	if (FStorageRecord* ExistingRecord = RunState.StorageRecords.FindByPredicate(
		[&ResolvedSlotId, &ItemTag](const FStorageRecord& Record)
		{
			return Record.SlotId == ResolvedSlotId || (!ResolvedSlotId.IsNone() && Record.ItemTag == ItemTag);
		}))
	{
		ExistingRecord->Quantity += Quantity;
		ExistingRecord->ItemTag = ItemTag;
		ExistingRecord->SessionScope = SessionScope;
	}
	else
	{
		FStorageRecord NewRecord;
		NewRecord.SlotId = ResolvedSlotId;
		NewRecord.ItemTag = ItemTag;
		NewRecord.Quantity = Quantity;
		NewRecord.SessionScope = SessionScope;
		RunState.StorageRecords.Add(NewRecord);
	}

	SyncToGameState();
	OnStorageRecordsUpdated.Broadcast(RunState.StorageRecords);
}

bool UCoreLoopSubsystem::ConsumeStorageItem(const FGameplayTag& ItemTag, int32 Quantity, FName SlotId)
{
	if (!HasAuthority() || !RunState.bRunActive || !ItemTag.IsValid() || Quantity <= 0)
	{
		return false;
	}

	const int32 RecordIndex = RunState.StorageRecords.IndexOfByPredicate(
		[&ItemTag, &SlotId](const FStorageRecord& Record)
		{
			const bool bSlotMatch = SlotId.IsNone() || Record.SlotId == SlotId;
			return bSlotMatch && Record.ItemTag == ItemTag;
		});

	if (!RunState.StorageRecords.IsValidIndex(RecordIndex) || RunState.StorageRecords[RecordIndex].Quantity < Quantity)
	{
		return false;
	}

	FStorageRecord& Record = RunState.StorageRecords[RecordIndex];
	Record.Quantity -= Quantity;
	if (Record.Quantity <= 0)
	{
		RunState.StorageRecords.RemoveAt(RecordIndex);
	}

	SyncToGameState();
	OnStorageRecordsUpdated.Broadcast(RunState.StorageRecords);
	return true;
}

void UCoreLoopSubsystem::SetPersonaStats(const FPersonaStats& NewPersonaStats)
{
	if (!HasAuthority() || !RunState.bRunActive)
	{
		return;
	}

	RunState.PersonaStats = NewPersonaStats;
	SyncToGameState();
}

void UCoreLoopSubsystem::SetEndingFlagState(const FEndingFlagState& EndingFlagState)
{
	if (!HasAuthority() || !RunState.bRunActive || EndingFlagState.FlagId.IsNone())
	{
		return;
	}

	if (FEndingFlagState* ExistingFlag = RunState.EndingFlags.FindByPredicate(
		[&EndingFlagState](const FEndingFlagState& Existing)
		{
			return Existing.FlagId == EndingFlagState.FlagId;
		}))
	{
		*ExistingFlag = EndingFlagState;
	}
	else
	{
		RunState.EndingFlags.Add(EndingFlagState);
	}

	SyncToGameState();
}

void UCoreLoopSubsystem::SetTeamCredits(int32 NewCredits)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 ClampedCredits = FMath::Max(0, NewCredits);
	if (RunState.TeamCredits == ClampedCredits)
	{
		return;
	}

	RunState.TeamCredits = ClampedCredits;
}

void UCoreLoopSubsystem::SetTeamSuspicion(float NewSuspicion)
{
	if (!HasAuthority())
	{
		return;
	}

	const float ClampedSuspicion = FMath::Max(0.0f, NewSuspicion);
	if (FMath::IsNearlyEqual(RunState.TeamSuspicion, ClampedSuspicion))
	{
		return;
	}

	RunState.TeamSuspicion = ClampedSuspicion;
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
	GS->SetTeamCredits(RunState.TeamCredits);
	GS->SetTeamSuspicionValue(RunState.TeamSuspicion);
	GS->SetRunStage(RunState.CurrentStage);
	GS->SetCurrentMissionDefinition(RunState.ActiveMission);
	GS->SetSessionStorageRecords(RunState.StorageRecords);
	GS->SetRunPersonaStats(RunState.PersonaStats);
	GS->SetEndingFlagStates(RunState.EndingFlags);
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
