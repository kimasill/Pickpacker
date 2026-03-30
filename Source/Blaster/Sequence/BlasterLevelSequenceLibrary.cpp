#include "BlasterLevelSequenceLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "LevelSequencePlayer.h"

bool UBlasterLevelSequenceLibrary::CreateSequencePlayerSync(
	UObject* WorldContextObject,
	TSoftObjectPtr<ULevelSequence> SequenceAsset,
	const FMovieSceneSequencePlaybackSettings& PlaybackSettings,
	ULevelSequencePlayer*& OutPlayer,
	ALevelSequenceActor*& OutActor)
{
	OutPlayer = nullptr;
	OutActor = nullptr;

	if (!WorldContextObject)
	{
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!World)
	{
		return false;
	}

	ULevelSequence* Sequence = SequenceAsset.LoadSynchronous();
	if (!Sequence)
	{
		return false;
	}

	OutPlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(World, Sequence, PlaybackSettings, OutActor);
	return OutPlayer != nullptr;
}
