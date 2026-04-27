#pragma once

#include "CoreMinimal.h"
#include "PickpackerTypes/CoreLoopTypes.h"
#include "NPCDialogueUIData.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct PICKPACKER_API FDialogueChoiceIndicatorUIData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EDialogueChoiceIndicatorType IndicatorType = EDialogueChoiceIndicatorType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText IndicatorText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag RelatedTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TObjectPtr<UTexture2D> IndicatorIcon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bIsSatisfied = true;
};

USTRUCT(BlueprintType)
struct PICKPACKER_API FDialogueChoiceUIData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 ChoiceIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText ChoiceText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FDialogueChoiceIndicatorUIData> UnlockIndicators;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bIsEnabled = true;
};

USTRUCT(BlueprintType)
struct PICKPACKER_API FDialogueUIState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText SpeakerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText DialogueText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FDialogueChoiceUIData> Choices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bCanAdvance = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bCanExit = true;
};
