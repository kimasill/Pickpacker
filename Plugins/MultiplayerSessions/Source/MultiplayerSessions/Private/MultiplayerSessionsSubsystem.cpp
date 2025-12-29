// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystemNames.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem():
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete)),
	FindSessionCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete)),
	StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete)),
	DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete))
{

}

void UMultiplayerSessionsSubsystem::CreateSession(int32 NumPublicConnections, FString MatchType, const FString& SessionTitle, ESessionVisibility Visibility, const FString& SelectedMap, const FString& GameMode)
{
	DesiredNumPublicConnections = NumPublicConnections;
	DesiredMatchType = MatchType;
	DesiredSessionTitle = SessionTitle;
	DesiredSessionVisibility = Visibility;
	DesiredSelectedMap = SelectedMap;
	DesiredGameMode = GameMode;
	if (!IsValidSessionInterface())
	{
		return;
	}
	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession != nullptr) {
		// If a session already exists, destroy it before creating a new one
		bCreateSessionOnDestroy = true;
		LastNumPublicConnections = NumPublicConnections;
		LastMatchType = MatchType;
		LastSessionVisibility = Visibility;
		LastSelectedMap = SelectedMap;
		LastGameMode = GameMode;

		DestroySession();		
	}


	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

    LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
    bool bIsNull = false;
    if (IOnlineSubsystem* OSS = IOnlineSubsystem::Get())
    {
        bIsNull = OSS->GetSubsystemName() == FName(TEXT("NULL"));
    }
	LastSessionSettings->bIsLANMatch = bIsNull; // NULL 서브시스템은 LAN으로 강제
	LastSessionSettings->NumPublicConnections = NumPublicConnections;
	LastSessionSettings->bAllowJoinInProgress = true;

	// Visibility별 광고/참여 설정
	const bool bIsPublic = Visibility == ESessionVisibility::Public;
	const bool bFriends = Visibility == ESessionVisibility::Friends;

    // LAN(Null)과 온라인을 분기하되, 광고/조인/Presence는 공통으로 활성화해 검색 가능성을 높임
    LastSessionSettings->bAllowJoinViaPresence = true;
    LastSessionSettings->bShouldAdvertise = true;                 // LAN/온라인 공통 광고
    LastSessionSettings->bUsesPresence = true;                    // Presence on (LAN/온라인)
    LastSessionSettings->bUseLobbiesIfAvailable = bIsNull ? false : true;
	LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	if (!DesiredSessionTitle.IsEmpty())
	{
		LastSessionSettings->Set(FName("SessionTitle"), DesiredSessionTitle, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	}
	if (!SelectedMap.IsEmpty())
	{
		LastSessionSettings->Set(FName("SelectedMap"), SelectedMap, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	}
	if (!GameMode.IsEmpty())
	{
		LastSessionSettings->Set(FName("GameMode"), GameMode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	}
	LastSessionSettings->Set(FName("SessionVisibility"), static_cast<int32>(Visibility), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->BuildUniqueId = 1; // Unique ID for the session, can be used to differentiate between sessions

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);

		MultiplayerOnCreateSessionComplete.Broadcast(false);		
	}
	// #region agent log
	{
		const int64 Ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();
		const FString Line = FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"run-find\",\"hypothesisId\":\"H1\",\"location\":\"MultiplayerSessionsSubsystem.cpp:CreateSession\",\"message\":\"CreateSession params\",\"data\":{\"num\":%d,\"matchType\":\"%s\",\"title\":\"%s\",\"visibility\":%d,\"advertise\":%s,\"allowJoin\":%s,\"isLan\":%s,\"usesPresence\":%s,\"useLobbies\":%s},\"timestamp\":%lld}\n"),
			NumPublicConnections, *MatchType, *SessionTitle, static_cast<int32>(Visibility),
			LastSessionSettings->bShouldAdvertise ? TEXT("true") : TEXT("false"),
			LastSessionSettings->bAllowJoinViaPresence ? TEXT("true") : TEXT("false"),
			LastSessionSettings->bIsLANMatch ? TEXT("true") : TEXT("false"),
			LastSessionSettings->bUsesPresence ? TEXT("true") : TEXT("false"),
			LastSessionSettings->bUseLobbiesIfAvailable ? TEXT("true") : TEXT("false"),
			Ms);
		FFileHelper::SaveStringToFile(Line, TEXT("s:\\\\Project\\\\Unreal5\\\\Blaster\\\\.cursor\\\\debug.log"), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
	// #endregion
}

void UMultiplayerSessionsSubsystem::FindSessions(int32 MaxSearchResults)
{
	if (!IsValidSessionInterface())
	{
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}

	// #region agent log
	{
		const int64 Ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();
		const FString SubsystemName = IOnlineSubsystem::Get() ? IOnlineSubsystem::Get()->GetSubsystemName().ToString() : TEXT("null");
		const bool bHasLP = GetWorld() && GetWorld()->GetFirstLocalPlayerFromController() && GetWorld()->GetFirstLocalPlayerFromController()->GetPreferredUniqueNetId().IsValid();
		const FString Line = FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"run-find\",\"hypothesisId\":\"H5\",\"location\":\"MultiplayerSessionsSubsystem.cpp:FindSessions\",\"message\":\"FindSessions start\",\"data\":{\"max\":%d,\"subsystem\":\"%s\",\"hasLP\":%s},\"timestamp\":%lld}\n"),
			MaxSearchResults, *SubsystemName, bHasLP ? TEXT("true") : TEXT("false"), Ms);
		FFileHelper::SaveStringToFile(Line, TEXT("s:\\\\Project\\\\Unreal5\\\\Blaster\\\\.cursor\\\\debug.log"), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
	// #endregion

	FindSessionCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionCompleteDelegate);

    LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
    LastSessionSearch->MaxSearchResults = MaxSearchResults;
    const bool bIsNull = IOnlineSubsystem::Get() && IOnlineSubsystem::Get()->GetSubsystemName() == "NULL";
    LastSessionSearch->bIsLanQuery = bIsNull;
    if (!bIsNull)
    {
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996)
#endif
        // Presence 검색 (온라인)
        LastSessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
        // Public 세션만 검색 (온라인)
        const int32 PublicVisibility = static_cast<int32>(ESessionVisibility::Public);
        LastSessionSearch->QuerySettings.Set(FName("SessionVisibility"), PublicVisibility, EOnlineComparisonOp::Equals);
    }

    const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
    if (!LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionCompleteDelegateHandle);
        MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
        return;
    }

    if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionCompleteDelegateHandle);		
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		// #region agent log
		{
			const int64 Ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();
			const FString Line = FString::Printf(
				TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"run-find\",\"hypothesisId\":\"H5\",\"location\":\"MultiplayerSessionsSubsystem.cpp:FindSessions\",\"message\":\"FindSessions failed to start\",\"data\":{\"lanQuery\":%s},\"timestamp\":%lld}\n"),
				LastSessionSearch->bIsLanQuery ? TEXT("true") : TEXT("false"),
				Ms);
			FFileHelper::SaveStringToFile(Line, TEXT("s:\\\\Project\\\\Unreal5\\\\Blaster\\\\.cursor\\\\debug.log"), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
		}
		// #endregion
	}
	// #region agent log
	{
		const int64 Ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();
		const FString Line = FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"run-find\",\"hypothesisId\":\"H5\",\"location\":\"MultiplayerSessionsSubsystem.cpp:FindSessions\",\"message\":\"FindSessions dispatched\",\"data\":{\"lanQuery\":%s,\"visibilityFilterApplied\":%s},\"timestamp\":%lld}\n"),
			LastSessionSearch->bIsLanQuery ? TEXT("true") : TEXT("false"),
			bIsNull ? TEXT("false") : TEXT("true"),
			Ms);
		FFileHelper::SaveStringToFile(Line, TEXT("s:\\\\Project\\\\Unreal5\\\\Blaster\\\\.cursor\\\\debug.log"), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
	// #endregion
}
void UMultiplayerSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
	if(!SessionInterface.IsValid()) {
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
	}
}
void UMultiplayerSessionsSubsystem::StartSession()
{
}
void UMultiplayerSessionsSubsystem::DestroySession()
{
	if(!SessionInterface.IsValid()) {
		MultiplayerOnDestroySessionComplete.Broadcast(false);
		return;
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);

	if(!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		
		MultiplayerOnDestroySessionComplete.Broadcast(false);
	}
	// #region agent log
	{
		const int64 Ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();
		const FString Line = FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"run-find\",\"hypothesisId\":\"H5\",\"location\":\"MultiplayerSessionsSubsystem.cpp:DestroySession\",\"message\":\"DestroySession called\",\"data\":{\"recreateFlag\":%s},\"timestamp\":%lld}\n"),
			bCreateSessionOnDestroy ? TEXT("true") : TEXT("false"),
			Ms);
		FFileHelper::SaveStringToFile(Line, TEXT("s:\\\\Project\\\\Unreal5\\\\Blaster\\\\.cursor\\\\debug.log"), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
	// #endregion
}

void UMultiplayerSessionsSubsystem::OnCreateSessionComplete(FName SessionName, bool bwasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}

	MultiplayerOnCreateSessionComplete.Broadcast(bwasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnFindSessionsComplete(bool bwasSuccessful)
{
	if(SessionInterface)
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionCompleteDelegateHandle);
	}

	if(LastSessionSearch->SearchResults.Num() <= 0)
	{
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		// #region agent log
		{
			const int64 Ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();
			const FString Line = FString::Printf(
				TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"run-find\",\"hypothesisId\":\"H5\",\"location\":\"MultiplayerSessionsSubsystem.cpp:OnFindSessionsComplete\",\"message\":\"FindSessions complete empty\",\"data\":{\"success\":%s,\"lanQuery\":%s,\"results\":0,\"dump\":[]},\"timestamp\":%lld}\n"),
				bwasSuccessful ? TEXT("true") : TEXT("false"),
				LastSessionSearch->bIsLanQuery ? TEXT("true") : TEXT("false"),
				Ms);
			FFileHelper::SaveStringToFile(Line, TEXT("s:\\\\Project\\\\Unreal5\\\\Blaster\\\\.cursor\\\\debug.log"), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
		}
		// #endregion
		return;
	}
	MultiplayerOnFindSessionsComplete.Broadcast(LastSessionSearch->SearchResults, bwasSuccessful);
	// #region agent log
	{
		const int64 Ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();
		const int32 DumpCount = FMath::Min(2, LastSessionSearch->SearchResults.Num());
		FString ResultsJson = TEXT("[");
		for (int32 i = 0; i < DumpCount; ++i)
		{
			const FOnlineSessionSearchResult& R = LastSessionSearch->SearchResults[i];
			const bool bIsLAN = R.Session.SessionSettings.bIsLANMatch;
			int32 VisInt = -1;
			R.Session.SessionSettings.Get(FName("SessionVisibility"), VisInt);
			int32 MaxPub = R.Session.SessionSettings.NumPublicConnections;
			int32 OpenPub = R.Session.NumOpenPublicConnections;
			const FString Title = ExtractSessionTitle(R);
			ResultsJson += FString::Printf(
				TEXT("{\"i\":%d,\"id\":\"%s\",\"isLan\":%s,\"vis\":%d,\"max\":%d,\"open\":%d,\"title\":\"%s\"}%s"),
				i,
				*R.GetSessionIdStr(),
				bIsLAN ? TEXT("true") : TEXT("false"),
				VisInt,
				MaxPub,
				OpenPub,
				*Title,
				(i + 1 < DumpCount) ? TEXT(",") : TEXT(""));
		}
		ResultsJson += TEXT("]");
		const FString Line = FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"run-find\",\"hypothesisId\":\"H5\",\"location\":\"MultiplayerSessionsSubsystem.cpp:OnFindSessionsComplete\",\"message\":\"FindSessions complete with results\",\"data\":{\"success\":%s,\"lanQuery\":%s,\"results\":%d,\"dump\":%s},\"timestamp\":%lld}\n"),
			bwasSuccessful ? TEXT("true") : TEXT("false"),
			LastSessionSearch->bIsLanQuery ? TEXT("true") : TEXT("false"),
			LastSessionSearch->SearchResults.Num(),
			*ResultsJson,
			Ms);
		FFileHelper::SaveStringToFile(Line, TEXT("s:\\\\Project\\\\Unreal5\\\\Blaster\\\\.cursor\\\\debug.log"), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
	// #endregion
}

void UMultiplayerSessionsSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if(SessionInterface)
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}
	
	MultiplayerOnJoinSessionComplete.Broadcast(Result);
	
}

void UMultiplayerSessionsSubsystem::OnStartSessionComplete(FName SessionName, bool bwasSuccessful)
{
}

void UMultiplayerSessionsSubsystem::OnDestroySessionComplete(FName SessionName, bool bwasSuccessful)
{
	if(SessionInterface)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}
	if(bwasSuccessful && bCreateSessionOnDestroy)
	{
		bCreateSessionOnDestroy = false;
		CreateSession(LastNumPublicConnections, LastMatchType, DesiredSessionTitle, LastSessionVisibility, LastSelectedMap, LastGameMode);
	}
	MultiplayerOnDestroySessionComplete.Broadcast(bwasSuccessful);
	// #region agent log
	{
		const int64 Ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();
		const FString Line = FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"run-find\",\"hypothesisId\":\"H2\",\"location\":\"MultiplayerSessionsSubsystem.cpp:OnDestroySessionComplete\",\"message\":\"Destroy completed\",\"data\":{\"success\":%s,\"recreate\":%s,\"lastVisibility\":%d},\"timestamp\":%lld}\n"),
			bwasSuccessful ? TEXT("true") : TEXT("false"),
			bCreateSessionOnDestroy ? TEXT("true") : TEXT("false"),
			static_cast<int32>(LastSessionVisibility),
			Ms);
		FFileHelper::SaveStringToFile(Line, TEXT("s:\\\\Project\\\\Unreal5\\\\Blaster\\\\.cursor\\\\debug.log"), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
	// #endregion
}

void UMultiplayerSessionsSubsystem::UpdateSessionVisibility(ESessionVisibility NewVisibility)
{
	LastNumPublicConnections = DesiredNumPublicConnections;
	LastMatchType = DesiredMatchType;
	LastSessionVisibility = NewVisibility;
	DesiredSessionVisibility = NewVisibility; // keep desired in sync for future updates
	LastSelectedMap = DesiredSelectedMap;
	LastGameMode = DesiredGameMode;
	bCreateSessionOnDestroy = true;
	DestroySession();
	// #region agent log
	{
		const int64 Ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();
		const FString Line = FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"run-find\",\"hypothesisId\":\"H3\",\"location\":\"MultiplayerSessionsSubsystem.cpp:UpdateSessionVisibility\",\"message\":\"Visibility update requested\",\"data\":{\"newVisibility\":%d},\"timestamp\":%lld}\n"),
			static_cast<int32>(NewVisibility), Ms);
		FFileHelper::SaveStringToFile(Line, TEXT("s:\\\\Project\\\\Unreal5\\\\Blaster\\\\.cursor\\\\debug.log"), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
	// #endregion
}

void UMultiplayerSessionsSubsystem::UpdateSessionSettings(int32 NumPublicConnections, const FString& MatchType, const FString& SessionTitle, ESessionVisibility Visibility, const FString& SelectedMap, const FString& GameMode)
{
	LastNumPublicConnections = NumPublicConnections;
	LastMatchType = MatchType;
	DesiredSessionTitle = SessionTitle;
	LastSessionVisibility = Visibility;
	DesiredSessionVisibility = Visibility; // keep desired in sync for UI queries
	LastSelectedMap = SelectedMap;
	LastGameMode = GameMode;
	bCreateSessionOnDestroy = true;
	DestroySession();
	// #region agent log
	{
		const int64 Ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();
		const FString Line = FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"run-find\",\"hypothesisId\":\"H4\",\"location\":\"MultiplayerSessionsSubsystem.cpp:UpdateSessionSettings\",\"message\":\"Session settings update requested\",\"data\":{\"num\":%d,\"matchType\":\"%s\",\"title\":\"%s\",\"visibility\":%d,\"map\":\"%s\",\"mode\":\"%s\"},\"timestamp\":%lld}\n"),
			NumPublicConnections, *MatchType, *SessionTitle, static_cast<int32>(Visibility), *SelectedMap, *GameMode, Ms);
		FFileHelper::SaveStringToFile(Line, TEXT("s:\\\\Project\\\\Unreal5\\\\Blaster\\\\.cursor\\\\debug.log"), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
	// #endregion
}

bool UMultiplayerSessionsSubsystem::IsValidSessionInterface()
{
	if (!SessionInterface)
	{
		IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
		if (Subsystem)
		{
			SessionInterface = Subsystem->GetSessionInterface();
		}
	}
	return SessionInterface.IsValid();
}

FString UMultiplayerSessionsSubsystem::ExtractSessionTitle(const FOnlineSessionSearchResult& SessionResult)
{
	FString SessionTitle;
	SessionResult.Session.SessionSettings.Get(FName("SessionTitle"), SessionTitle);
	return SessionTitle;
}
