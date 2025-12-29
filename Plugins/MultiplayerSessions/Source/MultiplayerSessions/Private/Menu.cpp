// Fill out your copyright notice in the Description page of Project Settings.


#include "Menu.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/Class.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
// Debug logging (NDJSON) for run-pre3
#define DEBUG_LOG_PATH TEXT("s:\\\\Project\\\\Unreal5\\\\Blaster\\\\.cursor\\\\debug.log")
// #region agent log
static void MenuWriteDebugLog(const FString& Location, const FString& Message, const FString& DataJson, const FString& HypothesisId, const FString& RunId)
{
	const int64 Ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond();
	const FString Line = FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"%s\",\"hypothesisId\":\"%s\",\"location\":\"%s\",\"message\":\"%s\",\"data\":%s,\"timestamp\":%lld}\n"),
		*RunId, *HypothesisId, *Location, *Message, *DataJson, Ms);
	FFileHelper::SaveStringToFile(Line, DEBUG_LOG_PATH, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
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
	PathToLobby = FString::Printf(TEXT("%s?listen"), *LobbyPath);
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
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Green,
				FString(TEXT("Session Created Successfully!"))
			);
		}
		UWorld* World = GetWorld();
		if (World)
		{
			// 클라이언트 넷 모드에서도 자신이 리슨 서버가 되도록 OpenLevel + listen 사용
			FString TargetPath = PathToLobby;

			// ?listen 제거 후 맵 경로/이름 준비
			FString CleanPath = TargetPath;
			CleanPath.RemoveFromEnd(TEXT("?listen"));
			MenuWriteDebugLog(
				TEXT("Menu.cpp:OnCreateSession"),
				TEXT("OpenLevel"),
				FString::Printf(TEXT("{\"map\":\"%s\",\"netMode\":%d,\"hasAuthority\":%s}"),
					*CleanPath,
					static_cast<int32>(World->GetNetMode()),
					World->IsNetMode(NM_Client) ? TEXT("false") : TEXT("true")),
				TEXT("H2"),
				TEXT("run-pre3"));

			const FString TravelURL = FString::Printf(TEXT("%s?listen"), *CleanPath);

			// PIE 클라이언트(NetMode==NM_Client)에서도 강제로 open 명령으로 전환 (기존 연결을 끊고 리슨 서버로 재시작)
			if (World->GetNetMode() == NM_Client)
			{
				if (APlayerController* PC = World->GetFirstPlayerController())
				{
					PC->ConsoleCommand(FString::Printf(TEXT("open %s"), *TravelURL));
				}
				else
				{
					UGameplayStatics::OpenLevel(World, FName(*CleanPath), false, TEXT("listen"));
				}
			}
			else
			{
				UGameplayStatics::OpenLevel(World, FName(*CleanPath), false, TEXT("listen"));
			}
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
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Red,
				FString(TEXT("Failed to Create Session!"))
			);
		}
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
			FFileHelper::SaveStringToFile(Line, DEBUG_LOG_PATH, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
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
		FFileHelper::SaveStringToFile(Line, DEBUG_LOG_PATH, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
	// #endregion
	OnSessionListUpdated(LastSessionInfos);
}

void UMenu::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	if(Result != EOnJoinSessionCompleteResult::Success)
	{
		// 버튼 상태 관리는 블루프린트에서 처리
		// 조인 실패 이벤트를 블루프린트에 알릴 수 있음 (선택사항)
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Red,
				FString(TEXT("Failed to join session."))
			);
		}
		return;
	}

	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			FString Address;
			if (SessionInterface->GetResolvedConnectString(NAME_GameSession, Address))
			{
				APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
				if (PlayerController)
				{
					PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
				}
			}
			else
			{
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(
						-1,
						15.f,
						FColor::Red,
						FString(TEXT("Failed to resolve session address."))
					);
				}
			}
		}
		else
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
					-1,
					15.f,
					FColor::Red,
					FString(TEXT("Session interface is not valid."))
				);
			}
		}
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
	if (!MultiplayerSessionsSubsystem)
	{
		MenuWriteDebugLog(
			TEXT("Menu.cpp:CreateSession"),
			TEXT("No subsystem"),
			TEXT("{}"),
			TEXT("H1"),
			TEXT("run-pre3"));
		return;
	}

	FString Title = SessionTitle;
	if (Title.IsEmpty())
	{
		// 기본 제목: 플레이어 이름 사용
		if (const ULocalPlayer* LP = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr)
		{
			Title = FString::Printf(TEXT("%s의 방"), *LP->GetNickname());
		}
		if (Title.IsEmpty())
		{
			Title = TEXT("새로운 방");
		}
	}

	MenuWriteDebugLog(
		TEXT("Menu.cpp:CreateSession"),
		TEXT("CreateSession called"),
		FString::Printf(TEXT("{\"title\":\"%s\",\"numConnections\":%d,\"matchType\":\"%s\"}"),
			*Title, NumPublicConnections, *MatchType),
		TEXT("H1"),
		TEXT("run-pre3"));
	// 로비/메뉴에서 설정한 가시성(DesiredSessionVisibility)을 반영하여 최초 생성 시부터 올바른 광고 설정을 사용
	MultiplayerSessionsSubsystem->CreateSession(
		NumPublicConnections,
		MatchType,
		Title,
		MultiplayerSessionsSubsystem->DesiredSessionVisibility);
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
		FFileHelper::SaveStringToFile(Line, DEBUG_LOG_PATH, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
	// #endregion
	MultiplayerSessionsSubsystem->FindSessions(10000);
}

void UMenu::RefreshSessionList()
{
	LastSessionSearchResults.Empty();
	LastSessionInfos.Empty();
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

	FString TargetPath = LobbyMapPath.IsEmpty() ? PathToLobby : LobbyMapPath;
	
	// ?listen 제거 (맵 경로만 추출)
	FString CleanPath = TargetPath;
	CleanPath.RemoveFromEnd(TEXT("?listen"));
	
	// 맵 경로에서 맵 이름만 추출 (예: /Game/Maps/Lobby -> Lobby)
	FString MapName = CleanPath;
	if (MapName.Contains(TEXT("/")))
	{
		// 마지막 "/" 이후의 부분을 추출
		int32 LastSlashIndex = -1;
		if (MapName.FindLastChar(TEXT('/'), LastSlashIndex))
		{
			MapName = MapName.Mid(LastSlashIndex + 1);
		}
	}
	
	// OpenLevel을 사용하여 레벨 이동 (자동으로 서버가 됨)
	// listen 옵션을 추가하여 리슨 서버로 동작하도록 함 (호스트가 되도록)
	UGameplayStatics::OpenLevel(World, FName(*MapName), false, TEXT("listen"));
}

void UMenu::JoinSessionByIndex(int32 SessionIndex)
{
	if (!MultiplayerSessionsSubsystem)
	{
		return;
	}

	if (!LastSessionSearchResults.IsValidIndex(SessionIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid session index: %d"), SessionIndex);
		return;
	}

	MultiplayerSessionsSubsystem->JoinSession(LastSessionSearchResults[SessionIndex]);
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
		FFileHelper::SaveStringToFile(Line, DEBUG_LOG_PATH, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
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
	if (IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
	{
		IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			if (FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession))
			{
				FSessionInfo Info;

				FString SessionTitle;
				Session->SessionSettings.Get(FName("SessionTitle"), SessionTitle);
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




