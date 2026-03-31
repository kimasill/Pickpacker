// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "LobbyGameMode.generated.h"

enum class ESessionVisibility : uint8;

UCLASS()
class BLASTER_API ALobbyGameMode : public AGameMode
{
	GENERATED_BODY()
public:
	ALobbyGameMode();
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void BeginPlay() override;
	virtual void PostSeamlessTravel() override;

	// 수동 시작 함수 (호스트 버튼 등에서 호출)
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void StartGameManually();

	// 로비 진입 시 자동 세션 생성
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void AutoCreateLobbySession();

	// 플레이어 준비 상태 설정
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void SetPlayerReadyStatus(APlayerController* PlayerController, bool bReady);

	// 모든 플레이어 준비 여부
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	bool CanStartGame() const;

	// 플레이어 준비 상태 확인
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	bool IsPlayerReady(const FString& PlayerId) const;

	// 모든 플레이어의 OverHeadWidget 업데이트
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void UpdateAllPlayerOverheadWidgets();

	// Room 세팅 업데이트
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void UpdateRoomSettings(int32 MaxPlayers, const FString& MatchType, const FString& SessionTitle,
		ESessionVisibility Visibility, const FString& SelectedMap, const FString& GameMode);

	// 로비용 Pawn 클래스 반환
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	// 플레이어 퇴장 처리
	virtual void Logout(AController* Exiting) override;

protected:
	// EntryMap 경로 (시퀀스 재생 후 메인 레벨로 이동)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby Settings")
	FString EntryMapPath = TEXT("/Game/Maps/EntryMap");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby Settings")
	bool bAutoCreateSessionOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby Settings")
	int32 DefaultLobbyMaxPlayers = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby Settings")
	FString DefaultMatchType = TEXT("Industral");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby Settings")
	FString DefaultLobbyMap = TEXT("/Game/Maps/IndustralMap");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby Settings")
	ESessionVisibility DefaultLobbyVisibility;

	/** 로비에서 사용할 기본 Pawn 클래스 (BlasterCharacter) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby Settings")
	TSubclassOf<APawn> LobbyPawnClass;

	// 레벨 전환 시 페이드 연출 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby Settings")
	float TravelFadeDuration = 0.35f;

private:
	bool bGameStarting = false;

	UPROPERTY()
	TSet<FString> ReadyPlayers;

	// 준비 인원 집계 및 자동 시작 처리
	void UpdateReadyCountsAndMaybeStart();

	// 게임 시작 시 사용할 대상 맵을 결정
	FString ResolveTargetMap() const;

	// 호스트 IP를 세션에 저장 (참가 시 IP로 연결 가능하도록)
	void TryUpdateSessionHostAddress();

	// 레벨 트래블용 페이드 및 지연 트래블 처리
	void StartFadeOnAllPlayers(bool bFadeOut) const;
	void DoServerTravelWithFade(const FString& TravelPath);

	FTimerHandle TravelTimerHandle;
};
