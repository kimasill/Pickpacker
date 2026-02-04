#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "UObject/SoftObjectPtr.h"
#include "LevelSequence.h"

#include "EndingGameMode.generated.h"

class ABlasterPlayerController;

/**
 * 엔딩 전용 GameMode
 * - 엔딩 맵 입장 시 플레이어 스폰, 입력 차단, 블루프린트 훅 제공
 */
UCLASS()
class BLASTER_API AEndingGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AEndingGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

protected:
	/** 엔딩 맵에서 사용할 전용 Pawn 클래스 (없으면 기존 기본 Pawn 사용) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ending")
	TSubclassOf<APawn> EndingPawnClass;

	UPROPERTY(EditDefaultsOnly, Category = "Ending")
	TSoftObjectPtr<ULevelSequence> EndingSequenceAsset;

	/** 입장 시 컨트롤러 입력 차단 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ending")
	bool bBlockInputOnLogin = true;

	/** 엔딩 후 로비로 복귀 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ending")
	bool bReturnToLobbyAfterEnding = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ending")
	float PostEndingDelay = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ending")
	FString LobbyTravelPath = TEXT("/Game/Maps/EntryMap");

	void OnEndingPlayerReady(ABlasterPlayerController* PlayerController);

private:
	void ApplyEndingSetup(ABlasterPlayerController* PlayerController);
	float GetEndingSequenceDuration() const;
	void ReturnPlayersToLobby();

	FTimerHandle ReturnToLobbyTimerHandle;
	bool bReturnScheduled = false;
};

