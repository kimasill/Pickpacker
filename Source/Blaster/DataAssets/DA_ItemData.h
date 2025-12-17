// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
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
 * Item 소비 정책
 */
UENUM(BlueprintType)
enum class EItemConsumePolicy : uint8
{
	None,               // 소비/내구도 감소 없음
	ConsumeOnce,        // 한 번 사용 시 완전 소비
	DurabilityReduction // 내구도 감소 값 사용
};

/**
 * Item Property - key/value pair for item properties
 */
USTRUCT(BlueprintType)
struct BLASTER_API FItemProperty
{
	GENERATED_BODY()

	/** Property key */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data")
	FName Key = NAME_None;

	/** Property value */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data")
	float Value = 0.f;
};

/**
 * Item Data - Defines item properties and behavior
 */
USTRUCT(BlueprintType)
struct BLASTER_API FItemData
{
	GENERATED_BODY()

	/** Item gameplay identifier (tag based) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data")
	FGameplayTag ItemId;

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

	/** Gameplay tags describing what this item can do (UseAction.*) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data")
	FGameplayTagContainer UseActions;

	/** 사용 소비 정책 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data")
	EItemConsumePolicy ConsumePolicy = EItemConsumePolicy::None;

	/** 소비 정책이 DurabilityReduction일 때 감소값 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data", meta = (EditCondition = "ConsumePolicy == EItemConsumePolicy::DurabilityReduction", EditConditionHides))
	float DurabilityConsumeValue = 0.0f;

	/** 사용 쿨다운(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data")
	float UseCooldown = 0.0f;

	/** Special properties for item effects (replication/RPC safe) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item Data")
	TArray<FItemProperty> ItemProperties;

	FItemData()
	{
		ItemId = FGameplayTag();
		ItemType = EItemType::Unknown;
		ItemName = TEXT("Default Item");
		bIsUsable = false;
		bConsumedOnUse = false;
		ConsumePolicy = EItemConsumePolicy::None;
		DurabilityConsumeValue = 0.0f;
		UseCooldown = 0.0f;
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

