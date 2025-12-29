// Fill out your copyright notice in the Description page of Project Settings.

#include "LobbyGameMode.h"
#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Blaster/GameState/LobbyGameState.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerState.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "TimerManager.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"

ALobbyGameMode::ALobbyGameMode()
	: DefaultLobbyVisibility(ESessionVisibility::Private)
{
	// DefaultPawnClass는 Blueprint에서 설정하거나 생성자에서 설정 가능
	// Blueprint에서 LobbyPawnClass를 설정하면 그것을 사용
}

void ALobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	// LobbyPawnClass가 설정되어 있으면 DefaultPawnClass로 설정
	// 설정되지 않았으면 BlasterCharacter를 기본값으로 사용 (로비에서도 이동/입력 허용)
	if (LobbyPawnClass)
	{
		DefaultPawnClass = LobbyPawnClass;
	}
	else
	{
		DefaultPawnClass = ABlasterCharacter::StaticClass();
	}

	if (HasAuthority() && bAutoCreateSessionOnBeginPlay)
	{
		AutoCreateLobbySession();
	}

	// 초기 준비 인원 수 동기화
	UpdateReadyCountsAndMaybeStart();
}

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!HasAuthority())
	{
		return;
	}

	int32 NumberOfPlayers = 0;
	if (GameState)
	{
		NumberOfPlayers = GameState->PlayerArray.Num();
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		if (UMultiplayerSessionsSubsystem* Subsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>())
		{
			UE_LOG(LogTemp, Log, TEXT("Player joined lobby. Players: %d / %d"),
				NumberOfPlayers,
				Subsystem->DesiredNumPublicConnections);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("MultiplayerSessionsSubsystem not found in GameInstance."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GameInstance not found in LobbyGameMode."));
	}

	// 플레이어 참가 알림 (모든 클라이언트에 브로드캐스트)
	if (ALobbyGameState* LobbyGS = GetGameState<ALobbyGameState>())
	{
		FString PlayerName = TEXT("Player");
		if (APlayerState* PS = NewPlayer->GetPlayerState<APlayerState>())
		{
			PlayerName = PS->GetPlayerName();
			if (PlayerName.IsEmpty())
			{
				PlayerName = TEXT("Player");
			}
		}
		LobbyGS->MulticastPlayerJoined(PlayerName);
	}

	// 플레이어가 로그인한 후 OverHeadWidget 업데이트
	FTimerHandle TempHandle;
	GetWorld()->GetTimerManager().SetTimer(
		TempHandle,
		this,
		&ALobbyGameMode::UpdateAllPlayerOverheadWidgets,
		0.2f,
		false
	);

	// 뷰타겟 강제 설정 (카메라가 Pawn에 붙지 않는 경우를 방지)
	{
		TWeakObjectPtr<APlayerController> WeakPC = NewPlayer;
		FTimerHandle ViewHandle;
		GetWorld()->GetTimerManager().SetTimer(
			ViewHandle,
			[this, WeakPC]()
			{
				if (APlayerController* PC = WeakPC.Get())
				{
					APawn* Pawn = PC->GetPawn();
					AActor* ViewTarget = Pawn ? static_cast<AActor*>(Pawn) : static_cast<AActor*>(PC);
					PC->SetViewTarget(ViewTarget);
				}
			},
			0.1f,
			false);
	}

	UpdateReadyCountsAndMaybeStart();
}

void ALobbyGameMode::AutoCreateLobbySession()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("AutoCreateLobbySession: GameInstance missing"));
		return;
	}

	if (UMultiplayerSessionsSubsystem* Subsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>())
	{

		// 현재 원하는 가시성을 사용 (기본값 대신)
		const ESessionVisibility EffectiveVisibility = Subsystem->DesiredSessionVisibility != ESessionVisibility::Private
			? Subsystem->DesiredSessionVisibility
			: DefaultLobbyVisibility;

		FString Title;
		if (const ULocalPlayer* LP = GetWorld() ? GetWorld()->GetFirstLocalPlayerFromController() : nullptr)
		{
			Title = FString::Printf(TEXT("%s의 로비"), *LP->GetNickname());
		}
		if (Title.IsEmpty())
		{
			Title = TEXT("호스트의 로비");
		}

		Subsystem->CreateSession(
			DefaultLobbyMaxPlayers,
			DefaultMatchType,
			Title,
			EffectiveVisibility,
			DefaultLobbyMap,
			DefaultMatchType);

		UE_LOG(LogTemp, Log, TEXT("Auto lobby session created: %s (Visibility %d)"),
			*Title, static_cast<int32>(DefaultLobbyVisibility));

		// GameState에 Room 세팅 업데이트
		if (ALobbyGameState* LobbyGS = GetGameState<ALobbyGameState>())
		{
			LobbyGS->UpdateRoomSettings(
				DefaultLobbyMaxPlayers,
				DefaultMatchType,
				Title,
				EffectiveVisibility,
				DefaultLobbyMap,
				DefaultMatchType
			);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AutoCreateLobbySession: Subsystem missing"));
	}
}

void ALobbyGameMode::SetPlayerReadyStatus(APlayerController* PlayerController, bool bReady)
{
	if (!HasAuthority() || !PlayerController)
	{
		return;
	}

	APlayerState* PS = PlayerController->PlayerState;
	if (!PS)
	{
		return;
	}

	const FString PlayerId = PS->GetUniqueId().IsValid() ? PS->GetUniqueId()->ToString() : PS->GetPlayerName();

	const bool bWasReady = ReadyPlayers.Contains(PlayerId);

	if (bReady)
	{
		ReadyPlayers.Add(PlayerId);
	}
	else
	{
		ReadyPlayers.Remove(PlayerId);
	}

	// PlayerState의 준비 상태도 업데이트
	if (ABlasterPlayerState* BlasterPS = Cast<ABlasterPlayerState>(PS))
	{
		BlasterPS->SetReadyStatus(bReady);
	}

	UE_LOG(LogTemp, Log, TEXT("Ready status: %s -> %s"), *PlayerId, bReady ? TEXT("Ready") : TEXT("Not Ready"));

	// 모든 클라이언트에 준비 상태 변경 알림
	if (ALobbyGameState* LobbyGS = GetGameState<ALobbyGameState>())
	{
		LobbyGS->MulticastPlayerReadyStatusChanged(PlayerId, bReady);
	}

	// 모든 플레이어의 OverHeadWidget 업데이트
	UpdateAllPlayerOverheadWidgets();

	// 인원 수 집계 및 자동 시작 체크
	if (bWasReady != bReady)
	{
		UpdateReadyCountsAndMaybeStart();
	}
}

bool ALobbyGameMode::CanStartGame() const
{
	if (!GameState)
	{
		return false;
	}

	const int32 TotalPlayers = GameState->PlayerArray.Num();
	if (TotalPlayers == 0)
	{
		return false;
	}

	return ReadyPlayers.Num() >= TotalPlayers;
}

void ALobbyGameMode::StartGameManually()
{
	if (!HasAuthority())
	{
		return;
	}

	if (bGameStarting)
	{
		UE_LOG(LogTemp, Warning, TEXT("Game start already in progress."));
		return;
	}

	if (!CanStartGame())
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot start: not all players are ready."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("World not found. Cannot start game."));
		return;
	}

	FString TargetMap = EntryMapPath;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UMultiplayerSessionsSubsystem* Subsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>())
		{
			if (!Subsystem->DesiredSelectedMap.IsEmpty())
			{
				TargetMap = Subsystem->DesiredSelectedMap;
			}
		}
	}

	if (TargetMap.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Target map is empty. Set EntryMapPath or SelectedMap in session settings."));
		return;
	}

	const FString TravelPath = FString::Printf(TEXT("%s?listen"), *TargetMap);

	// Optional: ensure subsystem exists for diagnostics
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (!GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>())
		{
			UE_LOG(LogTemp, Warning, TEXT("Starting game without MultiplayerSessionsSubsystem."));
				}
			}

	bGameStarting = true;
	bUseSeamlessTravel = true;

	UE_LOG(LogTemp, Log, TEXT("Starting game manually. Traveling to %s"), *TravelPath);
	World->ServerTravel(TravelPath);
}

bool ALobbyGameMode::IsPlayerReady(const FString& PlayerId) const
{
	return ReadyPlayers.Contains(PlayerId);
}

void ALobbyGameMode::UpdateAllPlayerOverheadWidgets()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 모든 플레이어 컨트롤러 순회
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				if (ABlasterCharacter* Character = Cast<ABlasterCharacter>(Pawn))
				{
					Character->UpdateOverheadWidget();
				}
			}
		}
	}
}

UClass* ALobbyGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// LobbyPawnClass가 설정되어 있으면 사용
	if (LobbyPawnClass)
	{
		return LobbyPawnClass;
	}
	
	// 기본값으로 BlasterCharacter 사용
	return ABlasterCharacter::StaticClass();
}

void ALobbyGameMode::UpdateRoomSettings(int32 MaxPlayers, const FString& MatchType, const FString& SessionTitle,
	ESessionVisibility Visibility, const FString& SelectedMap, const FString& GameMode)
{
	if (!HasAuthority())
	{
		return;
	}

	// GameState에 Room 세팅 업데이트
	if (ALobbyGameState* LobbyGS = GetGameState<ALobbyGameState>())
	{
		LobbyGS->UpdateRoomSettings(MaxPlayers, MatchType, SessionTitle, Visibility, SelectedMap, GameMode);
	}

	// MultiplayerSessionsSubsystem에도 업데이트
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		if (UMultiplayerSessionsSubsystem* Subsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>())
		{
			Subsystem->UpdateSessionSettings(MaxPlayers, MatchType, SessionTitle, Visibility, SelectedMap, GameMode);
		}
	}
}

void ALobbyGameMode::Logout(AController* Exiting)
{
	if (HasAuthority() && Exiting)
	{
		// 플레이어 퇴장 알림 (모든 클라이언트에 브로드캐스트)
		if (ALobbyGameState* LobbyGS = GetGameState<ALobbyGameState>())
		{
			FString PlayerName = TEXT("Player");
			if (APlayerState* PS = Exiting->GetPlayerState<APlayerState>())
			{
				PlayerName = PS->GetPlayerName();
				if (PlayerName.IsEmpty())
				{
					PlayerName = TEXT("Player");
				}

				// ReadyPlayers에서 제거
				const FString PlayerId = PS->GetUniqueId().IsValid() ? PS->GetUniqueId()->ToString() : PlayerName;
				ReadyPlayers.Remove(PlayerId);
			}
			LobbyGS->MulticastPlayerLeft(PlayerName);
		}
	}

	Super::Logout(Exiting);
	UpdateReadyCountsAndMaybeStart();
}

void ALobbyGameMode::UpdateReadyCountsAndMaybeStart()
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 TotalPlayers = GameState ? GameState->PlayerArray.Num() : 0;
	const int32 ReadyCount = ReadyPlayers.Num();

	if (ALobbyGameState* LobbyGS = GetGameState<ALobbyGameState>())
	{
		LobbyGS->SetReadyCounts(ReadyCount, TotalPlayers);
	}

	// 모든 인원이 준비되면 자동 시작
	if (!bGameStarting && CanStartGame())
	{
		StartGameManually();
	}
}