#include "EndingGameMode.h"

#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "MovieScene.h"
#include "Engine/World.h"
#include "TimerManager.h"

AEndingGameMode::AEndingGameMode()
{
	// 엔딩 맵은 짧은 연출용이므로 틱 불필요
	PrimaryActorTick.bCanEverTick = false;
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
	if (EndingSequenceAsset.IsValid())
	{
		PlayerController->ClientPlayEndingSequence(EndingSequenceAsset);
	}

	if (bReturnToLobbyAfterEnding && !bReturnScheduled && GetWorld())
	{
		bReturnScheduled = true;
		const float SequenceDuration = GetEndingSequenceDuration();
		const float Delay = FMath::Max(0.1f, SequenceDuration + PostEndingDelay);
		GetWorld()->GetTimerManager().SetTimer(
			ReturnToLobbyTimerHandle,
			this,
			&AEndingGameMode::ReturnPlayersToLobby,
			Delay,
			false
		);
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
	OnEndingPlayerReady(PlayerController);
}

float AEndingGameMode::GetEndingSequenceDuration() const
{
	if (EndingSequenceAsset.IsNull())
	{
		return 0.0f;
	}

	ULevelSequence* Sequence = EndingSequenceAsset.LoadSynchronous();
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

void AEndingGameMode::ReturnPlayersToLobby()
{
	if (!HasAuthority() || LobbyTravelPath.IsEmpty())
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

	const FString TravelPath = FString::Printf(TEXT("%s?listen"), *LobbyTravelPath);
	World->ServerTravel(TravelPath, true);
}

