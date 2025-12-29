// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Menu.generated.h"

USTRUCT(BlueprintType)
struct FSessionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString SessionId;

	UPROPERTY(BlueprintReadOnly)
	FString SessionTitle;

	UPROPERTY(BlueprintReadOnly)
	FString MatchType;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 PingInMs = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 SearchResultIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	ESessionVisibility Visibility = ESessionVisibility::Private;

	UPROPERTY(BlueprintReadOnly)
	FString RoomName;

	UPROPERTY(BlueprintReadOnly)
	FString SelectedMap;

	UPROPERTY(BlueprintReadOnly)
	FString GameMode;

	UPROPERTY(BlueprintReadOnly)
	FString InviteCode;
};

/**
 * 
 */
UCLASS()
class MULTIPLAYERSESSIONS_API UMenu : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void MenuSetup(int32 NumberOfPublicConnections = 4, FString TypeOfMatch = FString(TEXT("Industral")), FString LobbyPath = FString(TEXT("/Game/ThirdPerson/Maps/Lobby?listen")));

protected:
	virtual bool Initialize() override;
	virtual void NativeDestruct() override;

	
	// callbacks for the custom delegates on the MultiplayerSessionsSubsystem
	UFUNCTION()
	void OnCreateSession(bool bWasSuccessful);
	void OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);
	void OnJoinSession(EOnJoinSessionCompleteResult::Type Result);
	UFUNCTION()
	void OnStartSession(bool bWasSuccessful);
	UFUNCTION()
	void OnDestroySession(bool bWasSuccessful);

	// UI update hooks (Blueprint can implement)
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu|Sessions")
	void OnSessionListUpdated(const TArray<FSessionInfo>& SessionInfos);

	UFUNCTION(BlueprintImplementableEvent, Category = "Menu|Sessions")
	void OnSessionListCleared();

	UFUNCTION(BlueprintImplementableEvent, Category = "Menu|Lobby")
	void OnLobbyShown();

	UFUNCTION(BlueprintImplementableEvent, Category = "Menu|Lobby")
	void OnPlayerListUpdated(const TArray<FString>& PlayerNames);

	// Blueprint can call to refresh lobby UI
	UFUNCTION(BlueprintCallable, Category = "Menu|Lobby")
	void RefreshLobbyPlayers();

	UFUNCTION(BlueprintCallable, Category = "Menu|Lobby")
	void LeaveLobby();

	UFUNCTION(BlueprintCallable, Category = "Menu|Lobby")
	void StartGame();

	// Show lobby UI (call this when entering lobby level)
	UFUNCTION(BlueprintCallable, Category = "Menu|Lobby")
	void ShowLobby();

	// Blueprint can call these functions directly
	UFUNCTION(BlueprintCallable, Category = "Menu|Sessions")
	void CreateSession(const FString& SessionTitle = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Menu|Sessions")
	void FindSessions();

	UFUNCTION(BlueprintCallable, Category = "Menu|Sessions")
	void RefreshSessionList();

	// Session list interaction (Blueprint can call)
	UFUNCTION(BlueprintCallable, Category = "Menu|Sessions")
	void JoinSessionByIndex(int32 SessionIndex);

	UFUNCTION(BlueprintCallable, Category = "Menu|Sessions")
	void UpdateSessionVisibility(ESessionVisibility NewVisibility);

	UFUNCTION(BlueprintCallable, Category = "Menu|Sessions")
	void UpdateRoomSettings(const FString& RoomName, const FString& GameMode, const FString& MapPath, int32 MaxPlayers);

	UFUNCTION(BlueprintCallable, Category = "Menu|Sessions")
	FString GetSessionInviteCode() const;

	UFUNCTION(BlueprintCallable, Category = "Menu|Lobby")
	void SetReadyStatus(bool bReady);

	UFUNCTION(BlueprintCallable, Category = "Menu|Sessions")
	FSessionInfo GetCurrentSessionInfo() const;

	// 메인 메뉴에서 바로 로비 맵으로 이동 (세션 없이)
	UFUNCTION(BlueprintCallable, Category = "Menu|Navigation")
	void StartGameDirectly(const FString& LobbyMapPath = TEXT(""));

private:

	void MenuTearDown();

	// The subsystem that handles multiplayer sessions
	class UMultiplayerSessionsSubsystem* MultiplayerSessionsSubsystem;

	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	int32 NumPublicConnections{4}; // Default number of connections

	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FString MatchType{ TEXT("FreeForAll") }; // Default match type
	FString PathToLobby{ TEXT("") }; // Default path to the lobby map

	// Cache of search results to join by index
	TArray<FOnlineSessionSearchResult> LastSessionSearchResults;
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TArray<FSessionInfo> LastSessionInfos;

	// CreateSession 성공 처리 여부 (추가 실패 콜백 무시용)
	bool bHasHandledCreateSessionSuccess = false;

	// 서브시스템 보장용 헬퍼 (레벨 전환 후 nullptr 방지)
	UMultiplayerSessionsSubsystem* EnsureMultiplayerSubsystem();

	// Helpers for lobby mode
	void UpdatePlayerListInternal();
	void ShowLobbyInternal();
};
