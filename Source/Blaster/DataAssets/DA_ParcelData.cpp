// Fill out your copyright notice in the Description page of Project Settings.

#include "DA_ParcelData.h"
#include "GameplayTagsManager.h"

UDA_ParcelData::UDA_ParcelData()
{
	// Initialize with default parcel configs
	FParcelConfig StandardConfig;
	StandardConfig.ParcelName = TEXT("Standard Parcel");
	StandardConfig.ClassificationTag = FGameplayTag::RequestGameplayTag(TEXT("Parcel-Classification.Standard"));
	ParcelConfigs.Add(StandardConfig);

	FParcelConfig FragileConfig;
	FragileConfig.ParcelName = TEXT("Fragile Parcel");
	FragileConfig.BaseWeight = 0.8f;
	FragileConfig.BaseDurability = 50.0f;
	FragileConfig.ClassificationTag = FGameplayTag::RequestGameplayTag(TEXT("Parcel-Classification.Fragile"));
	ParcelConfigs.Add(FragileConfig);

	FParcelConfig ContrabandConfig;
	ContrabandConfig.ParcelName = TEXT("Contraband Parcel");
	ContrabandConfig.BaseDurability = 80.0f;
	ContrabandConfig.InstabilityFactor = 0.5f;
	ContrabandConfig.SuspicionPoints = 25.0f;
	ContrabandConfig.ClassificationTag = FGameplayTag::RequestGameplayTag(TEXT("Parcel-Classification.Contraband"));
	ParcelConfigs.Add(ContrabandConfig);
}

bool UDA_ParcelData::GetParcelConfigByTag(const FGameplayTag& ParcelTag, FParcelConfig& OutConfig) const
{
	for (const FParcelConfig& Config : ParcelConfigs)
	{
		if (Config.ParcelTag == ParcelTag)
		{
			OutConfig = Config;
			return true;
		}
	}
	return false;
}

bool UDA_ParcelData::GetParcelConfigByName(const FName& RowName, FParcelConfig& OutConfig) const
{
	if (RowName == NAME_None)
	{
		return false;
	}

	// 1) 이름 매칭 (ParcelName 또는 TagName)
	for (const FParcelConfig& Config : ParcelConfigs)
	{
		if (Config.ParcelTag.GetTagName() == RowName || FName(*Config.ParcelName) == RowName)
		{
			OutConfig = Config;
			return true;
		}
	}

	// 2) 숫자 인덱스 해석
	const FString RowNameString = RowName.ToString();
	if (RowNameString.IsNumeric())
	{
		const int32 Index = FCString::Atoi(*RowNameString);
		if (ParcelConfigs.IsValidIndex(Index))
		{
			OutConfig = ParcelConfigs[Index];
			return true;
		}
	}

	return false;
}

TArray<FName> UDA_ParcelData::GetParcelRowOptions() const
{
	TArray<FName> Options;
	const int32 Num = ParcelConfigs.Num();
	Options.Reserve(Num);

	for (int32 Index = 0; Index < Num; ++Index)
	{
		const FParcelConfig& Config = ParcelConfigs[Index];
		FString Label = Config.ParcelName;
		if (Label.IsEmpty())
		{
			Label = Config.ParcelTag.IsValid()
				? Config.ParcelTag.ToString()
				: FString::Printf(TEXT("Parcel_%d"), Index);
		}
		Options.Add(FName(*Label));
	}

	return Options;
}

float UDA_ParcelData::GetGlobalParameter(const FString& ParameterName, float DefaultValue) const
{
	if (const float* Value = GlobalParameters.Find(ParameterName))
	{
		return *Value;
	}
	return DefaultValue;
}
