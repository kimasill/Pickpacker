// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DA_ItemData.generated.h"

/**
 * Item Type Enumeration
 */
UENUM(BlueprintType)
enum class EItemType : uint8
{
	Key			UMETA(DisplayName = "Key"),
	Tool		UMETA(DisplayName = "Tool"),
	Information	UMETA(DisplayName = "Information"),
	Evidence	UMETA(DisplayName = "Evidence"),
	Consumable	UMETA(DisplayName = "Consumable"),
	Unknown		UMETA(DisplayName = "Unknown")
};

/**
 * Item Data - Defines item properties and behavior
 */
USTRUCT(BlueprintType)
struct BLASTER_API FItemData
{
	GENERATED_BODY()

	/** Item type */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data")
	EItemType ItemType = EItemType::Unknown;

	/** Item name */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data")
	FString ItemName = TEXT("Default Item");

	/** Item description */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data")
	FText ItemDescription;

	/** Whether this item can be used */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data")
	bool bIsUsable = false;

	/** Whether item is consumed after use */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data")
	bool bConsumedOnUse = false;

	/** Special properties for item effects */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data")
	TMap<FString, float> ItemProperties;

	FItemData()
	{
		ItemType = EItemType::Unknown;
		ItemName = TEXT("Default Item");
		bIsUsable = false;
		bConsumedOnUse = false;
	}
};

/**
 * Item Data Asset - Contains all item configurations
 */
UCLASS(BlueprintType)
class BLASTER_API UDA_ItemData : public UDataAsset
{
	GENERATED_BODY()

public:
	UDA_ItemData();

	/** All item configurations */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items")
	TArray<FItemData> ItemConfigs;

	/**
	 * Get item data by type
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Item Data")
	bool GetItemDataByType(EItemType ItemType, FItemData& OutItemData) const;

	/**
	 * Get item data by name
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Item Data")
	bool GetItemDataByName(const FString& ItemName, FItemData& OutItemData) const;

	/**
	 * Get all items of a specific type
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Item Data")
	TArray<FItemData> GetItemsByType(EItemType ItemType) const;

	/**
	 * Get all usable items
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Item Data")
	TArray<FItemData> GetUsableItems() const;
};

