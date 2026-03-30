#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "UObject/SoftObjectPtr.h"
#include "LevelSequence.h"
#include "MovieSceneSequencePlaybackSettings.h"

#include "BlasterAsyncLoadSequencePlayer.generated.h"

class ULevelSequencePlayer;
class ALevelSequenceActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBlasterSequencePlayerLoaded, ULevelSequencePlayer*, Player, ALevelSequenceActor*, Actor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBlasterSequencePlayerFailed);

UCLASS()
class BLASTER_API UBlasterAsyncLoadSequencePlayer : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Blaster|Sequence")
	FBlasterSequencePlayerLoaded OnLoaded;

	UPROPERTY(BlueprintAssignable, Category = "Blaster|Sequence")
	FBlasterSequencePlayerFailed OnFailed;

	UFUNCTION(BlueprintCallable, Category = "Blaster|Sequence", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UBlasterAsyncLoadSequencePlayer* LoadSequencePlayerAsync(
		UObject* WorldContextObject,
		TSoftObjectPtr<ULevelSequence> SequenceAsset,
		const FMovieSceneSequencePlaybackSettings& PlaybackSettings
	);

	virtual void Activate() override;

private:
	void HandleLoaded();
	bool TryCreatePlayer(ULevelSequence* Sequence);

	UPROPERTY()
	TObjectPtr<UObject> WorldContextObject;

	TSoftObjectPtr<ULevelSequence> SequenceAsset;
	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	TSharedPtr<struct FStreamableHandle> StreamableHandle;
};
