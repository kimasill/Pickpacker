// Fill out your copyright notice in the Description page of Project Settings.

#include "LobbyGameState.h"
#include "Net/UnrealNetwork.h"
#include "Blaster/GameMode/LobbyGameMode.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ALobbyGameState::ALobbyGameState()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ALobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALobbyGameState, RoomSettings);
	DOREPLIFETIME(ALobbyGameState, ReadyPlayerCount);
	DOREPLIFETIME(ALobbyGameState, TotalPlayerCount);	
}

void ALobbyGameState::UpdateRoomSettings(int32 MaxPlayers, const FString& MatchType, const FString& SessionTitle,
	ESessionVisibility Visibility, const FString& SelectedMap, const FString& GameMode)
{
	if (!HasAuthority())
	{
		return;
	}

	RoomSettings.MaxPlayers = MaxPlayers;
	RoomSettings.MatchType = MatchType;
	RoomSettings.SessionTitle = SessionTitle;
	RoomSettings.Visibility = Visibility;
	RoomSettings.SelectedMap = SelectedMap;
	RoomSettings.GameMode = GameMode;

	UE_LOG(LogTemp, Log, TEXT("[LobbyGameState] Room settings updated: MaxPlayers=%d, MatchType=%s, Title=%s"),
		MaxPlayers, *MatchType, *SessionTitle);

	OnRoomSettingsUpdated();
}

void ALobbyGameState::SetReadyCounts(int32 InReadyPlayerCount, int32 InTotalPlayerCount)
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bReadyChanged = ReadyPlayerCount != InReadyPlayerCount;
	const bool bTotalChanged = TotalPlayerCount != InTotalPlayerCount;

	if (!bReadyChanged && !bTotalChanged)
	{
		return;
	}

	ReadyPlayerCount = InReadyPlayerCount;
	TotalPlayerCount = InTotalPlayerCount;

	// 서버에서도 즉시 Blueprint 이벤트 호출 (클라이언트는 OnRep에서 호출)
	OnReadyCountChanged(ReadyPlayerCount, TotalPlayerCount);
}

void ALobbyGameState::OnRep_RoomSettings()
{
	// 클라이언트에서 Room 세팅이 변경되었을 때 호출
	UE_LOG(LogTemp, Log, TEXT("[LobbyGameState] Room settings replicated: MaxPlayers=%d, MatchType=%s, Title=%s"),
		RoomSettings.MaxPlayers, *RoomSettings.MatchType, *RoomSettings.SessionTitle);

	// Blueprint 이벤트로 UI 업데이트 알림
	OnRoomSettingsUpdated();
}

void ALobbyGameState::OnRep_ReadyCounts()
{
	UE_LOG(LogTemp, Log, TEXT("[LobbyGameState] Ready count replicated: %d / %d"),
		ReadyPlayerCount, TotalPlayerCount);

	OnReadyCountChanged(ReadyPlayerCount, TotalPlayerCount);
}

void ALobbyGameState::MulticastPlayerReadyStatusChanged_Implementation(const FString& PlayerId, bool bIsReady)
{
	UE_LOG(LogTemp, Log, TEXT("[LobbyGameState] Player %s ready status changed: %s"), *PlayerId, bIsReady ? TEXT("Ready") : TEXT("Not Ready"));

	// 모든 클라이언트에서 플레이어 위젯 업데이트
	UWorld* World = GetWorld();
	if (World)
	{
		for (TActorIterator<ABlasterCharacter> It(World); It; ++It)
		{
			if (ABlasterCharacter* Character = *It)
			{
				Character->UpdateOverheadWidget();
			}
		}
	}
}

void ALobbyGameState::MulticastStartFadeOnPlayers_Implementation(bool bFadeOut, float Duration)
{
	const float From = bFadeOut ? 0.f : 1.f;
	const float To = bFadeOut ? 1.f : 0.f;
	const bool bFadeAudio = true;
	const bool bHoldWhenFinished = bFadeOut;

	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{				
				if (PC->PlayerCameraManager)
				{
					PC->PlayerCameraManager->StartCameraFade(
						From,
						To,
						Duration,
						FLinearColor::Black,
						bHoldWhenFinished,
						bFadeAudio);
				}
			}
		}
	}
	OnFaded();
}

void ALobbyGameState::MulticastPlayerJoined_Implementation(const FString& PlayerName)
{
	UE_LOG(LogTemp, Log, TEXT("[LobbyGameState] Player joined: %s"), *PlayerName);

	// Blueprint 이벤트로 UI 업데이트 알림
	OnPlayerJoined(PlayerName);
}

void ALobbyGameState::MulticastPlayerLeft_Implementation(const FString& PlayerName)
{
	UE_LOG(LogTemp, Log, TEXT("[LobbyGameState] Player left: %s"), *PlayerName);

	// Blueprint 이벤트로 UI 업데이트 알림
	OnPlayerLeft(PlayerName);
}

