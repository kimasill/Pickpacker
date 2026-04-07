#pragma once

#include "CoreMinimal.h"
#include "ParcelRowNamePicker.generated.h"

class UDA_ParcelData;

/**
 * 에디터에서 DataAsset을 선택하면 해당 Asset의 RowName을 드롭다운으로 보여주기 위한 래퍼
 */
USTRUCT(BlueprintType)
struct PICKPACKER_API FParcelRowNamePicker
{
	GENERATED_BODY()

	/** 선택할 DataAsset (UDA_ParcelData) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel")
	TObjectPtr<UDA_ParcelData> ParcelData = nullptr;

	/** RowName (ParcelName 또는 TagName) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel")
	FName RowName = NAME_None;
};


