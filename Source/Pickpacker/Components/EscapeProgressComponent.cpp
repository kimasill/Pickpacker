#include "EscapeProgressComponent.h"

#include "Net/UnrealNetwork.h"
#include "DataAssets/DA_EndingData.h"
#include "GameState/PickpackerGameState.h"
#include "GameMode/PickpackerGameMode.h"
#include "PlayerController/BlasterPlayerController.h"
#include "Subsystem/CoreLoopSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"

namespace
{
	static void AppendDebugLog_EscapeProgress(const FString& JsonLine)
	{
#if !UE_BUILD_SHIPPING
		const FString LogDir = FPaths::ProjectSavedDir() + TEXT("Logs/BlasterDebug.log");
		FFileHelper::SaveStringToFile(JsonLine + LINE_TERMINATOR, *LogDir, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
#endif
	}

	static FString BuildTravelURL(UWorld* World, const FString& LevelPath)
	{
		if (LevelPath.IsEmpty())
		{
			return FString();
		}

		FString TravelURL = LevelPath;
		if (World && World->GetNetMode() == NM_ListenServer && !TravelURL.Contains(TEXT("?listen")))
		{
			TravelURL += TEXT("?listen");
		}
		return TravelURL;
	}
}

UEscapeProgressComponent::UEscapeProgressComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

FWorldFlagEntry* UEscapeProgressComponent::FindWorldFlagEntry(const FGameplayTag& Flag)
{
	return WorldFlags.FindByPredicate([&Flag](const FWorldFlagEntry& Entry)
		{
			return Entry.Flag == Flag;
		});
}

const FWorldFlagEntry* UEscapeProgressComponent::FindWorldFlagEntry(const FGameplayTag& Flag) const
{
	return WorldFlags.FindByPredicate([&Flag](const FWorldFlagEntry& Entry)
		{
			return Entry.Flag == Flag;
		});
}

void UEscapeProgressComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEscapeProgressComponent, WorldFlags);
	DOREPLIFETIME(UEscapeProgressComponent, RouteProgress);
	DOREPLIFETIME(UEscapeProgressComponent, AuthorizedPlayerCount);
	DOREPLIFETIME(UEscapeProgressComponent, CurrentEndingId);
}

void UEscapeProgressComponent::SetWorldFlag(const FGameplayTag& Flag, int32 Value)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (FWorldFlagEntry* Entry = FindWorldFlagEntry(Flag))
	{
		Entry->Value = Value;
	}
	else
	{
		FWorldFlagEntry NewEntry;
		NewEntry.Flag = Flag;
		NewEntry.Value = Value;
		WorldFlags.Add(MoveTemp(NewEntry));
	}

	if (UWorld* World = GetWorld())
	{
		if (UCoreLoopSubsystem* CoreLoop = World->GetSubsystem<UCoreLoopSubsystem>())
		{
			FEndingFlagState EndingFlagState;
			EndingFlagState.FlagId = Flag.GetTagName();
			EndingFlagState.bUnlocked = Value > 0;
			EndingFlagState.bLocked = Value < 0;
			CoreLoop->SetEndingFlagState(EndingFlagState);
		}
	}

	// 월드 플래그 변경 시 즉시 엔딩 조건 재평가
	EvaluateEndings();
}

int32 UEscapeProgressComponent::GetWorldFlag(const FGameplayTag& Flag) const
{
	if (const FWorldFlagEntry* Entry = FindWorldFlagEntry(Flag))
	{
		return Entry->Value;
	}
	return 0;
}

bool UEscapeProgressComponent::IsWorldFlagAtLeast(const FGameplayTag& Flag, int32 MinValue) const
{
	return GetWorldFlag(Flag) >= MinValue;
}

void UEscapeProgressComponent::RestoreWorldFlags(const TArray<FWorldFlagEntry>& InWorldFlags, bool bReevaluateEndings)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	WorldFlags = InWorldFlags;

	if (bReevaluateEndings)
	{
		EvaluateEndings();
	}
}

bool UEscapeProgressComponent::IsWorldFlagInRange(const FGameplayTag& Flag, int32 MinValue, int32 MaxValue) const
{
	const int32 Value = GetWorldFlag(Flag);
	return (MinValue < 0 || Value >= MinValue) && (MaxValue < 0 || Value <= MaxValue);
}

void UEscapeProgressComponent::AddAuthorizedPlayer(APlayerState* PlayerState)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !PlayerState)
	{
		return;
	}

	const TWeakObjectPtr<APlayerState> Key(PlayerState);
	if (!AuthorizedPlayers.Contains(Key))
	{
		AuthorizedPlayers.Add(Key);
		AuthorizedPlayerCount = AuthorizedPlayers.Num();
		EvaluateEndings();
	}
}

bool UEscapeProgressComponent::StartEndingById(const FName& EndingId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || EndingId.IsNone())
	{
		return false;
	}

	for (const UDA_EndingData* EndingData : EndingDataAssets)
	{
		if (EndingData && EndingData->EndingId == EndingId)
		{
			StartEnding(EndingData);
			return true;
		}
	}
	return false;
}

void UEscapeProgressComponent::EvaluateEndings()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// 이미 엔딩이 시작된 경우 추가 트리거 방지
	if (!CurrentEndingId.IsNone())
	{
		return;
	}

	// 우선순위 정렬된 배열 생성
	TArray<TObjectPtr<UDA_EndingData>> SortedEndings = EndingDataAssets;
	SortedEndings.Sort([](const UDA_EndingData& A, const UDA_EndingData& B) {
		return A.EvaluationPriority > B.EvaluationPriority;
	});

	for (const UDA_EndingData* EndingData : SortedEndings)
	{
		if (!EndingData)
		{
			continue;
		}

		if (IsEndingConditionMet(EndingData))
		{
			StartEnding(EndingData);
			return;
		}
	}
}

bool UEscapeProgressComponent::IsEndingConditionMet(const UDA_EndingData* EndingData) const
{
	if (!EndingData)
	{
		return false;
	}

	// 필수 월드 플래그 체크
	for (const FGameplayTag& Flag : EndingData->RequiredWorldFlags)
	{
		const int32 Value = GetWorldFlag(Flag);
		if (Value <= 0)
		{
			return false;
		}
	}

	// 인증 플레이어 수 체크
	if (EndingData->RequiredAuthorizedPlayers > 0 && AuthorizedPlayerCount < EndingData->RequiredAuthorizedPlayers)
	{
		return false;
	}

	// 페르소나 티어 체크 (WorldFlag.Persona 태그 사용 가정)
	const FGameplayTag PersonaTag = FGameplayTag::RequestGameplayTag(FName("WorldFlag.Persona"), false);
	if (PersonaTag.IsValid())
	{
		if (!IsWorldFlagInRange(PersonaTag, EndingData->MinPersonaTier, EndingData->MaxPersonaTier))
		{
			return false;
		}
	}

	return true;
}

void UEscapeProgressComponent::StartEnding(const UDA_EndingData* EndingData)
{
	if (!EndingData || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!CurrentEndingId.IsNone())
	{
		return;
	}

	CurrentEndingId = EndingData->EndingId;

	if (UWorld* World = GetWorld())
	{
		if (UCoreLoopSubsystem* CoreLoop = World->GetSubsystem<UCoreLoopSubsystem>())
		{
			FEndingFlagState EndingFlagState;
			EndingFlagState.FlagId = CurrentEndingId;
			EndingFlagState.bUnlocked = true;
			EndingFlagState.bLocked = false;
			CoreLoop->SetEndingFlagState(EndingFlagState);
		}
	}

	// 탈출 시퀀스 시작 시 오더 웨이브 제거 (탈출 과정 방해 방지)
	if (UWorld* World = GetWorld())
	{
		if (APickpackerGameMode* GameMode = World->GetAuthGameMode<APickpackerGameMode>())
		{
			GameMode->StopOrderWaves();
		}
	}

	// 모든 플레이어 입력 차단 및 HUD 전환 트리거
	BroadcastInputBlock(true);

	// 레벨 전환 전 시퀀스가 있는 경우 먼저 재생
	if (EndingData->TransitionSequence)
	{
		// 레벨 전환 정보 저장
		if (!EndingData->EndingLevel.IsNull())
		{
			if (UWorld* World = GetWorld())
			{
				PendingLevelPath = BuildTravelURL(World, EndingData->EndingLevel.GetLongPackageName());
			}
		}
		PendingEndingSequence = EndingData->EndingSequence;
		
		// 전환 시퀀스 재생
		Multicast_PlayTransitionSequence(EndingData->TransitionSequence);
	}
	else
	{
		// 전환 시퀀스가 없으면 바로 레벨 전환 또는 엔딩 시퀀스 재생
		if (EndingData->EndingLevel.IsNull())
		{
			Multicast_PlayEndingSequence(EndingData->EndingSequence);
		}
		else
		{
			const FString LevelPath = EndingData->EndingLevel.GetLongPackageName();
			const FString ServerTravelURL = BuildTravelURL(GetWorld(), LevelPath);
			if (ServerTravelURL.IsEmpty())
			{
				Multicast_PlayEndingSequence(EndingData->EndingSequence);
			}
			else if (UWorld* World = GetWorld())
			{
				for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
				{
					if (ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(It->Get()))
					{
						PC->Client_ShowEndingBlackScreen();
					}
				}
				World->ServerTravel(ServerTravelURL, true);
			}
		}
	}

	if (bReturnToLobbyAfterEnding && EndingData->EndingLevel.IsNull())
	{
		float TotalDuration = 0.0f;
		if (EndingData->TransitionSequence)
		{
			TotalDuration += GetSequenceDuration(EndingData->TransitionSequence);
		}
		if (EndingData->EndingSequence)
		{
			TotalDuration += GetSequenceDuration(EndingData->EndingSequence);
		}
		ScheduleReturnToLobby(TotalDuration + PostEndingDelay);
	}
}

void UEscapeProgressComponent::BroadcastInputBlock(bool bBlocked)
{
	if (!GetWorld())
	{
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(It->Get()))
		{
			PC->SetInputBlocked(bBlocked);
		}
	}
}

void UEscapeProgressComponent::Multicast_PlayEndingSequence_Implementation(ULevelSequence* Sequence)
{
	if (!Sequence || !GetWorld())
	{
		return;
	}

	ALevelSequenceActor* OutActor = nullptr;
	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bPauseAtEnd = true;

	if (ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(GetWorld(), Sequence, Settings, OutActor))
	{
		if (GetOwner() && GetOwner()->HasAuthority())
		{
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				if (ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(It->Get()))
				{
					PC->Client_HideForSequence();
				}
			}
		}
		Player->Play();
	}
}

void UEscapeProgressComponent::Multicast_PlayTransitionSequence_Implementation(ULevelSequence* Sequence)
{
	if (!Sequence || !GetWorld())
	{
		return;
	}

	ALevelSequenceActor* OutActor = nullptr;
	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bPauseAtEnd = true;

	if (ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(GetWorld(), Sequence, Settings, OutActor))
	{
		// 서버에서만 시퀀스 완료 콜백 바인딩 (레벨 전환은 서버에서만 실행)
		if (GetOwner() && GetOwner()->HasAuthority())
		{
			CurrentSequencePlayer = Player;
			Player->OnFinished.AddDynamic(this, &UEscapeProgressComponent::OnTransitionSequenceFinished);
			// 각 클라이언트에 플레이어/HUD 숨김 지시 (Client RPC)
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				if (ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(It->Get()))
				{
					PC->Client_HideForSequence();
				}
			}
		}
		OnSequenceStarted.Broadcast(Sequence);
		Player->Play();
	}
}

void UEscapeProgressComponent::OnTransitionSequenceFinished()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// 전환 시퀀스 완료 후 레벨 전환 또는 엔딩 시퀀스 재생
	if (!PendingLevelPath.IsEmpty())
	{
		// 엔딩 레벨 전환 직전 검은 화면 표시 (클라이언트가 로드 중 화면 노출 방지)
		if (UWorld* World = GetWorld())
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(It->Get()))
				{
					PC->Client_ShowEndingBlackScreen();
				}
			}
			World->ServerTravel(PendingLevelPath);
		}
	}
	else if (PendingEndingSequence)
	{
		// 레벨이 없으면 엔딩 시퀀스 재생
		Multicast_PlayEndingSequence(PendingEndingSequence);
	}

	// 정리
	PendingLevelPath.Empty();
	PendingEndingSequence = nullptr;
	
	if (CurrentSequencePlayer)
	{
		CurrentSequencePlayer->OnFinished.RemoveDynamic(this, &UEscapeProgressComponent::OnTransitionSequenceFinished);
		CurrentSequencePlayer = nullptr;
	}
}
float UEscapeProgressComponent::GetSequenceDuration(ULevelSequence* Sequence) const
{
	if (!Sequence || !Sequence->GetMovieScene())
	{
		return 0.0f;
	}

	const UMovieScene* MovieScene = Sequence->GetMovieScene();
	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
	const FFrameNumber Start = PlaybackRange.GetLowerBoundValue();
	const FFrameNumber End = PlaybackRange.GetUpperBoundValue();
	const int32 DurationFrames = (End - Start).Value;

	return TickResolution.AsSeconds(DurationFrames);
}

void UEscapeProgressComponent::ScheduleReturnToLobby(float TotalDelay)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bReturnScheduled)
	{
		return;
	}

	if (LobbyTravelPath.IsEmpty())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		bReturnScheduled = true;
		World->GetTimerManager().SetTimer(
			ReturnToLobbyTimerHandle,
			this,
			&UEscapeProgressComponent::ReturnPlayersToLobby,
			FMath::Max(0.1f, TotalDelay),
			false
		);
	}
}

void UEscapeProgressComponent::ReturnPlayersToLobby()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		// #region agent log
		AppendDebugLog_EscapeProgress(FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H20\",\"location\":\"EscapeProgressComponent.cpp:372\",\"message\":\"ReturnPlayersToLobby\",\"data\":{\"world\":\"%s\"},\"timestamp\":%lld}"),
			World ? *World->GetMapName() : TEXT("none"),
			FDateTime::UtcNow().ToUnixTimestamp() * 1000));
		// #endregion
		const FString TravelPath = FString::Printf(TEXT("%s?listen"), *LobbyTravelPath);
		World->ServerTravel(TravelPath, true);
	}
}

void UEscapeProgressComponent::OnRep_CurrentEndingId()
{
	// 클라이언트에서 엔딩 시작 시 입력 차단
	if (!CurrentEndingId.IsNone())
	{
		BroadcastInputBlock(true);
	}
}























































