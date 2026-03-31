#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/SoftObjectPtr.h"
#include "LevelSequence.h"
#include "MovieSceneSequencePlaybackSettings.h"

#include "BlasterLevelSequenceLibrary.generated.h"

class ULevelSequencePlayer;
class ALevelSequenceActor;

UCLASS()
class BLASTER_API UBlasterLevelSequenceLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Blaster|Sequence", meta = (WorldContext = "WorldContextObject"))
	static bool CreateSequencePlayerSync(
		UObject* WorldContextObject,
		TSoftObjectPtr<ULevelSequence> SequenceAsset,
		const FMovieSceneSequencePlaybackSettings& PlaybackSettings,
		ULevelSequencePlayer*& OutPlayer,
		ALevelSequenceActor*& OutActor
	);
};
