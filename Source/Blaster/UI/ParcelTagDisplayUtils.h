// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UDataTable;

class BLASTER_API FParcelTagDisplayUtils
{
public:
	static FString GetTagLastPart(const FGameplayTag& Tag);
	static bool TryGetTagDisplayText(const UDataTable* DataTable, const FGameplayTag& Tag, FText& OutText);
};
