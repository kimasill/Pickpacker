#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DA_EndingData.generated.h"

class ULevelSequence;

UENUM(BlueprintType)
enum class EEndingType : uint8
{
    Ending_Bad UMETA(DisplayName = "Bad"),
    Ending_Normal UMETA(DisplayName = "Normal"),
    Ending_Fail UMETA(DisplayName = "Fail")
};

UCLASS()
class BLASTER_API UDA_EndingData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 엔딩 식별자 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ending")
	FName EndingId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ending")
	EEndingType EndingType = EEndingType::Ending_Bad;

	/** 필수 월드 플래그 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ending")
	TArray<FGameplayTag> RequiredWorldFlags;

	/** 요구되는 인증 플레이어 수 (0이면 무시) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ending")
	int32 RequiredAuthorizedPlayers = 0;

	/** 엔딩 시퀀스 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ending")
	TObjectPtr<ULevelSequence> EndingSequence = nullptr;

	/** 엔딩 메시지 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ending")
	FText EndingMessage;
};


