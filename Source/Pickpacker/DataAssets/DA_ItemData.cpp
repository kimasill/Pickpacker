// Fill out your copyright notice in the Description page of Project Settings.

#include "DA_ItemData.h"
#include "GameplayTagsManager.h"

UDA_ItemData::UDA_ItemData()
{
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

