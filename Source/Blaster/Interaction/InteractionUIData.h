#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "InteractionUIData.generated.h"

UENUM(BlueprintType)
enum class EInteractionType : uint8
{
	None,
	Default,
	Pickup,
	Door,
	PlaceOnShelf,
	Unlock,
	Use,
	Talk,
	Inspect,
	Custom
};

USTRUCT(BlueprintType)
struct BLASTER_API FInteractionUIData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	EInteractionType InteractionType = EInteractionType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText ActionText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText DetailText;

	// 주 아이콘 (없으면 기본 키 아이콘 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FSlateBrush IconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Credits")
	bool bRequiresUnlock = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Credits")
	int32 UnlockCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Credits")
	int32 CurrentCredits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Credits")
	FText LockedMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Credits")
	FText UnlockedMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Visual")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Visual")
	bool bHighlight = false;
};


