// DA_TrainDestinationData - Data asset for defining available train destinations

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PickpackerTypes/CoreLoopTypes.h"
#include "DA_TrainDestinationData.generated.h"

/**
 * Data asset holding the full list of train destinations.
 * Assigned to the GameMode or registered with the CoreLoopSubsystem at level setup.
 */
UCLASS(BlueprintType)
class PICKPACKER_API UDA_TrainDestinationData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** All available destinations */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Train")
	TArray<FTrainDestination> Destinations;

	/** Get a destination by ID */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Train")
	bool GetDestinationById(const FName& DestinationId, FTrainDestination& OutDest) const
	{
		for (const FTrainDestination& Dest : Destinations)
		{
			if (Dest.DestinationId == DestinationId)
			{
				OutDest = Dest;
				return true;
			}
		}
		return false;
	}

	/** Get all destinations of a given difficulty */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Train")
	TArray<FTrainDestination> GetDestinationsByDifficulty(EZoneDifficulty Difficulty) const
	{
		TArray<FTrainDestination> Result;
		for (const FTrainDestination& Dest : Destinations)
		{
			if (Dest.Difficulty == Difficulty)
			{
				Result.Add(Dest);
			}
		}
		return Result;
	}
};
