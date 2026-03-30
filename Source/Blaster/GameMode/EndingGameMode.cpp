#include "EndingGameMode.h"

#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/Sequence/BlasterLevelSequenceLibrary.h"
#include "Engine/World.h"

AEndingGameMode::AEndingGameMode()
{
	// 엔딩 맵은 짧은 연출용이므로 틱 불필요

	FMovieSceneSequenceLoopCount LoopCount;
	LoopCount.Value = 0;

	PrimaryActorTick.bCanEverTick = false;
	PlaybackSettings.bAutoPlay = false;	
	PlaybackSettings.LoopCount = LoopCount;
	PlaybackSettings.PlayRate = 1.0f;
	PlaybackSettings.bDisableMovementInput = true;
	PlaybackSettings.bDisableLookAtInput = true;
	PlaybackSettings.bHidePlayer = true;
	PlaybackSettings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::ForceRestoreState;
}

void AEndingGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(NewPlayer))
	{
		ApplyEndingSetup(PC);
	}
}

void AEndingGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	if (ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(NewPlayer))
	{
		ApplyEndingSetup(PC);
	}
}

void AEndingGameMode::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();
	RequestReadyFromAllPlayers();
}

UClass* AEndingGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (EndingPawnClass)
	{
		return EndingPawnClass;
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void AEndingGameMode::OnEndingPlayerReady(ABlasterPlayerController* PlayerController)
{
	OnEndingPlayerReadyBP(PlayerController, EndingSequenceAsset, PlaybackSettings);
}

void AEndingGameMode::PlayEndingSequence()
{
	if (EndingSequenceAsset.IsNull()) return;
	UObject* WorldContext = GetWorld();

	// 시퀀스 시작 직전 검은 화면 해제
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(It->Get()))
		{
			PC->Client_HideEndingBlackScreen();
		}
	}

	FMovieSceneSequencePlaybackSettings EffectiveSettings = PlaybackSettings;
	EffectiveSettings.bHidePlayer = bHidePlayersOnSequenceStart && bIncludeSelfInHide;

	UBlasterLevelSequenceLibrary::CreateSequencePlayerSync(WorldContext, EndingSequenceAsset, EffectiveSettings, SequencePlayer, SequenceActor);
	SequencePlayer->OnFinished.AddDynamic(
		this,
		&AEndingGameMode::OnSequenceFinished
	);

	SequencePlayer->Play();

	// 시퀀스 시작 시 플레이어 숨김 (디테일 패널 bHidePlayersOnSequenceStart)
	if (bHidePlayersOnSequenceStart)
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(It->Get()))
			{
				PC->Client_HideForSequence(bIncludeSelfInHide);
			}
		}
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(It->Get()))
		{
			if (PC->IsLocalController()) continue;
			const bool bHidePlayer = bHidePlayersOnSequenceStart && bIncludeSelfInHide;
			PC->ClientPlayEndingSequence(EndingSequenceAsset, bHidePlayer);
		}
	}
}

void AEndingGameMode::OnSequenceFinished()
{
	OnSequenceFinishedDelegate.Broadcast();

	// 크레딧 재생 (블루프린트 BP_PlayCredits에서 구현)
	// 크레딧 종료 시 블루프린트에서 ServerRequestTravelAfterCredits 호출
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(It->Get()))
		{
			PC->ClientPlayCredits();
		}
	}
}

void AEndingGameMode::RequestTravelAfterCredits()
{
	if (!HasAuthority() || bTravelAfterCreditsRequested)
	{
		return;
	}
	bTravelAfterCreditsRequested = true;
	TravelToMap(LobbyTravelPath);
}

void AEndingGameMode::RequestReadyFromAllPlayers()
{
	if (!HasAuthority() || bEndingSequenceStarted || EndingSequenceAsset.IsNull())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 기존 타이머 해제
	World->GetTimerManager().ClearTimer(EndingReadyTimerHandle);
	EndingLevelReadyPCs.Empty();

	// 모든 PC에 검은 화면 표시 + 준비 요청 전송
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(It->Get()))
		{
			PC->Client_ShowEndingBlackScreen();
			PC->Client_RequestEndingLevelReady();
		}
	}

	// 대기 시간 후 시퀀스 재생 시도 (늦게 도착한 플레이어 대비)
	World->GetTimerManager().SetTimer(
		EndingReadyTimerHandle,
		this,
		&AEndingGameMode::CheckAllPlayersReadyAndPlaySequence,
		EndingLevelReadyWaitTime,
		false
	);
}

void AEndingGameMode::OnPlayerEndingLevelReady(ABlasterPlayerController* PC)
{
	if (!PC || !HasAuthority() || bEndingSequenceStarted)
	{
		return;
	}

	EndingLevelReadyPCs.Add(PC);
	CheckAllPlayersReadyAndPlaySequence();
}

void AEndingGameMode::CheckAllPlayersReadyAndPlaySequence()
{
	if (!HasAuthority() || bEndingSequenceStarted || EndingSequenceAsset.IsNull())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 TotalPCs = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (Cast<ABlasterPlayerController>(It->Get()))
		{
			++TotalPCs;
		}
	}

	if (TotalPCs == 0)
	{
		return;
	}

	// 모든 PC가 준비했거나, 대기 시간이 지났으면 시퀀스 재생
	int32 ReadyCount = 0;
	for (const TWeakObjectPtr<ABlasterPlayerController>& WeakPC : EndingLevelReadyPCs)
	{
		if (WeakPC.IsValid())
		{
			++ReadyCount;
		}
	}

	const bool bAllReady = (ReadyCount >= TotalPCs);
	const bool bTimerExpired = !World->GetTimerManager().IsTimerActive(EndingReadyTimerHandle);
	if (bAllReady || bTimerExpired)
	{
		World->GetTimerManager().ClearTimer(EndingReadyTimerHandle);
		bEndingSequenceStarted = true;
		PlayEndingSequence();
	}
}

void AEndingGameMode::ApplyEndingSetup(ABlasterPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return;
	}

	if (bBlockInputOnLogin)
	{
		PlayerController->SetInputBlocked(true);
	}

	// 블루프린트로 추가 연출(시퀀스/카메라/UI)을 트리거할 수 있도록 이벤트 호출
	// 주의: PlayEndingSequence는 C++에서 모든 플레이어 준비 후 자동 호출됨 (블루프린트에서 호출하지 않음)
	OnEndingPlayerReady(PlayerController);

	// SeamlessTravel이 아닌 직접 진입 시(PostSeamlessTravel 미호출) 대비 - 마지막 플레이어 입장 기준으로 대기
	if (!bEndingSequenceStarted && HasAuthority() && !EndingSequenceAsset.IsNull())
	{
		RequestReadyFromAllPlayers();
	}
}

void AEndingGameMode::TravelToMap(const FString& MapPath)
{
	if (!HasAuthority() || MapPath.IsEmpty())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(It->Get()))
		{
			PC->SetInputBlocked(true);
			PC->ClientShowLoadingScreenWithKey(TEXT("Ready"), TEXT("Ready"), 0.75f);
		}
	}

	const FString TravelPath = FString::Printf(TEXT("%s?listen"), *MapPath);
	World->ServerTravel(TravelPath);
}