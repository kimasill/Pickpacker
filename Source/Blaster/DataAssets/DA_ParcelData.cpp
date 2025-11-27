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

float UDA_ParcelData::GetGlobalParameter(const FString& ParameterName, float DefaultValue) const
{
	if (const float* Value = GlobalParameters.Find(ParameterName))
	{
		return *Value;
	}
	return DefaultValue;
}
