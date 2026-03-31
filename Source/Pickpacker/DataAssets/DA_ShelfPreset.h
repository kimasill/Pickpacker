#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DA_ShelfPreset.generated.h"

/**
 * Defines expected items per shelf slot.
 */
USTRUCT(BlueprintType)
struct BLASTER_API FShelfSlotPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shelf")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shelf")
	FGameplayTagContainer ExpectedTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shelf")
	bool bRewardOnCorrectPlacement = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shelf")
	int32 CreditReward = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shelf")
	int32 CreditPenalty = 0;
};

/**
 * Data asset describing shelf slot presets.
 */
UCLASS(BlueprintType)
class BLASTER_API UDA_ShelfPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shelf")
	TArray<FShelfSlotPreset> SlotDefinitions;
};


