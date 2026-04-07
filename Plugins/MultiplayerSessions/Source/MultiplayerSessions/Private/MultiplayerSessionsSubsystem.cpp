// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystemNames.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Base64.h"
#include "HAL/FileManager.h"

namespace { static FString GetSubsystemDebugLogPath() { return FPaths::ProjectSavedDir() + TEXT("Logs/BlasterDebug.log"); } }

UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem():
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete)),
	FindSessionCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete)),
	StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete)),
	DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete))
{
	LoadConfig();
}

IOnlineSubsystem* UMultiplayerSessionsSubsystem::GetOnlineSubsystem() const
{
	const FName SubsystemName = GetSubsystemName();
	return SubsystemName.IsNone() ? IOnlineSubsystem::Get() : IOnlineSubsystem::Get(SubsystemName);
}

FName UMultiplayerSessionsSubsystem::GetSubsystemName() const
{
#if WITH_EDITOR
	if (bUseNullSubsystemInEditor)
	{
		return FName(TEXT("NULL"));
	}
#endif
	return NAME_None;
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
    if (IOnlineSubsystem* OSS = GetOnlineSubsystem())
    {
        bIsNull = OSS->GetSubsystemName() == NULL_SUBSYSTEM;
    }
	LastSessionSettings->bIsLANMatch = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
	LastSessionSettings->NumPublicConnections = NumPublicConnections;
	LastSessionSettings->bAllowJoinInProgress = true;

	// Visibility별 광고/참여 설정
	const bool bIsPublic = Visibility == ESessionVisibility::Public;
	const bool bFriends = Visibility == ESessionVisibility::Friends;

    // LAN(Null)과 온라인을 분기하되, 광고/조인/Presence는 공통으로 활성화해 검색 가능성을 높임
    LastSessionSettings->bAllowJoinViaPresence = true;
    LastSessionSettings->bShouldAdvertise = true;        
    LastSessionSettings->bUsesPresence = true;                    // Presence on (LAN/온라인)
    LastSessionSettings->bUseLobbiesIfAvailable = true;
	LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	if (!DesiredSessionTitle.IsEmpty())
	{
		// Steam OSS에서 한글 등 멀티바이트 문자가 깨지는 문제 회피: Base64로 저장
		const FString SessionTitleB64 = FBase64::Encode(DesiredSessionTitle, EBase64Mode::Standard);
		LastSessionSettings->Set(FName("SessionTitleB64"), SessionTitleB64, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		// 하위 호환: ASCII 전용일 때 사용할 수 있도록 원본도 저장 (한글은 B64에서 복원)
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
	// bForceCrossBuildCompatible: Development/Shipping 빌드 간 세션 검색 호환 (BuildUniqueId를 0으로 고정)
	LastSessionSettings->BuildUniqueId = bForceCrossBuildCompatible ? 0 : GetBuildUniqueId();

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
		FFileHelper::SaveStringToFile(Line, *GetSubsystemDebugLogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
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
		const FString SubsystemName = GetOnlineSubsystem() ? GetOnlineSubsystem()->GetSubsystemName().ToString() : TEXT("null");
		const bool bHasLP = GetWorld() && GetWorld()->GetFirstLocalPlayerFromController() && GetWorld()->GetFirstLocalPlayerFromController()->GetPreferredUniqueNetId().IsValid();
		const FString Line = FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"run-find\",\"hypothesisId\":\"H5\",\"location\":\"MultiplayerSessionsSubsystem.cpp:FindSessions\",\"message\":\"FindSessions start\",\"data\":{\"max\":%d,\"subsystem\":\"%s\",\"hasLP\":%s},\"timestamp\":%lld}\n"),
			MaxSearchResults, *SubsystemName, bHasLP ? TEXT("true") : TEXT("false"), Ms);
		FFileHelper::SaveStringToFile(Line, *GetSubsystemDebugLogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
	// #endregion

	FindSessionCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionCompleteDelegate);

    LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
    LastSessionSearch->MaxSearchResults = MaxSearchResults;	
    const bool bIsNull = GetOnlineSubsystem() && GetOnlineSubsystem()->GetSubsystemName() == NULL_SUBSYSTEM;
    LastSessionSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
	// bForceCrossBuildCompatible: CreateSession의 BuildUniqueId=0과 쌍으로, 검색 시 0을 요구
	// (Steam OSS는 검색 필터에 로컬 BuildUniqueId를 쓰므로, Create도 0이어야 매칭됨)
	// Presence 검색 (온라인) - SEARCH_PRESENCE deprecated(UE5.5+), SEARCH_LOBBIES 우선 사용
#if defined(SEARCH_LOBBIES)
	LastSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
#else
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996)
#endif
	LastSessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#endif
    if (!bIsNull)
    {
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996)
#endif
       
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
			FFileHelper::SaveStringToFile(Line, *GetSubsystemDebugLogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
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
		FFileHelper::SaveStringToFile(Line, *GetSubsystemDebugLogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
	// #endregion
}
void UMultiplayerSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
	// #region agent log - Blaster/Saved/Logs/Blaster.log
	UE_LOG(LogTemp, Warning, TEXT("[Join] JoinSession subsystem: sessionId=\"%s\""), *SessionResult.GetSessionIdStr());
	// #endregion
	if(!SessionInterface.IsValid()) {
		UE_LOG(LogTemp, Warning, TEXT("[Join] JoinSession: SessionInterface invalid, broadcast UnknownError"));
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	const bool bJoinStarted = SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionResult);
	// #region agent log - Blaster/Saved/Logs/Blaster.log
	UE_LOG(LogTemp, Warning, TEXT("[Join] JoinSession API started: %s"), bJoinStarted ? TEXT("true") : TEXT("false"));
	// #endregion
	if (!bJoinStarted)
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		UE_LOG(LogTemp, Warning, TEXT("[Join] JoinSession API failed immediately, broadcast UnknownError"));
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
		FFileHelper::SaveStringToFile(Line, *GetSubsystemDebugLogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
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
			FFileHelper::SaveStringToFile(Line, *GetSubsystemDebugLogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
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
		FFileHelper::SaveStringToFile(Line, *GetSubsystemDebugLogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
	// #endregion
}

void UMultiplayerSessionsSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	// #region agent log - Blaster/Saved/Logs/Blaster.log
	UE_LOG(LogTemp, Warning, TEXT("[Join] OnJoinSessionComplete: Result=%d (%s)"), static_cast<int32>(Result),
		Result == EOnJoinSessionCompleteResult::Success ? TEXT("Success") :
		Result == EOnJoinSessionCompleteResult::SessionIsFull ? TEXT("SessionIsFull") :
		Result == EOnJoinSessionCompleteResult::SessionDoesNotExist ? TEXT("SessionDoesNotExist") :
		Result == EOnJoinSessionCompleteResult::CouldNotRetrieveAddress ? TEXT("CouldNotRetrieveAddress") :
		Result == EOnJoinSessionCompleteResult::AlreadyInSession ? TEXT("AlreadyInSession") : TEXT("UnknownError"));
	// #endregion
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
		FFileHelper::SaveStringToFile(Line, *GetSubsystemDebugLogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
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
		FFileHelper::SaveStringToFile(Line, *GetSubsystemDebugLogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
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
		FFileHelper::SaveStringToFile(Line, *GetSubsystemDebugLogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
	// #endregion
}

void UMultiplayerSessionsSubsystem::UpdateSessionHostAddress(const FString& HostAddressPort)
{
	if (!IsValidSessionInterface() || HostAddressPort.IsEmpty())
	{
		return;
	}
	FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
	if (!Session)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Host] UpdateSessionHostAddress: no active session"));
		return;
	}
	FOnlineSessionSettings& Settings = Session->SessionSettings;
	Settings.Set(FName(TEXT("HostAddress")), HostAddressPort, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	const bool bOk = SessionInterface->UpdateSession(NAME_GameSession, Settings);
	UE_LOG(LogTemp, Log, TEXT("[Host] UpdateSessionHostAddress: %s addr=\"%s\""), bOk ? TEXT("ok") : TEXT("fail"), *HostAddressPort);
}

bool UMultiplayerSessionsSubsystem::IsValidSessionInterface()
{
	if (!SessionInterface)
	{
		if (IOnlineSubsystem* Subsystem = GetOnlineSubsystem())
		{
			SessionInterface = Subsystem->GetSessionInterface();
		}
	}
	return SessionInterface.IsValid();
}

bool UMultiplayerSessionsSubsystem::HasActiveSession() const
{
	IOnlineSessionPtr Interface = SessionInterface;
	if (!Interface.IsValid() && GetOnlineSubsystem())
	{
		Interface = GetOnlineSubsystem()->GetSessionInterface();
	}
	return Interface.IsValid() && Interface->GetNamedSession(NAME_GameSession) != nullptr;
}

FString UMultiplayerSessionsSubsystem::ExtractSessionTitle(const FOnlineSessionSearchResult& SessionResult)
{
	return ExtractSessionTitleFromSettings(SessionResult.Session.SessionSettings);
}

FString UMultiplayerSessionsSubsystem::ExtractSessionTitleFromSettings(const FOnlineSessionSettings& SessionSettings)
{
	// Base64 인코딩된 제목 우선 사용 (한글 깨짐 방지)
	FString SessionTitleB64;
	if (SessionSettings.Get(FName("SessionTitleB64"), SessionTitleB64) && !SessionTitleB64.IsEmpty())
	{
		FString Decoded;
		if (FBase64::Decode(SessionTitleB64, Decoded, EBase64Mode::Standard))
		{
			return Decoded;
		}
	}
	// 하위 호환: SessionTitle 폴백
	FString SessionTitle;
	SessionSettings.Get(FName("SessionTitle"), SessionTitle);
	return SessionTitle;
}
