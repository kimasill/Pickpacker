// TrainTravelComponent.cpp

#include "TrainTravelComponent.h"
#include "Subsystem/CoreLoopSubsystem.h"
#include "GameState/PickpackerGameState.h"
#include "DataAssets/DA_LevelVariant.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Parcel/ParcelActor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "PlayerController/BlasterPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Subsystem/RunPersistenceSubsystem.h"

UTrainTravelComponent::UTrainTravelComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UTrainTravelComponent::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] Initialized"));
}

void UTrainTravelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!HasAuthority())
	{
		return;
	}

	if (TrainState == ETrainState::Idle || TrainState == ETrainState::Arrived)
	{
		return;
	}

	PhaseElapsedTime += DeltaTime;

	switch (TrainState)
	{
	case ETrainState::Boarding:
		if (PhaseElapsedTime >= BoardingDuration)
		{
			HandleBoardingComplete();
		}
		break;

	case ETrainState::Departing:
		if (bDestinationSelected && PhaseElapsedTime >= DepartureDuration)
		{
			HandleDepartureComplete();
		}
		else if (!bDestinationSelected && PhaseElapsedTime >= DepartureDuration)
		{
			HandleDepartureComplete();

			if (AutoSelectTimeout > 0.0f && PhaseElapsedTime >= (DepartureDuration + AutoSelectTimeout))
			{
				TryAutoSelectDestination();
			}
		}
		break;

	case ETrainState::Traveling:
		TravelElapsedTime += DeltaTime;
		OnTravelProgress.Broadcast(GetTravelProgress());
		if (TravelElapsedTime >= TravelDuration)
		{
			HandleTravelComplete();
		}
		break;

	case ETrainState::Arriving:
		if (PhaseElapsedTime >= ArrivalDuration)
		{
			HandleArrivalComplete();
		}
		break;

	default:
		break;
	}
}

void UTrainTravelComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTrainTravelComponent, TrainState);
	DOREPLIFETIME(UTrainTravelComponent, SelectedDestination);
	DOREPLIFETIME(UTrainTravelComponent, bDestinationSelected);
	DOREPLIFETIME(UTrainTravelComponent, SelectionContext);
	DOREPLIFETIME(UTrainTravelComponent, DestinationVotes);
	DOREPLIFETIME(UTrainTravelComponent, RouteSelectionResult);
	DOREPLIFETIME(UTrainTravelComponent, TravelElapsedTime);
	DOREPLIFETIME(UTrainTravelComponent, JourneyType);
	DOREPLIFETIME(UTrainTravelComponent, CargoRecords);
}

// =========================================================================
// API
// =========================================================================

void UTrainTravelComponent::BeginBoarding()
{
	if (!HasAuthority())
	{
		return;
	}

	if (TrainState != ETrainState::Idle)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TrainTravelComponent] BeginBoarding called but state is %d"), static_cast<int32>(TrainState));
		return;
	}

	JourneyType = ETrainJourneyType::OutboundMission;

	// CoreLoopSubsystem을 Train 페이즈로 전환
	if (UCoreLoopSubsystem* CoreLoop = GetCoreLoopSubsystem())
	{
		CoreLoop->TransitionToTrain();
	}

	ClearDestinationVotes();
	RouteSelectionResult = FRouteSelectionResult();
	CargoRecords.Empty();
	RefreshSelectionContext();
	SetTrainState(ETrainState::Boarding);
	UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] Boarding started. Duration=%.1fs"), BoardingDuration);
}

void UTrainTravelComponent::BeginReturnBoarding()
{
	if (!HasAuthority())
	{
		return;
	}

	if (TrainState != ETrainState::Idle)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TrainTravelComponent] BeginReturnBoarding called but state is %d"), static_cast<int32>(TrainState));
		return;
	}

	if (ReturnBaseLevel.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TrainTravelComponent] BeginReturnBoarding called without ReturnBaseLevel"));
		return;
	}

	JourneyType = ETrainJourneyType::ReturnToBase;
	SelectedDestination = BuildReturnBaseDestination();
	bDestinationSelected = true;
	RouteSelectionResult = BuildRouteSelectionResult(SelectedDestination);
	ClearDestinationVotes();
	RefreshSelectionContext();

	if (UCoreLoopSubsystem* CoreLoop = GetCoreLoopSubsystem())
	{
		CoreLoop->TransitionToTrain();
		CoreLoop->SetRunStage(ERunProgressStage::Return);
	}

	if (APickpackerGameState* PickpackerGameState = GetOwner<APickpackerGameState>())
	{
		PickpackerGameState->SetCurrentRouteSelectionResult(RouteSelectionResult);
	}

	SetTrainState(ETrainState::Boarding);
	UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] Return boarding started. BaseLevel=%s"), *ReturnBaseLevel.ToSoftObjectPath().ToString());
}

void UTrainTravelComponent::Depart()
{
	if (!HasAuthority())
	{
		return;
	}

	if (TrainState != ETrainState::Boarding && TrainState != ETrainState::Idle)
	{
		return;
	}

	SetTrainState(ETrainState::Departing);
	OnTrainDeparted.Broadcast();

	// 출발 시 목적지 선택 UI 오픈 트리거
	if (ShouldOpenDestinationSelection())
	{
		OpenDestinationSelection();
	}

	UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] Train departing. JourneyType=%d"), static_cast<int32>(JourneyType));
}

void UTrainTravelComponent::SelectDestination(const FTrainDestination& Destination)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bDestinationSelected)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TrainTravelComponent] Destination already selected"));
		return;
	}

	if (JourneyType != ETrainJourneyType::OutboundMission)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TrainTravelComponent] SelectDestination ignored because journey type is %d"), static_cast<int32>(JourneyType));
		return;
	}

	SelectedDestination = Destination;
	bDestinationSelected = true;
	RouteSelectionResult = BuildRouteSelectionResult(Destination);

	// CoreLoopSubsystem에 목적지 등록
	if (UCoreLoopSubsystem* CoreLoop = GetCoreLoopSubsystem())
	{
		CoreLoop->SelectTrainDestination(Destination);
	}

	if (APickpackerGameState* PickpackerGameState = GetOwner<APickpackerGameState>())
	{
		PickpackerGameState->SetRandomSeed(RouteSelectionResult.RouteSeed);
		PickpackerGameState->SetCurrentRouteSelectionResult(RouteSelectionResult);
	}

	RefreshSelectionContext();
	RefreshSelectionVotes();

	UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] Destination selected: %s"), *Destination.DestinationId.ToString());

	// 이미 출발 완료 상태라면 즉시 Traveling으로 전환
	if (TrainState == ETrainState::Departing)
	{
		if (PhaseElapsedTime >= DepartureDuration || bWaitingForDestinationAfterDeparture)
		{
			StartTraveling();
		}
	}
	else if (TrainState == ETrainState::Boarding)
	{
		Depart();
	}
}

void UTrainTravelComponent::SubmitDestinationVote(APlayerState* PlayerState, FName DestinationId)
{
	if (!HasAuthority() || !PlayerState || DestinationId.IsNone() || bDestinationSelected)
	{
		return;
	}

	const FTrainDestination* Destination = FindAvailableDestinationById(DestinationId);
	if (!Destination)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TrainTravelComponent] Vote rejected. Destination '%s' is not available"), *DestinationId.ToString());
		return;
	}

	FTrainDestinationVoteState* ExistingVote = DestinationVotes.FindByPredicate(
		[PlayerState](const FTrainDestinationVoteState& VoteState)
		{
			return VoteState.PlayerId == PlayerState->GetPlayerId();
		});

	if (ExistingVote)
	{
		ExistingVote->DestinationId = DestinationId;
		ExistingVote->PlayerName = FText::FromString(PlayerState->GetPlayerName());
		ExistingVote->bLockedIn = true;
	}
	else
	{
		FTrainDestinationVoteState VoteState;
		VoteState.PlayerId = PlayerState->GetPlayerId();
		VoteState.PlayerName = FText::FromString(PlayerState->GetPlayerName());
		VoteState.DestinationId = DestinationId;
		VoteState.bLockedIn = true;
		DestinationVotes.Add(VoteState);
	}

	RefreshSelectionVotes();
	RefreshSelectionContext();
	EvaluateDestinationVotes();

	UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] Vote submitted - PlayerId=%d Destination=%s"),
		PlayerState->GetPlayerId(), *DestinationId.ToString());
}

void UTrainTravelComponent::ForceArrive()
{
	if (!HasAuthority())
	{
		return;
	}

	HandleArrivalComplete();
}

void UTrainTravelComponent::ResetTrain()
{
	if (!HasAuthority())
	{
		return;
	}

	SetTrainState(ETrainState::Idle);
	SelectedDestination = FTrainDestination();
	bDestinationSelected = false;
	SelectionContext = FTrainSelectionContext();
	DestinationVotes.Empty();
	RouteSelectionResult = FRouteSelectionResult();
	PhaseElapsedTime = 0.0f;
	TravelElapsedTime = 0.0f;
	bWaitingForDestinationAfterDeparture = false;
	JourneyType = ETrainJourneyType::None;
	CargoRecords.Empty();

	if (APickpackerGameState* PickpackerGameState = GetOwner<APickpackerGameState>())
	{
		PickpackerGameState->SetCurrentRouteSelectionResult(RouteSelectionResult);
	}

	UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] Train reset to Idle"));
}

// =========================================================================
// 조회
// =========================================================================

float UTrainTravelComponent::GetTravelProgress() const
{
	if (TravelDuration <= 0.0f)
	{
		return 1.0f;
	}
	return FMath::Clamp(TravelElapsedTime / TravelDuration, 0.0f, 1.0f);
}

float UTrainTravelComponent::GetRemainingTravelTime() const
{
	return FMath::Max(0.0f, TravelDuration - TravelElapsedTime);
}

// =========================================================================
// 리플리케이션
// =========================================================================

void UTrainTravelComponent::OnRep_TrainState(ETrainState OldState)
{
	OnTrainStateChanged.Broadcast(OldState, TrainState);

	if (TrainState == ETrainState::Departing)
	{
		OnTrainDeparted.Broadcast();
		if (!bDestinationSelected)
		{
			OnDestinationSelectionOpened.Broadcast();
		}
	}
	else if (TrainState == ETrainState::Arrived)
	{
		OnTrainArrived.Broadcast();
	}
}

void UTrainTravelComponent::OnRep_SelectedDestination()
{
	// 클라이언트에서 목적지 정보 갱신 시 UI 업데이트 등에 활용
	UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] Client: Destination replicated: %s"), *SelectedDestination.DestinationId.ToString());
}

void UTrainTravelComponent::OnRep_SelectionContext()
{
	OnSelectionContextUpdated.Broadcast(SelectionContext);
}

void UTrainTravelComponent::OnRep_DestinationVotes()
{
	OnDestinationVotesUpdated.Broadcast(DestinationVotes);
}

void UTrainTravelComponent::OnRep_RouteSelectionResult()
{
	OnRouteSelectionResultUpdated.Broadcast(RouteSelectionResult);
}

// =========================================================================
// 내부 상태 전환
// =========================================================================

void UTrainTravelComponent::SetTrainState(ETrainState NewState)
{
	if (TrainState == NewState)
	{
		return;
	}

	const ETrainState OldState = TrainState;
	TrainState = NewState;
	PhaseElapsedTime = 0.0f;
	if (NewState != ETrainState::Departing)
	{
		bWaitingForDestinationAfterDeparture = false;
	}

	OnTrainStateChanged.Broadcast(OldState, NewState);

	UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] State: %d -> %d"), static_cast<int32>(OldState), static_cast<int32>(NewState));
}

void UTrainTravelComponent::HandleBoardingComplete()
{
	if (ShouldAutoDepartFromBoarding())
	{
		Depart();
	}
}

void UTrainTravelComponent::HandleDepartureComplete()
{
	if (bDestinationSelected)
	{
		StartTraveling();
	}
	else if (!bWaitingForDestinationAfterDeparture)
	{
		// 목적지 미선택 — Departing 상태 유지하며 대기
		bWaitingForDestinationAfterDeparture = true;
		UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] Departure complete but no destination selected. Waiting..."));
	}
}

void UTrainTravelComponent::HandleTravelComplete()
{
	SetTrainState(ETrainState::Arriving);
	UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] Arriving at destination"));
}

void UTrainTravelComponent::HandleArrivalComplete()
{
	SetTrainState(ETrainState::Arrived);
	OnTrainArrived.Broadcast();

	// CoreLoopSubsystem을 Underground 페이즈로 전환
	if (UCoreLoopSubsystem* CoreLoop = GetCoreLoopSubsystem())
	{
		CoreLoop->TransitionToUnderground();
	}

	// 레벨 트래블 실행
	ExecuteLevelTravel();
}

void UTrainTravelComponent::ExecuteLevelTravel()
{
	if (!bDestinationSelected)
	{
		UE_LOG(LogTemp, Error, TEXT("[TrainTravelComponent] Cannot travel — no destination selected"));
		return;
	}

	const TSoftObjectPtr<UWorld>& ZoneLevel = SelectedDestination.ZoneLevel;
	if (ZoneLevel.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TrainTravelComponent] Destination %s has no ZoneLevel set. Skipping travel."),
			*SelectedDestination.DestinationId.ToString());
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FString LevelPath = ZoneLevel.GetLongPackageName();
	UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] ServerTravel to: %s"), *LevelPath);

	if (APickpackerGameState* PickpackerGameState = GetOwner<APickpackerGameState>())
	{
		if (UCoreLoopSubsystem* CoreLoop = GetCoreLoopSubsystem())
		{
			FTrainDestination DestinationToPersist = SelectedDestination;
			FRouteSelectionResult RouteSelectionToPersist = RouteSelectionResult;
			TArray<FStorageRecord> CargoRecordsToPersist = CargoRecords;

			if (JourneyType == ETrainJourneyType::ReturnToBase)
			{
				CoreLoop->ReturnToBase();
				DestinationToPersist = FTrainDestination();
				RouteSelectionToPersist = FRouteSelectionResult();
				ResetTrain();
			}

			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				if (URunPersistenceSubsystem* RunPersistence = GameInstance->GetSubsystem<URunPersistenceSubsystem>())
				{
					const TArray<FWorldFlagEntry>& WorldFlags = PickpackerGameState->GetEscapeProgressComponent()
						? PickpackerGameState->GetEscapeProgressComponent()->GetWorldFlags()
						: TArray<FWorldFlagEntry>();
					RunPersistence->SaveSnapshot(CoreLoop->GetRunState(), DestinationToPersist, RouteSelectionToPersist, WorldFlags, CargoRecordsToPersist);
				}
			}
		}
	}

	World->ServerTravel(LevelPath + TEXT("?listen"), true);
}

void UTrainTravelComponent::OpenDestinationSelection()
{
	if (!ShouldOpenDestinationSelection())
	{
		return;
	}

	RefreshSelectionContext();
	RefreshSelectionVotes();
	OnDestinationSelectionOpened.Broadcast();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABlasterPlayerController* PlayerController = Cast<ABlasterPlayerController>(It->Get()))
		{
			PlayerController->ClientOpenDestinationSelectUI();
		}
	}
}

bool UTrainTravelComponent::TryAutoSelectDestination()
{
	if (JourneyType != ETrainJourneyType::OutboundMission)
	{
		return bDestinationSelected;
	}

	if (bDestinationSelected)
	{
		return true;
	}

	FTrainDestination ResolvedDestination;
	if (TryResolveVoteWinner(ResolvedDestination))
	{
		SelectDestination(ResolvedDestination);
		return bDestinationSelected;
	}

	UCoreLoopSubsystem* CoreLoop = GetCoreLoopSubsystem();
	const TArray<FTrainDestination> AvailableDestinations = CoreLoop ? CoreLoop->GetAvailableDestinations() : TArray<FTrainDestination>();
	if (AvailableDestinations.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TrainTravelComponent] Auto-select requested but no destinations are available"));
		return false;
	}

	if (TryResolveHostPreferredDestination(ResolvedDestination, AvailableDestinations))
	{
		SelectDestination(ResolvedDestination);
	}
	else
	{
		SelectDestination(AvailableDestinations[0]);
	}

	UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] Auto-selected destination: %s"), *GetSelectedDestination().DestinationId.ToString());
	return bDestinationSelected;
}

void UTrainTravelComponent::StartTraveling()
{
	if (!bDestinationSelected)
	{
		return;
	}

	TravelElapsedTime = 0.0f;
	SetTrainState(ETrainState::Traveling);
	UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] Traveling to %s. Duration=%.1fs"), *SelectedDestination.DestinationId.ToString(), TravelDuration);
}

// =========================================================================
// 유틸리티
// =========================================================================

bool UTrainTravelComponent::HasAuthority() const
{
	return GetOwner() && GetOwner()->HasAuthority();
}

UCoreLoopSubsystem* UTrainTravelComponent::GetCoreLoopSubsystem() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UCoreLoopSubsystem>();
}

void UTrainTravelComponent::RefreshSelectionContext()
{
	FTrainSelectionContext NewContext;

	if (UCoreLoopSubsystem* CoreLoop = GetCoreLoopSubsystem())
	{
		NewContext.AvailableDestinations = CoreLoop->GetAvailableDestinations();
	}

	if (const APickpackerGameState* PickpackerGameState = GetOwner<APickpackerGameState>())
	{
		NewContext.ActiveOrders = PickpackerGameState->GetActiveOrders();
		NewContext.MissionDefinition = PickpackerGameState->GetCurrentMissionDefinition();
	}

	NewContext.VotePolicy = VoteResolutionPolicy;
	NewContext.EligibleVoterCount = GetEligibleVoterCount();
	NewContext.BoardingDuration = BoardingDuration;
	NewContext.DepartureDuration = DepartureDuration;
	NewContext.TravelDuration = TravelDuration;
	NewContext.ArrivalDuration = ArrivalDuration;
	NewContext.AutoSelectTimeout = AutoSelectTimeout;
	NewContext.bSelectionOpen = ShouldOpenDestinationSelection() && TrainState == ETrainState::Departing && !bDestinationSelected;
	NewContext.LockedDestinationId = bDestinationSelected ? SelectedDestination.DestinationId : NAME_None;

	SelectionContext = MoveTemp(NewContext);
	OnSelectionContextUpdated.Broadcast(SelectionContext);
}

void UTrainTravelComponent::RefreshSelectionVotes()
{
	if (!HasAuthority())
	{
		return;
	}

	const TArray<FTrainDestination> AvailableDestinations = SelectionContext.AvailableDestinations.Num() > 0
		? SelectionContext.AvailableDestinations
		: (GetCoreLoopSubsystem() ? GetCoreLoopSubsystem()->GetAvailableDestinations() : TArray<FTrainDestination>());
	TSet<int32> EligiblePlayerIds;

	if (const APickpackerGameState* PickpackerGameState = GetOwner<APickpackerGameState>())
	{
		for (APlayerState* PlayerState : PickpackerGameState->PlayerArray)
		{
			if (IsValid(PlayerState))
			{
				EligiblePlayerIds.Add(PlayerState->GetPlayerId());
			}
		}
	}

	DestinationVotes.RemoveAll(
		[&AvailableDestinations, &EligiblePlayerIds](const FTrainDestinationVoteState& VoteState)
		{
			if (VoteState.PlayerId == INDEX_NONE || VoteState.DestinationId.IsNone())
			{
				return true;
			}

			if (EligiblePlayerIds.Num() > 0 && !EligiblePlayerIds.Contains(VoteState.PlayerId))
			{
				return true;
			}

			return !AvailableDestinations.ContainsByPredicate(
				[&VoteState](const FTrainDestination& Destination)
				{
					return Destination.DestinationId == VoteState.DestinationId;
				});
		});

	OnDestinationVotesUpdated.Broadcast(DestinationVotes);
}

void UTrainTravelComponent::EvaluateDestinationVotes()
{
	if (!HasAuthority() || bDestinationSelected)
	{
		return;
	}

	FTrainDestination WinningDestination;
	if (TryResolveVoteWinner(WinningDestination))
	{
		SelectDestination(WinningDestination);
	}
}

bool UTrainTravelComponent::TryResolveVoteWinner(FTrainDestination& OutDestination) const
{
	const TArray<FTrainDestination> AvailableDestinations = SelectionContext.AvailableDestinations.Num() > 0
		? SelectionContext.AvailableDestinations
		: (GetCoreLoopSubsystem() ? GetCoreLoopSubsystem()->GetAvailableDestinations() : TArray<FTrainDestination>());

	if (AvailableDestinations.Num() == 0)
	{
		return false;
	}

	if (VoteResolutionPolicy == ETrainVoteResolutionPolicy::HostOnly)
	{
		return TryResolveHostPreferredDestination(OutDestination, AvailableDestinations);
	}

	TMap<FName, int32> VoteCounts;
	for (const FTrainDestinationVoteState& VoteState : DestinationVotes)
	{
		if (!VoteState.bLockedIn || VoteState.DestinationId.IsNone())
		{
			continue;
		}

		VoteCounts.FindOrAdd(VoteState.DestinationId)++;
	}

	if (VoteCounts.Num() == 0)
	{
		return false;
	}

	const int32 EligibleVoters = FMath::Max(1, SelectionContext.EligibleVoterCount);
	int32 HighestVoteCount = 0;
	TArray<FTrainDestination> TiedDestinations;

	for (const FTrainDestination& Destination : AvailableDestinations)
	{
		const int32 VoteCount = VoteCounts.FindRef(Destination.DestinationId);
		if (VoteCount <= 0)
		{
			continue;
		}

		if (VoteCount > HighestVoteCount)
		{
			HighestVoteCount = VoteCount;
			TiedDestinations.Reset();
			TiedDestinations.Add(Destination);
		}
		else if (VoteCount == HighestVoteCount)
		{
			TiedDestinations.Add(Destination);
		}
	}

	if (HighestVoteCount > EligibleVoters / 2 && TiedDestinations.Num() > 0)
	{
		OutDestination = TiedDestinations[0];
		return true;
	}

	const bool bEveryoneLockedIn = DestinationVotes.Num() >= EligibleVoters;
	if (!bEveryoneLockedIn || TiedDestinations.Num() == 0)
	{
		return false;
	}

	if (TiedDestinations.Num() == 1)
	{
		OutDestination = TiedDestinations[0];
		return true;
	}

	return TryResolveHostPreferredDestination(OutDestination, TiedDestinations);
}

bool UTrainTravelComponent::TryResolveHostPreferredDestination(FTrainDestination& OutDestination, const TArray<FTrainDestination>& CandidateDestinations) const
{
	if (CandidateDestinations.Num() == 0)
	{
		return false;
	}

	APlayerState* HostPlayerState = nullptr;
	int32 LowestPlayerId = MAX_int32;

	if (const APickpackerGameState* PickpackerGameState = GetOwner<APickpackerGameState>())
	{
		for (APlayerState* PlayerState : PickpackerGameState->PlayerArray)
		{
			if (!PlayerState)
			{
				continue;
			}

			if (APlayerController* PlayerController = Cast<APlayerController>(PlayerState->GetOwner()))
			{
				if (PlayerController->IsLocalController())
				{
					HostPlayerState = PlayerState;
					break;
				}
			}

			if (PlayerState->GetPlayerId() < LowestPlayerId)
			{
				LowestPlayerId = PlayerState->GetPlayerId();
				HostPlayerState = PlayerState;
			}
		}
	}

	if (HostPlayerState)
	{
		if (const FTrainDestinationVoteState* HostVote = DestinationVotes.FindByPredicate(
			[&HostPlayerState](const FTrainDestinationVoteState& VoteState)
			{
				return VoteState.PlayerId == HostPlayerState->GetPlayerId() && VoteState.bLockedIn;
			}))
		{
			if (const FTrainDestination* HostDestination = CandidateDestinations.FindByPredicate(
				[&HostVote](const FTrainDestination& Destination)
				{
					return Destination.DestinationId == HostVote->DestinationId;
				}))
			{
				OutDestination = *HostDestination;
				return true;
			}
		}
	}

	OutDestination = CandidateDestinations[0];
	return true;
}

int32 UTrainTravelComponent::GetEligibleVoterCount() const
{
	if (const APickpackerGameState* PickpackerGameState = GetOwner<APickpackerGameState>())
	{
		int32 PlayerCount = 0;
		for (APlayerState* PlayerState : PickpackerGameState->PlayerArray)
		{
			if (IsValid(PlayerState))
			{
				++PlayerCount;
			}
		}
		return FMath::Max(1, PlayerCount);
	}

	return 1;
}

FRouteSelectionResult UTrainTravelComponent::BuildRouteSelectionResult(const FTrainDestination& Destination) const
{
	FRouteSelectionResult Result;
	Result.DestinationId = Destination.DestinationId;
	Result.DangerLevel = Destination.Difficulty;
	Result.TargetZoneTag = Destination.AvailableItemTags.IsEmpty()
		? FGameplayTag()
		: Destination.AvailableItemTags.First();
	Result.AvailableItemTags = Destination.AvailableItemTags;
	Result.ZoneLevel = Destination.ZoneLevel;

	if (const APickpackerGameState* PickpackerGameState = GetOwner<APickpackerGameState>())
	{
		if (const UDA_LevelVariant* LevelVariant = PickpackerGameState->GetLevelVariant())
		{
			Result.LevelVariantId = LevelVariant->GetFName();
		}
	}

	if (Result.LevelVariantId.IsNone() && !Destination.ZoneLevel.IsNull())
	{
		Result.LevelVariantId = FName(*Destination.ZoneLevel.ToSoftObjectPath().GetAssetName());
	}

	if (Result.LevelVariantId.IsNone())
	{
		Result.LevelVariantId = Destination.DestinationId;
	}

	uint32 RouteSeed = GetTypeHash(Destination.DestinationId);
	if (const APickpackerGameState* PickpackerGameState = GetOwner<APickpackerGameState>())
	{
		RouteSeed = HashCombineFast(RouteSeed, static_cast<uint32>(PickpackerGameState->GetRandomSeed()));
	}

	if (RouteSeed == 0)
	{
		RouteSeed = static_cast<uint32>(FMath::RandRange(1, MAX_int32));
	}

	Result.RouteSeed = static_cast<int32>(RouteSeed & 0x7fffffff);
	if (Result.RouteSeed == 0)
	{
		Result.RouteSeed = 1;
	}

	return Result;
}

void UTrainTravelComponent::SetCargoRecords(const TArray<FStorageRecord>& NewCargoRecords)
{
	if (!HasAuthority())
	{
		return;
	}

	CargoRecords = NewCargoRecords;
}

void UTrainTravelComponent::ClearCargoRecords()
{
	if (!HasAuthority())
	{
		return;
	}

	CargoRecords.Empty();
}

bool UTrainTravelComponent::LoadParcelIntoCargo(AParcelActor* Parcel)
{
	if (!HasAuthority() || !Parcel)
	{
		return false;
	}

	TMap<FGameplayTag, int32> UnitsByTag;
	Parcel->GetContentUnitsByTag(UnitsByTag);

	if (UnitsByTag.Num() == 0)
	{
		const FGameplayTag FallbackTag = Parcel->GetParcelTag().IsValid()
			? Parcel->GetParcelTag()
			: Parcel->GetClassificationTag();
		if (FallbackTag.IsValid())
		{
			UnitsByTag.Add(FallbackTag, FMath::Max(1, Parcel->GetContentUnitTotal()));
		}
	}

	if (UnitsByTag.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TrainTravelComponent] Failed to load parcel '%s' into cargo - no valid tags"), *Parcel->GetName());
		return false;
	}

	for (const TPair<FGameplayTag, int32>& Pair : UnitsByTag)
	{
		if (!Pair.Key.IsValid() || Pair.Value <= 0)
		{
			continue;
		}

		FStorageRecord* ExistingRecord = CargoRecords.FindByPredicate(
			[&Pair](const FStorageRecord& Record)
			{
				return Record.ItemTag == Pair.Key;
			});

		if (ExistingRecord)
		{
			ExistingRecord->Quantity += Pair.Value;
			continue;
		}

		FStorageRecord NewRecord;
		NewRecord.SlotId = FName(*Pair.Key.ToString());
		NewRecord.ItemTag = Pair.Key;
		NewRecord.Quantity = Pair.Value;
		NewRecord.SessionScope = FName(TEXT("TrainCargo"));
		CargoRecords.Add(NewRecord);
	}

	UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] Loaded parcel '%s' into cargo. CargoEntries=%d"),
		*Parcel->GetName(),
		CargoRecords.Num());
	return true;
}

FTrainDestination UTrainTravelComponent::BuildReturnBaseDestination() const
{
	FTrainDestination Destination;
	Destination.DestinationId = FName(TEXT("BaseReturn"));
	Destination.DisplayName = FText::FromString(TEXT("Main Station"));
	Destination.Difficulty = EZoneDifficulty::Low;
	Destination.ZoneLevel = ReturnBaseLevel;
	return Destination;
}

const FTrainDestination* UTrainTravelComponent::FindAvailableDestinationById(FName DestinationId) const
{
	if (DestinationId.IsNone())
	{
		return nullptr;
	}

	const TArray<FTrainDestination> AvailableDestinations = SelectionContext.AvailableDestinations.Num() > 0
		? SelectionContext.AvailableDestinations
		: (GetCoreLoopSubsystem() ? GetCoreLoopSubsystem()->GetAvailableDestinations() : TArray<FTrainDestination>());

	return AvailableDestinations.FindByPredicate(
		[DestinationId](const FTrainDestination& Destination)
		{
			return Destination.DestinationId == DestinationId;
		});
}

void UTrainTravelComponent::ClearDestinationVotes()
{
	DestinationVotes.Empty();
	RefreshSelectionVotes();
}

bool UTrainTravelComponent::ShouldOpenDestinationSelection() const
{
	return JourneyType == ETrainJourneyType::OutboundMission;
}

bool UTrainTravelComponent::ShouldAutoDepartFromBoarding() const
{
	return JourneyType != ETrainJourneyType::ReturnToBase || !bRequireManualDepartureForReturnJourney;
}
