// Fill out your copyright notice in the Description page of Project Settings.


#include "Menu.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#if WITH_EDITOR
#include "UnrealEdMisc.h"
#endif
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/Class.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
// Debug logging: Saved/Logs/BlasterDebug.log (개발·패키징 빌드 공통)
static FString GetDebugLogPath() { return FPaths::ProjectSavedDir() + TEXT("Logs/BlasterDebug.log"); }
// #region agent log
static void MenuWriteDebugLog(const FString& Location, const FString& Message, const FString& DataJson, const FString& HypothesisId, const FString& RunId)
{
#if !UE_BUILD_SHIPPING
	const int64 Ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();
	const FString Line = FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"%s\",\"hypothesisId\":\"%s\",\"location\":\"%s\",\"message\":\"%s\",\"data\":%s,\"timestamp\":%lld}\n"),
		*RunId, *HypothesisId, *Location, *Message, *DataJson, Ms);
	FFileHelper::SaveStringToFile(Line, *GetDebugLogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
#endif
}
// #endregion

UMultiplayerSessionsSubsystem* UMenu::EnsureMultiplayerSubsystem()
{
	if (!MultiplayerSessionsSubsystem)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
		}
	}
	return MultiplayerSessionsSubsystem;
}

void UMenu::MenuSetup(int32 NumberOfPublicConnections, FString TypeOfMatch, FString LobbyPath)
{
	// 참조: PathToLobby에 ?listen 보장 (ServerTravel에 필요)
	PathToLobby = LobbyPath.Contains(TEXT("?listen")) ? LobbyPath : FString::Printf(TEXT("%s?listen"), *LobbyPath.TrimEnd());
	NumPublicConnections = NumberOfPublicConnections;
	MatchType = TypeOfMatch;
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);

	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			FInputModeUIOnly InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget()); // Lock the mouse to the viewport and hide it
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}

	EnsureMultiplayerSubsystem();
	if(MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->MultiplayerOnCreateSessionComplete.AddDynamic(this, &ThisClass::OnCreateSession);
		MultiplayerSessionsSubsystem->MultiplayerOnFindSessionsComplete.AddUObject(this, &ThisClass::OnFindSessions);
		MultiplayerSessionsSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &ThisClass::OnJoinSession);
		MultiplayerSessionsSubsystem->MultiplayerOnStartSessionComplete.AddDynamic(this, &ThisClass::OnStartSession);
		MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &ThisClass::OnDestroySession);
	}
}
bool UMenu::Initialize()
{
	if(!Super::Initialize())
	{
		return false;
	}
	// 위젯 바인딩 제거 - 블루프린트에서 직접 함수 호출
	return true;
}

void UMenu::OnCreateSession(bool bWasSuccessful)
{
	MenuWriteDebugLog(
		TEXT("Menu.cpp:OnCreateSession"),
		TEXT("OnCreateSession entry"),
		FString::Printf(TEXT("{\"success\":%s}"), bWasSuccessful ? TEXT("true") : TEXT("false")),
		TEXT("H1"),
		TEXT("run-pre3"));

	if (bHasHandledCreateSessionSuccess && !bWasSuccessful)
	{
		// 성공 후 추가로 들어오는 실패 콜백 무시 (OpenLevel 이후 세션 재구성 시 발생 가능)
		return;
	}

		if (bWasSuccessful)
	{
		bHasHandledCreateSessionSuccess = true;
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Green,
				FString(TEXT("Session Created Successfully!"))
			);
		}
#endif
		UWorld* World = GetWorld();
		if (World)
		{
#if WITH_EDITOR
			// 패키징/쿡 중 ServerTravel 호출 시 "Server travel should never create a pending net game" 에러 방지
			if (IsRunningCommandlet() || IsRunningCookCommandlet())
			{
				return;
			}
#endif
			FString TargetPath = PathToLobby;
			if (TargetPath.IsEmpty()) TargetPath = TEXT("/Game/Maps/Lobby?listen");
			FString CleanPath = TargetPath;
			CleanPath.RemoveFromEnd(TEXT("?listen"));
			CleanPath.TrimStartAndEndInline();
			if (CleanPath.IsEmpty()) CleanPath = TEXT("/Game/Maps/Lobby");
			const FString TravelURL = FString::Printf(TEXT("%s?listen"), *CleanPath);
			World->ServerTravel(TravelURL);
		}
	}
	else
	{
		MenuWriteDebugLog(
			TEXT("Menu.cpp:OnCreateSession"),
			TEXT("CreateSession failed"),
			TEXT("{}"),
			TEXT("H1"),
			TEXT("run-pre3"));
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Red,
				FString(TEXT("Failed to Create Session!"))
			);
		}
#endif
	}

	// 버튼 상태 관리는 블루프린트에서 처리
}
void UMenu::OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
	if (MultiplayerSessionsSubsystem == nullptr)
	{
		return;
	}

	LastSessionSearchResults = SessionResults;
	LastSessionInfos.Empty();

	for (auto& Result : SessionResults)
	{
		FString SettingsValue;
		Result.Session.SessionSettings.Get(FName(TEXT("MatchType")), SettingsValue);

		FString SessionTitle = UMultiplayerSessionsSubsystem::ExtractSessionTitle(Result);
		int32 VisibilityInt = static_cast<int32>(ESessionVisibility::Private);
		Result.Session.SessionSettings.Get(FName("SessionVisibility"), VisibilityInt);
		FString SelectedMap;
		Result.Session.SessionSettings.Get(FName("SelectedMap"), SelectedMap);
		FString SessionGameMode;
		Result.Session.SessionSettings.Get(FName("GameMode"), SessionGameMode);

		FSessionInfo Info;
		Info.MatchType = SettingsValue;
		Info.SessionTitle = SessionTitle.IsEmpty() ? TEXT("세션") : SessionTitle;
		Info.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
		Info.CurrentPlayers = Info.MaxPlayers - Result.Session.NumOpenPublicConnections;
		Info.PingInMs = Result.PingInMs;
		Info.SearchResultIndex = LastSessionInfos.Num();
		Info.SessionId = Result.GetSessionIdStr();
		Info.Visibility = static_cast<ESessionVisibility>(VisibilityInt);
		Info.RoomName = Info.SessionTitle;
		Info.SelectedMap = SelectedMap;
		Info.GameMode = SessionGameMode;

		LastSessionInfos.Add(Info);
	}

	if (LastSessionInfos.Num() == 0 || !bWasSuccessful)
	{
		// 버튼 상태 관리는 블루프린트에서 처리
		OnSessionListCleared();
		// #region agent log
		{
			const int64 Ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();
			const FString Line = FString::Printf(
				TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"run-find\",\"hypothesisId\":\"H1\",\"location\":\"Menu.cpp:OnFindSessions\",\"message\":\"FindSessions empty or fail\",\"data\":{\"count\":%d,\"success\":%s},\"timestamp\":%lld}\n"),
				SessionResults.Num(), bWasSuccessful ? TEXT("true") : TEXT("false"), Ms);
			FFileHelper::SaveStringToFile(Line, *GetDebugLogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
		}
		// #endregion
		return;
	}

	// #region agent log
	{
		const int64 Ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();
		int32 FirstVisibility = LastSessionInfos.Num() > 0 ? static_cast<int32>(LastSessionInfos[0].Visibility) : -1;
		const FString Line = FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"run-find\",\"hypothesisId\":\"H1\",\"location\":\"Menu.cpp:OnFindSessions\",\"message\":\"FindSessions results\",\"data\":{\"count\":%d,\"firstVisibility\":%d},\"timestamp\":%lld}\n"),
			LastSessionInfos.Num(), FirstVisibility, Ms);
		FFileHelper::SaveStringToFile(Line, *GetDebugLogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
	// #endregion
	OnSessionListUpdated(LastSessionInfos);
}

void UMenu::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	// #region agent log - Blaster/Saved/Logs/Blaster.log
	UE_LOG(LogTemp, Warning, TEXT("[Join] OnJoinSession entry: Result=%d (%s)"),
		static_cast<int32>(Result),
		Result == EOnJoinSessionCompleteResult::Success ? TEXT("Success") :
		Result == EOnJoinSessionCompleteResult::SessionIsFull ? TEXT("SessionIsFull") :
		Result == EOnJoinSessionCompleteResult::SessionDoesNotExist ? TEXT("SessionDoesNotExist") :
		Result == EOnJoinSessionCompleteResult::CouldNotRetrieveAddress ? TEXT("CouldNotRetrieveAddress") :
		Result == EOnJoinSessionCompleteResult::AlreadyInSession ? TEXT("AlreadyInSession") : TEXT("UnknownError"));
	// #endregion

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		FString ErrorMsg;
		switch (Result)
		{
		case EOnJoinSessionCompleteResult::SessionIsFull: ErrorMsg = TEXT("방이 가득 찼습니다."); break;
		case EOnJoinSessionCompleteResult::SessionDoesNotExist: ErrorMsg = TEXT("세션이 더 이상 존재하지 않습니다."); break;
		case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress: ErrorMsg = TEXT("연결 주소를 가져올 수 없습니다. (Steam/방화벽 확인)"); break;
		case EOnJoinSessionCompleteResult::AlreadyInSession: ErrorMsg = TEXT("이미 세션에 있습니다."); break;
		case EOnJoinSessionCompleteResult::UnknownError:
		default: ErrorMsg = TEXT("참가 실패. (빌드 버전/Steam 설정 확인)"); break;
		}
		UE_LOG(LogTemp, Warning, TEXT("[Join] JoinSession FAILED: %s (Result=%d)"), *ErrorMsg, static_cast<int32>(Result));
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, ErrorMsg);
		}
#endif
		return;
	}

	// Steam 세션 기반: GetResolvedConnectString(steam.xxx) 우선 - NetDriver가 SteamNetDriver여야 함
	// IP 우선은 임시방편(LAN만 가능) - Steam NAT/P2P 이점 손실
	FString Address;
	IOnlineSubsystem* Subsystem = MultiplayerSessionsSubsystem ? MultiplayerSessionsSubsystem->GetOnlineSubsystem() : IOnlineSubsystem::Get();
	if (Subsystem)
	{
		IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->GetResolvedConnectString(NAME_GameSession, Address);
		}
	}
	// GetResolvedConnectString 빈 경우에만 LAN/NULL 폴백
	if (Address.IsEmpty() && !CachedHostAddressForJoin.IsEmpty())
	{
		Address = CachedHostAddressForJoin;
		if (!Address.Contains(TEXT(":"))) Address += TEXT(":7777");
		CachedHostAddressForJoin.Empty();
	}

	if (Address.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Join] GetResolvedConnectString FAILED - no address"));
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, TEXT("연결 주소를 가져올 수 없습니다."));
		}
#endif
		return;
	}

	APlayerController* PlayerController = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (PlayerController)
	{
		// #region agent log - NetDriver 검증 (SteamNetDriver vs IpNetDriver)
		UWorld* W = GetWorld();
		UNetDriver* NetDriver = W ? W->GetNetDriver() : nullptr;
		FString NetDriverClass = NetDriver && NetDriver->GetClass() ? NetDriver->GetClass()->GetName() : TEXT("null");
		FString OSSName = Subsystem ? Subsystem->GetSubsystemName().ToString() : TEXT("null");
		{
			FString AddrPrefix = Address.Len() > 40 ? Address.Left(40) + TEXT("...") : Address;
			const FString Line = FString::Printf(
				TEXT("{\"hypothesisId\":\"H_NetDriver\",\"location\":\"Menu.cpp:OnJoinSession\",\"message\":\"ClientTravel\",\"data\":{\"address\":\"%s\",\"netDriver\":\"%s\",\"oss\":\"%s\",\"addressType\":\"%s\"},\"timestamp\":%lld}\n"),
				*AddrPrefix, *NetDriverClass, *OSSName,
				Address.StartsWith(TEXT("steam.")) ? TEXT("steam") : (Address.StartsWith(TEXT("steamid:")) ? TEXT("steamid") : TEXT("ip")),
				FDateTime::UtcNow().ToUnixTimestamp() * 1000);
			FFileHelper::SaveStringToFile(Line, *GetDebugLogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
		}
		// #endregion
		UE_LOG(LogTemp, Warning, TEXT("[Join] ClientTravel: addr=\"%s\" netDriver=%s oss=%s"), *Address, *NetDriverClass, *OSSName);
		PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Join] ClientTravel SKIPPED: PlayerController is null"));
	}
}

void UMenu::OnStartSession(bool bWasSuccessful)
{
}

void UMenu::OnDestroySession(bool bWasSuccessful)
{
}
void UMenu::CreateSession(const FString& SessionTitle)
{
	if (!EnsureMultiplayerSubsystem())
	{
		return;
	}
	FString Title = SessionTitle;
	if (Title.IsEmpty() && GetWorld())
	{
		if (ULocalPlayer* LP = GetWorld()->GetFirstLocalPlayerFromController())
		{
			Title = FString::Printf(TEXT("%s의 로비"), *LP->GetNickname());
		}
	}
	if (Title.IsEmpty()) Title = TEXT("호스트의 로비");
	FString TargetPath = PathToLobby;
	if (TargetPath.IsEmpty()) TargetPath = TEXT("/Game/Maps/Lobby?listen");
	FString CleanPath = TargetPath;
	CleanPath.RemoveFromEnd(TEXT("?listen"));
	CleanPath.TrimStartAndEndInline();
	if (CleanPath.IsEmpty()) CleanPath = TEXT("/Game/Maps/Lobby");
	// 서브시스템 CreateSession → OnCreateSession → ServerTravel 흐름 (LobbyGameMode AutoCreate 불필요)
	MultiplayerSessionsSubsystem->CreateSession(
		NumPublicConnections,
		MatchType,
		Title,
		ESessionVisibility::Public,
		CleanPath,
		TEXT(""));
}

void UMenu::FindSessions()
{
	if (!EnsureMultiplayerSubsystem())
	{
		return;
	}
	// #region agent log
	{
		const int64 Ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();
		const FString Line = FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"run-find\",\"hypothesisId\":\"H5\",\"location\":\"Menu.cpp:FindSessions\",\"message\":\"FindSessions triggered\",\"data\":{},\"timestamp\":%lld}\n"),
			Ms);
		FFileHelper::SaveStringToFile(Line, *GetDebugLogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
	// #endregion
	MultiplayerSessionsSubsystem->FindSessions(10000);
}

void UMenu::RefreshSessionList()
{
	LastSessionSearchResults.Empty();
	LastSessionInfos.Empty();
	CachedHostAddressForJoin.Empty();
	OnSessionListCleared();
	if (EnsureMultiplayerSubsystem())
	{
		MultiplayerSessionsSubsystem->FindSessions(10000);
	}
}

void UMenu::StartGameDirectly(const FString& LobbyMapPath)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

#if WITH_EDITOR
	// 패키징/쿡 중 OpenLevel 호출 시 "Server travel should never create a pending net game" 에러 방지
	if (IsRunningCommandlet() || IsRunningCookCommandlet())
	{
		return;
	}
#endif

	FString TargetPath = LobbyMapPath.IsEmpty() ? PathToLobby : LobbyMapPath;
	
	// ?listen 제거 (맵 경로만 추출)
	FString CleanPath = TargetPath;
	CleanPath.RemoveFromEnd(TEXT("?listen"));
	
	// 패키징 시 안정성을 위해 전체 맵 경로 사용 (short name은 느린 디스크 검색 유발)
	FString LevelName = CleanPath;
	if (LevelName.IsEmpty()) LevelName = TEXT("/Game/Maps/Lobby");
	
	// OpenLevel을 사용하여 레벨 이동 (자동으로 서버가 됨)
	// listen 옵션을 추가하여 리슨 서버로 동작하도록 함 (호스트가 되도록)
	UGameplayStatics::OpenLevel(World, FName(*LevelName), true, TEXT("listen"));
}

void UMenu::JoinSessionByIndex(int32 SessionIndex)
{
	// #region agent log - Blaster/Saved/Logs/Blaster.log
	UE_LOG(LogTemp, Warning, TEXT("[Join] JoinSessionByIndex: index=%d, results=%d"), SessionIndex, LastSessionSearchResults.Num());
	// #endregion
	if (!MultiplayerSessionsSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Join] JoinSessionByIndex: subsystem null"));
		return;
	}

	if (!LastSessionSearchResults.IsValidIndex(SessionIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Join] JoinSessionByIndex: invalid index %d (count=%d)"), SessionIndex, LastSessionSearchResults.Num());
		return;
	}

	// 세션에 저장된 호스트 IP 우선 캐시 (GetAddressInfo 실패 회피)
	CachedHostAddressForJoin.Empty();
	FString HostAddr;
	if (LastSessionSearchResults[SessionIndex].Session.SessionSettings.Get(FName(TEXT("HostAddress")), HostAddr) && !HostAddr.IsEmpty())
	{
		CachedHostAddressForJoin = HostAddr;
		UE_LOG(LogTemp, Warning, TEXT("[Join] Cached HostAddress from session: \"%s\""), *CachedHostAddressForJoin);
	}

	// Steam OSS: JoinSession 시 bUsesPresence/bUseLobbiesIfAvailable 일치 필요
	FOnlineSessionSearchResult ModifiedResult = LastSessionSearchResults[SessionIndex];
	ModifiedResult.Session.SessionSettings.bUsesPresence = true;
	ModifiedResult.Session.SessionSettings.bUseLobbiesIfAvailable = true;
	// #region agent log - Blaster/Saved/Logs/Blaster.log
	UE_LOG(LogTemp, Warning, TEXT("[Join] JoinSession API call: sessionId=\"%s\""), *ModifiedResult.GetSessionIdStr());
	// #endregion
	MultiplayerSessionsSubsystem->JoinSession(ModifiedResult);
}

void UMenu::UpdateSessionVisibility(ESessionVisibility NewVisibility)
{
	if (!EnsureMultiplayerSubsystem())
	{
		return;
	}

	MultiplayerSessionsSubsystem->UpdateSessionVisibility(NewVisibility);

	// #region agent log
	{
		const int64 Ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();
		const FString Line = FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"run-find\",\"hypothesisId\":\"H3\",\"location\":\"Menu.cpp:UpdateSessionVisibility\",\"message\":\"Menu update visibility\",\"data\":{\"visibility\":%d},\"timestamp\":%lld}\n"),
			static_cast<int32>(NewVisibility), Ms);
		FFileHelper::SaveStringToFile(Line, *GetDebugLogPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
	// #endregion
}

void UMenu::UpdateRoomSettings(const FString& RoomName, const FString& GameMode, const FString& MapPath, int32 MaxPlayers)
{
	if (!EnsureMultiplayerSubsystem())
	{
		return;
	}

	NumPublicConnections = MaxPlayers > 0 ? MaxPlayers : NumPublicConnections;
	MultiplayerSessionsSubsystem->UpdateSessionSettings(
		NumPublicConnections,
		MatchType,
		RoomName,
		MultiplayerSessionsSubsystem->DesiredSessionVisibility,
		MapPath,
		GameMode);
}

FString UMenu::GetSessionInviteCode() const
{
	if (LastSessionInfos.Num() > 0)
	{
		return LastSessionInfos[0].SessionId;
	}
	return FString();
}

void UMenu::SetReadyStatus(bool bReady)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	AGameModeBase* GameMode = World->GetAuthGameMode();
	if (!GameMode)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Ready status set locally: %s"), bReady ? TEXT("Ready") : TEXT("Not Ready"));
		return;
	}

	static const FName SetPlayerReadyStatusName(TEXT("SetPlayerReadyStatus"));
	if (UFunction* ReadyFunction = GameMode->FindFunction(SetPlayerReadyStatusName))
	{
		struct FSetPlayerReadyStatusParams
		{
			APlayerController* PlayerController;
			bool bReady;
		};

		FSetPlayerReadyStatusParams Params{ PC, bReady };
		GameMode->ProcessEvent(ReadyFunction, &Params);
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("GameMode %s does not implement SetPlayerReadyStatus"), *GameMode->GetName());
	}
}

FSessionInfo UMenu::GetCurrentSessionInfo() const
{
	// 1) 현재 세션 인터페이스에서 세션 설정을 직접 읽기 (플러그인-게임 모듈 의존 제거)
	IOnlineSubsystem* Subsystem = MultiplayerSessionsSubsystem ? MultiplayerSessionsSubsystem->GetOnlineSubsystem() : IOnlineSubsystem::Get();
	if (Subsystem)
	{
		IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			if (FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession))
			{
				FSessionInfo Info;

				const FString SessionTitle = UMultiplayerSessionsSubsystem::ExtractSessionTitleFromSettings(Session->SessionSettings);
				Info.SessionTitle = SessionTitle.IsEmpty() ? TEXT("로비") : SessionTitle;
				Info.RoomName = Info.SessionTitle;

				Session->SessionSettings.Get(FName("MatchType"), Info.MatchType);

				int32 VisibilityInt = static_cast<int32>(ESessionVisibility::Private);
				Session->SessionSettings.Get(FName("SessionVisibility"), VisibilityInt);
				Info.Visibility = static_cast<ESessionVisibility>(VisibilityInt);

				Session->SessionSettings.Get(FName("SelectedMap"), Info.SelectedMap);
				Session->SessionSettings.Get(FName("GameMode"), Info.GameMode);

				Info.MaxPlayers = Session->SessionSettings.NumPublicConnections;
				Info.CurrentPlayers = Info.MaxPlayers - Session->NumOpenPublicConnections;

				return Info;
			}
		}
	}

	// 2) 세션 정보를 못 얻은 경우, 마지막 검색 결과가 있으면 사용
	if (LastSessionInfos.Num() > 0)
	{
		return LastSessionInfos[0];
	}

	// 3) 최종 기본값
	FSessionInfo Info;
	Info.SessionTitle = FString(TEXT("로비"));
	Info.RoomName = Info.SessionTitle;
	Info.MatchType = MatchType;
	Info.MaxPlayers = NumPublicConnections;
	Info.Visibility = MultiplayerSessionsSubsystem ? MultiplayerSessionsSubsystem->DesiredSessionVisibility : ESessionVisibility::Private;
	Info.SelectedMap = PathToLobby;
	return Info;
}

void UMenu::RefreshLobbyPlayers()
{
	UpdatePlayerListInternal();
}

void UMenu::LeaveLobby()
{
	if (!MultiplayerSessionsSubsystem)
	{
		return;
	}
	MultiplayerSessionsSubsystem->DestroySession();
}

void UMenu::StartGame()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AGameModeBase* GameMode = World->GetAuthGameMode();
	if (!GameMode)
	{
		return;
	}

	static const FName StartGameFunctionName(TEXT("StartGameManually"));
	if (UFunction* StartGameFunction = GameMode->FindFunction(StartGameFunctionName))
	{
		GameMode->ProcessEvent(StartGameFunction, nullptr);
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("GameMode %s does not implement StartGameManually"), *GameMode->GetName());
	}
}

void UMenu::MenuTearDown()
{
	RemoveFromParent();
	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			FInputModeGameOnly InputModeData;
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(false);
		}
	}
}

void UMenu::NativeDestruct()
{
	MenuTearDown(); // Ensure the menu is properly torn down when the widget is destroyed

	Super::NativeDestruct();
}

void UMenu::UpdatePlayerListInternal()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AGameStateBase* GS = World->GetGameState<AGameStateBase>();
	if (!GS)
	{
		OnPlayerListUpdated(TArray<FString>());
		return;
	}

	TArray<FString> PlayerNames;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (PS)
		{
			PlayerNames.Add(PS->GetPlayerName());
		}
	}

	OnPlayerListUpdated(PlayerNames);
}

void UMenu::ShowLobby()
{
	ShowLobbyInternal();
}

void UMenu::ShowLobbyInternal()
{
	OnLobbyShown();
	UpdatePlayerListInternal();
}






