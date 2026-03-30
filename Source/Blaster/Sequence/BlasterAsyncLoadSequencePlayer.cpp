#include "BlasterAsyncLoadSequencePlayer.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "LevelSequencePlayer.h"

UBlasterAsyncLoadSequencePlayer* UBlasterAsyncLoadSequencePlayer::LoadSequencePlayerAsync(
	UObject* WorldContextObject,
	TSoftObjectPtr<ULevelSequence> SequenceAsset,
	const FMovieSceneSequencePlaybackSettings& PlaybackSettings)
{
	UBlasterAsyncLoadSequencePlayer* Action = NewObject<UBlasterAsyncLoadSequencePlayer>();
	Action->WorldContextObject = WorldContextObject;
	Action->SequenceAsset = SequenceAsset;
	Action->PlaybackSettings = PlaybackSettings;
	return Action;
}

void UBlasterAsyncLoadSequencePlayer::Activate()
{
	if (!WorldContextObject || SequenceAsset.IsNull())
	{
		OnFailed.Broadcast();
		SetReadyToDestroy();
		return;
	}

	if (SequenceAsset.IsValid())
	{
		if (!TryCreatePlayer(SequenceAsset.Get()))
		{
			OnFailed.Broadcast();
		}
		SetReadyToDestroy();
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	StreamableHandle = AssetManager.GetStreamableManager().RequestAsyncLoad(
		SequenceAsset.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &UBlasterAsyncLoadSequencePlayer::HandleLoaded)
	);

	if (!StreamableHandle.IsValid())
	{
		OnFailed.Broadcast();
		SetReadyToDestroy();
	}
}

void UBlasterAsyncLoadSequencePlayer::HandleLoaded()
{
	if (!TryCreatePlayer(SequenceAsset.Get()))
	{
		OnFailed.Broadcast();
	}
	SetReadyToDestroy();
}

bool UBlasterAsyncLoadSequencePlayer::TryCreatePlayer(ULevelSequence* Sequence)
{
	if (!WorldContextObject || !Sequence)
	{
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!World)
	{
		return false;
	}

	ALevelSequenceActor* OutActor = nullptr;
	ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(World, Sequence, PlaybackSettings, OutActor);
	if (!Player)
	{
		return false;
	}

	OnLoaded.Broadcast(Player, OutActor);
	return true;
}
