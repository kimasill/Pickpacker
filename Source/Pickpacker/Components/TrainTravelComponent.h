// TrainTravelComponent — 열차 탑승, 이동 연출, 목적지 도착을 관리하는 컴포넌트
// GameState에 부착하여 서버 권한으로 열차 상태를 제어하고 클라이언트에 리플리케이트

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PickpackerTypes/CoreLoopTypes.h"
#include "TrainTravelComponent.generated.h"

class UCoreLoopSubsystem;
class APlayerState;
class AParcelActor;

/** 열차 이동 상태 */
UENUM(BlueprintType)
enum class ETrainState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	Boarding	UMETA(DisplayName = "Boarding"),
	Departing	UMETA(DisplayName = "Departing"),
	Traveling	UMETA(DisplayName = "Traveling"),
	Arriving	UMETA(DisplayName = "Arriving"),
	Arrived		UMETA(DisplayName = "Arrived")
};

UENUM(BlueprintType)
enum class ETrainJourneyType : uint8
{
	None			UMETA(DisplayName = "None"),
	OutboundMission	UMETA(DisplayName = "Outbound Mission"),
	ReturnToBase	UMETA(DisplayName = "Return To Base"),
	Escape			UMETA(DisplayName = "Escape")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTrainStateChanged, ETrainState, OldState, ETrainState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrainTravelProgress, float, NormalizedProgress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTrainArrived);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTrainDeparted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDestinationSelectionOpened);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrainSelectionContextUpdated, const FTrainSelectionContext&, SelectionContext);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrainDestinationVotesUpdated, const TArray<FTrainDestinationVoteState>&, VoteStates);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRouteSelectionResultUpdated, const FRouteSelectionResult&, RouteSelectionResult);

/**
 * Train Travel Component
 *
 * 열차 탑승 → 출발 → 이동 중 UI로 목적지 선택 → 도착 → 레벨 입장 흐름을 관리.
 * GameState에 부착하여 모든 클라이언트에 열차 상태를 동기화.
 *
 * 서버 흐름:
 *  1. BeginBoarding() — 플레이어들이 열차에 탑승 (BoardingDuration 후 자동 출발)
 *  2. Depart() — 출발 연출, 목적지 선택 UI 오픈 트리거
 *  3. SelectDestination() — 목적지 확정, 이동 시작
 *  4. 이동 중 TravelDuration 경과 → Arrive()
 *  5. Arrive() — 도착 연출 후 ServerTravel로 레벨 전환
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PICKPACKER_API UTrainTravelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTrainTravelComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- API (서버 전용) --------------------------------------------------

	/** 열차 탑승 시작 — BoardingDuration 후 자동 출발 */
	UFUNCTION(BlueprintCallable, Category = "Train")
	void BeginBoarding();

	/** 즉시 출발 (탑승 대기 스킵) */
	UFUNCTION(BlueprintCallable, Category = "Train")
	void Depart();

	/** 귀환 열차 탑승 시작 — 수동 출발 전까지 Boarding 상태 유지 */
	UFUNCTION(BlueprintCallable, Category = "Train")
	void BeginReturnBoarding();

	/** 목적지 선택 확정 — 이동 시작 */
	UFUNCTION(BlueprintCallable, Category = "Train")
	void SelectDestination(const FTrainDestination& Destination);

	/** 플레이어의 목적지 투표를 집계한다 (서버 전용) */
	UFUNCTION(BlueprintCallable, Category = "Train")
	void SubmitDestinationVote(APlayerState* PlayerState, FName DestinationId);

	/** 강제 도착 (디버그/테스트용) */
	UFUNCTION(BlueprintCallable, Category = "Train")
	void ForceArrive();

	/** 열차 상태 초기화 (Base 복귀 시) */
	UFUNCTION(BlueprintCallable, Category = "Train")
	void ResetTrain();

	// --- 조회 ------------------------------------------------------------

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Train")
	ETrainState GetTrainState() const { return TrainState; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Train")
	float GetTravelProgress() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Train")
	float GetRemainingTravelTime() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Train")
	bool IsDestinationSelected() const { return bDestinationSelected; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Train")
	const FTrainDestination& GetSelectedDestination() const { return SelectedDestination; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Train")
	const FTrainSelectionContext& GetSelectionContext() const { return SelectionContext; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Train")
	const TArray<FTrainDestinationVoteState>& GetDestinationVotes() const { return DestinationVotes; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Train")
	const FRouteSelectionResult& GetRouteSelectionResult() const { return RouteSelectionResult; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Train")
	ETrainJourneyType GetJourneyType() const { return JourneyType; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Train|Cargo")
	const TArray<FStorageRecord>& GetCargoRecords() const { return CargoRecords; }

	UFUNCTION(BlueprintCallable, Category = "Train|Cargo")
	void SetCargoRecords(const TArray<FStorageRecord>& NewCargoRecords);

	UFUNCTION(BlueprintCallable, Category = "Train|Cargo")
	void ClearCargoRecords();

	UFUNCTION(BlueprintCallable, Category = "Train|Cargo")
	bool LoadParcelIntoCargo(AParcelActor* Parcel);

	// --- 이벤트 ----------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Train|Events")
	FOnTrainStateChanged OnTrainStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Train|Events")
	FOnTrainTravelProgress OnTravelProgress;

	UPROPERTY(BlueprintAssignable, Category = "Train|Events")
	FOnTrainArrived OnTrainArrived;

	UPROPERTY(BlueprintAssignable, Category = "Train|Events")
	FOnTrainDeparted OnTrainDeparted;

	/** 목적지 선택 UI를 열어야 할 때 브로드캐스트 */
	UPROPERTY(BlueprintAssignable, Category = "Train|Events")
	FOnDestinationSelectionOpened OnDestinationSelectionOpened;

	UPROPERTY(BlueprintAssignable, Category = "Train|Events")
	FOnTrainSelectionContextUpdated OnSelectionContextUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Train|Events")
	FOnTrainDestinationVotesUpdated OnDestinationVotesUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Train|Events")
	FOnRouteSelectionResultUpdated OnRouteSelectionResultUpdated;

	// --- 설정 ------------------------------------------------------------

	/** 탑승 대기 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Config", meta = (ClampMin = "0.0"))
	float BoardingDuration = 15.0f;

	/** 출발 연출 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Config", meta = (ClampMin = "0.0"))
	float DepartureDuration = 5.0f;

	/** 이동 시간 (초) — 목적지 선택 후 도착까지 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Config", meta = (ClampMin = "1.0"))
	float TravelDuration = 30.0f;

	/** 도착 연출 시간 (초) — 도착 후 레벨 전환까지 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Config", meta = (ClampMin = "0.0"))
	float ArrivalDuration = 5.0f;

	/** 목적지 미선택 시 자동 선택까지 대기 시간 (0 = 무제한 대기) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Config", meta = (ClampMin = "0.0"))
	float AutoSelectTimeout = 30.0f;

	/** 다수결 동률 시 적용할 정책 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Config")
	ETrainVoteResolutionPolicy VoteResolutionPolicy = ETrainVoteResolutionPolicy::MajorityThenHost;

	/** 귀환 열차가 도착할 베이스 레벨 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Config")
	TSoftObjectPtr<UWorld> ReturnBaseLevel;

	/** 귀환 여정은 열차 조작 전까지 자동 출발하지 않는다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Config")
	bool bRequireManualDepartureForReturnJourney = true;

protected:
	UFUNCTION()
	void OnRep_TrainState(ETrainState OldState);

	UFUNCTION()
	void OnRep_SelectedDestination();

	UFUNCTION()
	void OnRep_SelectionContext();

	UFUNCTION()
	void OnRep_DestinationVotes();

	UFUNCTION()
	void OnRep_RouteSelectionResult();

	void SetTrainState(ETrainState NewState);
	void HandleBoardingComplete();
	void HandleDepartureComplete();
	void HandleTravelComplete();
	void HandleArrivalComplete();
	void ExecuteLevelTravel();
	void OpenDestinationSelection();
	bool TryAutoSelectDestination();
	void StartTraveling();
	void RefreshSelectionContext();
	void RefreshSelectionVotes();
	void EvaluateDestinationVotes();
	bool TryResolveVoteWinner(FTrainDestination& OutDestination) const;
	bool TryResolveHostPreferredDestination(FTrainDestination& OutDestination, const TArray<FTrainDestination>& CandidateDestinations) const;
	int32 GetEligibleVoterCount() const;
	FRouteSelectionResult BuildRouteSelectionResult(const FTrainDestination& Destination) const;
	FTrainDestination BuildReturnBaseDestination() const;
	const FTrainDestination* FindAvailableDestinationById(FName DestinationId) const;
	void ClearDestinationVotes();
	bool ShouldOpenDestinationSelection() const;
	bool ShouldAutoDepartFromBoarding() const;

	bool HasAuthority() const;
	UCoreLoopSubsystem* GetCoreLoopSubsystem() const;

private:
	UPROPERTY(ReplicatedUsing = OnRep_TrainState)
	ETrainState TrainState = ETrainState::Idle;

	UPROPERTY(ReplicatedUsing = OnRep_SelectedDestination)
	FTrainDestination SelectedDestination;

	UPROPERTY(Replicated)
	bool bDestinationSelected = false;

	UPROPERTY(ReplicatedUsing = OnRep_SelectionContext)
	FTrainSelectionContext SelectionContext;

	UPROPERTY(ReplicatedUsing = OnRep_DestinationVotes)
	TArray<FTrainDestinationVoteState> DestinationVotes;

	UPROPERTY(ReplicatedUsing = OnRep_RouteSelectionResult)
	FRouteSelectionResult RouteSelectionResult;

	/** 현재 페이즈 경과 시간 */
	float PhaseElapsedTime = 0.0f;

	/** 이동 경과 시간 (Traveling 상태에서만 증가) */
	UPROPERTY(Replicated)
	float TravelElapsedTime = 0.0f;

	/** 출발 연출이 끝나고 목적지 선택만 대기 중인지 */
	bool bWaitingForDestinationAfterDeparture = false;

	UPROPERTY(Replicated)
	ETrainJourneyType JourneyType = ETrainJourneyType::None;

	UPROPERTY(Replicated)
	TArray<FStorageRecord> CargoRecords;
};
