// TrainTravelComponent.cpp

#include "TrainTravelComponent.h"
#include "Subsystem/CoreLoopSubsystem.h"
#include "GameState/PickpackerGameState.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"

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
		if (PhaseElapsedTime >= DepartureDuration)
		{
			HandleDepartureComplete();
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
	DOREPLIFETIME(UTrainTravelComponent, TravelElapsedTime);
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

	// CoreLoopSubsystem을 Train 페이즈로 전환
	if (UCoreLoopSubsystem* CoreLoop = GetCoreLoopSubsystem())
	{
		CoreLoop->TransitionToTrain();
	}

	SetTrainState(ETrainState::Boarding);
	UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] Boarding started. Duration=%.1fs"), BoardingDuration);
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
	OnDestinationSelectionOpened.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] Train departing. Destination selection UI should open."));
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

	SelectedDestination = Destination;
	bDestinationSelected = true;

	// CoreLoopSubsystem에 목적지 등록
	if (UCoreLoopSubsystem* CoreLoop = GetCoreLoopSubsystem())
	{
		CoreLoop->SelectTrainDestination(Destination);
	}

	UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] Destination selected: %s"), *Destination.DestinationId.ToString());

	// 이미 출발 완료 상태(Departing 끝남)면 즉시 Traveling으로
	if (TrainState == ETrainState::Departing || TrainState == ETrainState::Boarding)
	{
		// Departing 중이면 목적지 선택 완료 — Departing 끝나면 자동으로 Traveling 전환
		// Boarding 중이면 즉시 출발
		if (TrainState == ETrainState::Boarding)
		{
			Depart();
		}
	}
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
	PhaseElapsedTime = 0.0f;
	TravelElapsedTime = 0.0f;

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
		OnDestinationSelectionOpened.Broadcast();
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

	OnTrainStateChanged.Broadcast(OldState, NewState);

	UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] State: %d -> %d"), static_cast<int32>(OldState), static_cast<int32>(NewState));
}

void UTrainTravelComponent::HandleBoardingComplete()
{
	Depart();
}

void UTrainTravelComponent::HandleDepartureComplete()
{
	if (bDestinationSelected)
	{
		// 목적지 선택 완료 — 이동 시작
		TravelElapsedTime = 0.0f;
		SetTrainState(ETrainState::Traveling);
		UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] Traveling to %s. Duration=%.1fs"), *SelectedDestination.DestinationId.ToString(), TravelDuration);
	}
	else
	{
		// 목적지 미선택 — Departing 상태 유지하며 대기
		// AutoSelectTimeout이 설정되어 있으면 타임아웃 후 랜덤 선택
		UE_LOG(LogTemp, Log, TEXT("[TrainTravelComponent] Departure complete but no destination selected. Waiting..."));
		// PhaseElapsedTime을 리셋하지 않고 계속 대기
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

	World->ServerTravel(LevelPath + TEXT("?listen"), true);
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
