#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "UObject/SoftObjectPtr.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "TimerManager.h"
#include "EndingGameMode.generated.h"

class ABlasterPlayerController;


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSequenceFinished);
/**
 * 엔딩 전용 GameMode
 * - 엔딩 맵 입장 시 플레이어 스폰, 입력 차단, 블루프린트 훅 제공
 * - 모든 플레이어 로드 후 동시에 시퀀스 재생
 */
UCLASS()
class BLASTER_API AEndingGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AEndingGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void PostSeamlessTravel() override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	UFUNCTION(BlueprintCallable)
	void PlayEndingSequence();

	/** 크레딧 종료 시 PlayerController에서 호출 (블루프린트 ServerRequestTravelAfterCredits → 이 함수) */
	void RequestTravelAfterCredits();

	/** 플레이어가 엔딩 레벨 로드 완료 알림 시 호출 */
	void OnPlayerEndingLevelReady(ABlasterPlayerController* PC);

protected:
	/** 엔딩 맵에서 사용할 전용 Pawn 클래스 (없으면 기존 기본 Pawn 사용) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ending")
	TSubclassOf<APawn> EndingPawnClass;

	UPROPERTY(EditDefaultsOnly, Category = "Ending", meta=(AllowedClasses="LevelSequence"))
	TSoftObjectPtr<ULevelSequence> EndingSequenceAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Ending")
	FMovieSceneSequencePlaybackSettings PlaybackSettings;

	/** 입장 시 컨트롤러 입력 차단 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ending")
	bool bBlockInputOnLogin = true;

	/** 시퀀스 시작 시 플레이어 숨김 처리 (HideForSequence 연계) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ending")
	bool bHidePlayersOnSequenceStart = true;

	/** true: 자신 포함 모두 숨김, false: 다른 플레이어만 숨김 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ending", meta = (EditCondition = "bHidePlayersOnSequenceStart"))
	bool bIncludeSelfInHide = true;

	/** 모든 플레이어 준비 대기 시간 (초). 이 시간 후 한 번에 시퀀스 재생 요청 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ending", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float EndingLevelReadyWaitTime = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ending")
	FString LobbyTravelPath = TEXT("/Game/Maps/EntryMap");

	UPROPERTY()
	ULevelSequencePlayer* SequencePlayer;
	UPROPERTY()
	ALevelSequenceActor* SequenceActor;

	UFUNCTION(BlueprintImplementableEvent, Category = "Ending")
	void OnEndingPlayerReadyBP(ABlasterPlayerController* PlayerController, const TSoftObjectPtr<ULevelSequence>& EndingSequence, FMovieSceneSequencePlaybackSettings Playback);

	UFUNCTION(BlueprintCallable, Category = "Ending")
	void TravelToMap(const FString& MapPath);

	void OnEndingPlayerReady(ABlasterPlayerController* PlayerController);

	UFUNCTION()
	void OnSequenceFinished();

	UPROPERTY(BlueprintAssignable)
	FOnSequenceFinished OnSequenceFinishedDelegate;

private:
	void ApplyEndingSetup(ABlasterPlayerController* PlayerController);
	void RequestReadyFromAllPlayers();
	void CheckAllPlayersReadyAndPlaySequence();

	bool bTravelAfterCreditsRequested = false;
	bool bEndingSequenceStarted = false;
	TSet<TWeakObjectPtr<ABlasterPlayerController>> EndingLevelReadyPCs;
	FTimerHandle EndingReadyTimerHandle;
};

