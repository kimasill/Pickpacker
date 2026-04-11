#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "EscapeProgressComponent.generated.h"

class UDA_EndingData;
class APlayerState;
class ULevelSequence;
class ULevelSequencePlayer;
class ABlasterPlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSequenceStarted, ULevelSequence*, Sequence);

USTRUCT(BlueprintType)
struct FEscapeRouteProgress
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName RouteId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CurrentStepIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCompleted = false;
};

USTRUCT(BlueprintType)
struct FWorldFlagEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag Flag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Value = 0;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PICKPACKER_API UEscapeProgressComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEscapeProgressComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 월드 플래그 설정 (서버 전용) */
	UFUNCTION(BlueprintCallable, Category = "EscapeProgress")
	void SetWorldFlag(const FGameplayTag& Flag, int32 Value);

	/** 월드 플래그 조회 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EscapeProgress")
	int32 GetWorldFlag(const FGameplayTag& Flag) const;

	/** 월드 플래그 존재 여부 및 최소값 확인 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EscapeProgress")
	bool IsWorldFlagAtLeast(const FGameplayTag& Flag, int32 MinValue) const;

	/** 월드 플래그가 범위 내인지 확인 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EscapeProgress")
	bool IsWorldFlagInRange(const FGameplayTag& Flag, int32 MinValue, int32 MaxValue) const;

	/** 인증 플레이어 추가 (중복 방지) */
	UFUNCTION(BlueprintCallable, Category = "EscapeProgress")
	void AddAuthorizedPlayer(APlayerState* PlayerState);

	/** 엔딩 조건 평가 (서버 전용) */
	UFUNCTION(BlueprintCallable, Category = "EscapeProgress")
	void EvaluateEndings();

	/** 특정 엔딩을 ID로 강제 시작 (서버 전용) */
	UFUNCTION(BlueprintCallable, Category = "EscapeProgress")
	bool StartEndingById(const FName& EndingId);

	/** 현재 시작된 엔딩 ID */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EscapeProgress")
	FName GetCurrentEndingId() const { return CurrentEndingId; }
	
	UPROPERTY(BlueprintAssignable, Category = "EscapeProgress")
	FOnSequenceStarted OnSequenceStarted;

protected:
	UFUNCTION()
	void OnRep_CurrentEndingId();

	/** 엔딩 시작 메서드 (서버 전용) */
	void StartEnding(const UDA_EndingData* EndingData);

	bool IsEndingConditionMet(const UDA_EndingData* EndingData) const;

	void BroadcastInputBlock(bool bBlocked);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayEndingSequence(ULevelSequence* Sequence);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayTransitionSequence(ULevelSequence* Sequence);

	/** 전환 시퀀스 완료 후 레벨 전환 실행 */
	UFUNCTION()
	void OnTransitionSequenceFinished();
protected:
	UPROPERTY(EditAnywhere, Category = "EscapeProgress|Data")
	TArray<TObjectPtr<UDA_EndingData>> EndingDataAssets;

	/** 엔딩 후 로비 복귀 (엔딩 맵 이동 없이 시퀀스만 재생할 때) */
	UPROPERTY(EditAnywhere, Category = "EscapeProgress|Ending")
	bool bReturnToLobbyAfterEnding = true;

	UPROPERTY(EditAnywhere, Category = "EscapeProgress|Ending")
	float PostEndingDelay = 0.5f;

	UPROPERTY(EditAnywhere, Category = "EscapeProgress|Ending")
	FString LobbyTravelPath = TEXT("/Game/Maps/EntryMap");

	/** 월드 상태 플래그 */
	UPROPERTY(Replicated)
	TArray<FWorldFlagEntry> WorldFlags;

	/** 루트별 진행도 */
	UPROPERTY(Replicated)
	TArray<FEscapeRouteProgress> RouteProgress;

	/** 인증된 플레이어 수 (중복 제거) */
	UPROPERTY(Replicated)
	int32 AuthorizedPlayerCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentEndingId)
	FName CurrentEndingId = NAME_None;

private:
	TSet<TWeakObjectPtr<APlayerState>> AuthorizedPlayers;

	/** 전환 시퀀스 재생 후 레벨 전환을 위한 데이터 저장 */
	FString PendingLevelPath;
	TObjectPtr<ULevelSequence> PendingEndingSequence = nullptr;
	TObjectPtr<ULevelSequencePlayer> CurrentSequencePlayer = nullptr;

	FWorldFlagEntry* FindWorldFlagEntry(const FGameplayTag& Flag);
	const FWorldFlagEntry* FindWorldFlagEntry(const FGameplayTag& Flag) const;

	float GetSequenceDuration(ULevelSequence* Sequence) const;
	void ScheduleReturnToLobby(float TotalDelay);
	void ReturnPlayersToLobby();

	FTimerHandle ReturnToLobbyTimerHandle;
	bool bReturnScheduled = false;
};















