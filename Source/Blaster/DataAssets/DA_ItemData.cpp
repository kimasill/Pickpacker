// Fill out your copyright notice in the Description page of Project Settings.

#include "DA_ItemData.h"

UDA_ItemData::UDA_ItemData()
{
	// Initialize with default item configs
	FItemData KeyItem;
	KeyItem.ItemType = EItemType::Key;
	KeyItem.ItemName = TEXT("Key");
	KeyItem.ItemDescription = FText::FromString(TEXT("A key that can unlock doors"));
	KeyItem.bIsUsable = true;
	KeyItem.bConsumedOnUse = false;
	ItemConfigs.Add(KeyItem);

	FItemData ToolItem;
	ToolItem.ItemType = EItemType::Tool;
	ToolItem.ItemName = TEXT("Tool");
	ToolItem.ItemDescription = FText::FromString(TEXT("A tool for disabling systems"));
	ToolItem.bIsUsable = true;
	ToolItem.bConsumedOnUse = false;
	ItemConfigs.Add(ToolItem);

	FItemData InformationItem;
	InformationItem.ItemType = EItemType::Information;
	InformationItem.ItemName = TEXT("Information");
	InformationItem.ItemDescription = FText::FromString(TEXT("Information document"));
	InformationItem.bIsUsable = false;
	ItemConfigs.Add(InformationItem);
}

bool UDA_ItemData::GetItemDataByType(EItemType ItemType, FItemData& OutItemData) const
{
	for (const FItemData& ItemData : ItemConfigs)
	{
		if (ItemData.ItemType == ItemType)
		{
			OutItemData = ItemData;
			return true;
		}
	}
	return false;
}

bool UDA_ItemData::GetItemDataByName(const FString& ItemName, FItemData& OutItemData) const
{
	for (const FItemData& ItemData : ItemConfigs)
	{
		if (ItemData.ItemName == ItemName)
		{
			OutItemData = ItemData;
			return true;
		}
	}
	return false;
}

TArray<FItemData> UDA_ItemData::GetItemsByType(EItemType ItemType) const
{
	TArray<FItemData> Result;
	for (const FItemData& ItemData : ItemConfigs)
	{
		if (ItemData.ItemType == ItemType)
		{
			Result.Add(ItemData);
		}
	}
	return Result;
}

TArray<FItemData> UDA_ItemData::GetUsableItems() const
{
	TArray<FItemData> Result;
	for (const FItemData& ItemData : ItemConfigs)
	{
		if (ItemData.bIsUsable)
		{
			Result.Add(ItemData);
		}
	}
	return Result;
}

