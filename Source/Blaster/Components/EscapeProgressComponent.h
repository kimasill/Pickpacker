#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "EscapeProgressComponent.generated.h"

class UDA_EndingData;
class APlayerState;
class ULevelSequence;
class ABlasterPlayerController;

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
class BLASTER_API UEscapeProgressComponent : public UActorComponent
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

protected:
	UFUNCTION()
	void OnRep_CurrentEndingId();

	/** 엔딩 시작 메서드 (서버 전용) */
	void StartEnding(const UDA_EndingData* EndingData);

	bool IsEndingConditionMet(const UDA_EndingData* EndingData) const;

	void BroadcastInputBlock(bool bBlocked);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayEndingSequence(ULevelSequence* Sequence);

protected:
	UPROPERTY(EditAnywhere, Category = "EscapeProgress|Data")
	TArray<TObjectPtr<UDA_EndingData>> EndingDataAssets;

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

	FWorldFlagEntry* FindWorldFlagEntry(const FGameplayTag& Flag);
	const FWorldFlagEntry* FindWorldFlagEntry(const FGameplayTag& Flag) const;
};


