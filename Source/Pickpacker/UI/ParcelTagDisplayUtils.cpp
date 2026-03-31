// Fill out your copyright notice in the Description page of Project Settings.

#include "ParcelTagDisplayUtils.h"
#include "Engine/DataTable.h"
#include "Blaster/UI/ParcelTagDisplayData.h"

FString FParcelTagDisplayUtils::GetTagLastPart(const FGameplayTag& Tag)
{
	if (!Tag.IsValid())
	{
		return FString();
	}

	const FString TagString = Tag.ToString();
	int32 LastDotIndex = INDEX_NONE;
	if (TagString.FindLastChar(TEXT('.'), LastDotIndex))
	{
		return TagString.Mid(LastDotIndex + 1);
	}

	return TagString;
}

bool FParcelTagDisplayUtils::TryGetTagDisplayText(const UDataTable* DataTable, const FGameplayTag& Tag, FText& OutText)
{
	if (!DataTable || !Tag.IsValid())
	{
		return false;
	}

	const TArray<FName> RowNames = DataTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		const FParcelHUDTagDisplayRow* Row = DataTable->FindRow<FParcelHUDTagDisplayRow>(RowName, TEXT("ParcelHUDTagLookup"));
		if (!Row || !Row->Tag.IsValid())
		{
			continue;
		}

		if (Row->Tag.MatchesTagExact(Tag))
		{
			OutText = Row->DisplayText;
			return true;
		}
	}

	return false;
}
