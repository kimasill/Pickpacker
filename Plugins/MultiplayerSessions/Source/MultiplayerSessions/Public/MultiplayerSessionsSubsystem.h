// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSubsystem.h"
#include "MultiplayerSessionsSubsystem.generated.h"

// Forward declarations for the delegates we will use

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnCreateSessionComplete, bool, bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerOnJoinSessionComplete, EOnJoinSessionCompleteResult::Type Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnDestroySessionComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerOnStartSessionComplete, bool, bWasSuccessful);

UENUM(BlueprintType)
enum class ESessionVisibility : uint8
{
	Private     UMETA(DisplayName = "Private"),      // 검색 불가, 초대 전용
	Friends     UMETA(DisplayName = "Friends"),      // 검색 불가, 친구 합류 허용
	InviteOnly  UMETA(DisplayName = "InviteOnly"),   // 검색 불가, 초대 전용
	Public      UMETA(DisplayName = "Public")        // 검색 가능
};
/**
 * 
 */
UCLASS(Config=Game)
class MULTIPLAYERSESSIONS_API UMultiplayerSessionsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UMultiplayerSessionsSubsystem();
	IOnlineSubsystem* GetOnlineSubsystem() const;
	FName GetSubsystemName() const;
	void CreateSession(int32 NumPublicConnections, FString MatchType, const FString& SessionTitle = TEXT(""), ESessionVisibility Visibility = ESessionVisibility::Private, const FString& SelectedMap = TEXT(""), const FString& GameMode = TEXT(""));
	void FindSessions(int32 MaxSearchResults);
	void JoinSession(const FOnlineSessionSearchResult& SessionResult);
	void StartSession();
	void DestroySession();
	void UpdateSessionVisibility(ESessionVisibility NewVisibility);
	void UpdateSessionSettings(int32 NumPublicConnections, const FString& MatchType, const FString& SessionTitle, ESessionVisibility Visibility, const FString& SelectedMap, const FString& GameMode);

	/** 호스트 IP:포트를 세션에 저장 (GetAddressInfo 실패 회피 - Steam ID 대신 IP로 연결) */
	void UpdateSessionHostAddress(const FString& HostAddressPort);

	/** 기존 세션이 유효한지 확인 (불필요한 Destroy→Create 방지) */
	bool HasActiveSession() const;

	// Delegate to bind to the CreateSession function
	FMultiplayerOnCreateSessionComplete MultiplayerOnCreateSessionComplete;
	FMultiplayerOnFindSessionsComplete MultiplayerOnFindSessionsComplete;
	FMultiplayerOnJoinSessionComplete MultiplayerOnJoinSessionComplete;
	FMultiplayerOnDestroySessionComplete MultiplayerOnDestroySessionComplete;
	FMultiplayerOnStartSessionComplete MultiplayerOnStartSessionComplete;
protected:
	// Internal callbacks for the delegates we bind to the Online Session Interface.
	// These don't need to be called outside this class.
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	bool IsValidSessionInterface();
private:
	IOnlineSessionPtr SessionInterface;

	UPROPERTY(Config)
	bool bUseNullSubsystemInEditor = true;

	/** true면 BuildUniqueId를 0으로 고정하여 Development/Shipping 빌드 간 세션 검색 호환 */
	UPROPERTY(Config, EditAnywhere, Category = "Session")
	bool bForceCrossBuildCompatible = true;

	// To add th the Online Session Interface Delegate list.
	// we'll bind our MultiplayerSessionsSubsystem to these delegates
	TSharedPtr<FOnlineSessionSettings> LastSessionSettings;
	TSharedPtr<FOnlineSessionSearch> LastSessionSearch;

	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FDelegateHandle CreateSessionCompleteDelegateHandle;
	FOnFindSessionsCompleteDelegate FindSessionCompleteDelegate;
	FDelegateHandle FindSessionCompleteDelegateHandle;
	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	FOnStartSessionCompleteDelegate StartSessionCompleteDelegate;
	FDelegateHandle StartSessionCompleteDelegateHandle;
	FOnDestroySessionCompleteDelegate DestroySessionCompleteDelegate;
	FDelegateHandle DestroySessionCompleteDelegateHandle;

	bool bCreateSessionOnDestroy{ false }; // Flag to check if we need to create a session after destroying one
	int32 LastNumPublicConnections; // Store the last number of public connections for session creation
	FString LastMatchType; // Store the last match type for session creation
	FString DesiredSessionTitle; // Store the desired session title for the next session creation
	ESessionVisibility LastSessionVisibility = ESessionVisibility::Public; // Store visibility for recreation
	FString LastSelectedMap;
	FString LastGameMode;

public:
	int32 DesiredNumPublicConnections{ }; // Desired number of public connections for the next session creation
	FString DesiredMatchType{ }; // Desired match type for the next session creation
	ESessionVisibility DesiredSessionVisibility = ESessionVisibility::Public; // Desired visibility for the next session creation
	FString DesiredSelectedMap;
	FString DesiredGameMode;

	// Utility to read session title from search result
	static FString ExtractSessionTitle(const FOnlineSessionSearchResult& SessionResult);
	/** SessionSettings에서 제목 추출 (Base64 포함, 한글 지원) */
	static FString ExtractSessionTitleFromSettings(const FOnlineSessionSettings& SessionSettings);
};
