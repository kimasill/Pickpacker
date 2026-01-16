// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "MultiplayerSessionsSubsystem.h"
#include "LobbyGameState.generated.h"

/**
 * 로비 세팅 정보 구조체
 */
USTRUCT(BlueprintType)
struct FLobbySettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 MaxPlayers = 4;

	UPROPERTY(BlueprintReadOnly)
	FString MatchType = TEXT("Industral");

	UPROPERTY(BlueprintReadOnly)
	FString SessionTitle = TEXT("");

	UPROPERTY(BlueprintReadOnly)
	ESessionVisibility Visibility = ESessionVisibility::Private;

	UPROPERTY(BlueprintReadOnly)
	FString SelectedMap = TEXT("");

	UPROPERTY(BlueprintReadOnly)
	FString GameMode = TEXT("");

	FLobbySettings()
		: MaxPlayers(4)
		, MatchType(TEXT("Industral"))
		, SessionTitle(TEXT(""))
		, Visibility(ESessionVisibility::Private)
		, SelectedMap(TEXT(""))
		, GameMode(TEXT(""))
	{}
};


UCLASS()
class BLASTER_API ALobbyGameState : public AGameState
{
	GENERATED_BODY()

public:
	ALobbyGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Room 세팅 업데이트 (서버에서만 호출)
	 */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void UpdateRoomSettings(int32 MaxPlayers, const FString& MatchType, const FString& SessionTitle, 
		ESessionVisibility Visibility, const FString& SelectedMap, const FString& GameMode);

	/**
	 * 현재 Room 세팅 가져오기
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lobby")
	FLobbySettings GetRoomSettings() const { return RoomSettings; }

	/**
	 * 플레이어 준비 상태 변경 알림 (모든 클라이언트에 브로드캐스트)
	 */
	UFUNCTION(NetMulticast, Reliable, Category = "Lobby")
	void MulticastPlayerReadyStatusChanged(const FString& PlayerId, bool bIsReady);

	/** 클라이언트 전체 페이드 */
	UFUNCTION(NetMulticast, Reliable, Category = "Lobby")
	void MulticastStartFadeOnPlayers(bool bFadeOut, float Duration);

	/** 준비 인원/전체 인원 수 반환 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lobby")
	int32 GetReadyPlayerCount() const { return ReadyPlayerCount; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lobby")
	int32 GetTotalPlayerCount() const { return TotalPlayerCount; }

	/** 서버 전용: 준비 인원 수 갱신 */
	void SetReadyCounts(int32 InReadyPlayerCount, int32 InTotalPlayerCount);

	/**
	 * 플레이어 참가 알림 (모든 클라이언트에 브로드캐스트)
	 */
	UFUNCTION(NetMulticast, Reliable, Category = "Lobby")
	void MulticastPlayerJoined(const FString& PlayerName);

	/**
	 * 플레이어 퇴장 알림 (모든 클라이언트에 브로드캐스트)
	 */
	UFUNCTION(NetMulticast, Reliable, Category = "Lobby")
	void MulticastPlayerLeft(const FString& PlayerName);

	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby")
	void OnFaded();

	/**
	 * Blueprint 이벤트: Room 세팅 업데이트 시 호출
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby")
	void OnRoomSettingsUpdated();

	/**
	 * Blueprint 이벤트: 준비 인원 변경 시 호출 (Ready/Total)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby")
	void OnReadyCountChanged(int32 NewReadyCount, int32 NewTotalCount);

	/**
	 * Blueprint 이벤트: 플레이어 참가 시 호출
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby")
	void OnPlayerJoined(const FString& PlayerName);

	/**
	 * Blueprint 이벤트: 플레이어 퇴장 시 호출
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby")
	void OnPlayerLeft(const FString& PlayerName);

protected:
	/**
	 * Room 세팅 (복제됨)
	 */
	UPROPERTY(ReplicatedUsing = OnRep_RoomSettings, BlueprintReadOnly, Category = "Lobby")
	FLobbySettings RoomSettings;

	UPROPERTY(ReplicatedUsing = OnRep_ReadyCounts, BlueprintReadOnly, Category = "Lobby")
	int32 ReadyPlayerCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_ReadyCounts, BlueprintReadOnly, Category = "Lobby")
	int32 TotalPlayerCount = 0;

	/**
	 * Room 세팅 변경 시 호출 (클라이언트)
	 */
	UFUNCTION()
	void OnRep_RoomSettings();

	UFUNCTION()
	void OnRep_ReadyCounts();

	/**
	 * 플레이어 배열 변경 시 호출
	 */
};

