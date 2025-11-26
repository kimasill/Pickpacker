#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Blaster/PickpackerTypes/PickpackerTypes.h"
#include "DA_OrderWaveData.generated.h"

/**
 * Data asset describing order waves for the Pickpacker mode
 */
UCLASS(BlueprintType)
class BLASTER_API UDA_OrderWaveData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Waves that will be spawned sequentially */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickpacker|Orders")
	TArray<FParcelOrderWave> OrderWaves;

	/** Retrieve a wave by index; returns nullptr if out of range */
	const FParcelOrderWave* GetWave(int32 WaveIndex) const;
};


