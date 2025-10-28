// Fill out your copyright notice in the Description page of Project Settings.

#include "DA_ParcelData.h"

UDA_ParcelData::UDA_ParcelData()
{
	// Initialize with default parcel configs
	FParcelConfig StandardConfig;
	StandardConfig.ParcelType = EParcelType::Standard;
	StandardConfig.ParcelName = TEXT("Standard Parcel");
	StandardConfig.BaseWeight = 1.0f;
	StandardConfig.BaseDurability = 100.0f;
	StandardConfig.InstabilityFactor = 0.0f;
	StandardConfig.SuspicionPoints = 0.0f;
	StandardConfig.ProcessingTime = 3.0f;
	ParcelConfigs.Add(StandardConfig);

	FParcelConfig FragileConfig;
	FragileConfig.ParcelType = EParcelType::Fragile;
	FragileConfig.ParcelName = TEXT("Fragile Parcel");
	FragileConfig.BaseWeight = 0.8f;
	FragileConfig.BaseDurability = 50.0f;
	FragileConfig.InstabilityFactor = 0.3f;
	FragileConfig.SuspicionPoints = 5.0f;
	FragileConfig.ProcessingTime = 4.0f;
	ParcelConfigs.Add(FragileConfig);

	FParcelConfig HeavyConfig;
	HeavyConfig.ParcelType = EParcelType::Heavy;
	HeavyConfig.ParcelName = TEXT("Heavy Parcel");
	HeavyConfig.BaseWeight = 2.5f;
	HeavyConfig.BaseDurability = 150.0f;
	HeavyConfig.InstabilityFactor = 0.1f;
	HeavyConfig.SuspicionPoints = 10.0f;
	HeavyConfig.ProcessingTime = 5.0f;
	ParcelConfigs.Add(HeavyConfig);

	FParcelConfig ContrabandConfig;
	ContrabandConfig.ParcelType = EParcelType::Contraband;
	ContrabandConfig.ParcelName = TEXT("Contraband Parcel");
	ContrabandConfig.BaseWeight = 1.2f;
	ContrabandConfig.BaseDurability = 80.0f;
	ContrabandConfig.InstabilityFactor = 0.5f;
	ContrabandConfig.SuspicionPoints = 25.0f;
	ContrabandConfig.ProcessingTime = 6.0f;
	ParcelConfigs.Add(ContrabandConfig);
}

bool UDA_ParcelData::GetParcelConfigByType(EParcelType ParcelType, FParcelConfig& OutConfig) const
{
	for (const FParcelConfig& Config : ParcelConfigs)
	{
		if (Config.ParcelType == ParcelType)
		{
			OutConfig = Config;
			return true;
		}
	}
	return false;
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

TArray<FParcelConfig> UDA_ParcelData::GetParcelConfigsByType(EParcelType ParcelType) const
{
	TArray<FParcelConfig> Result;
	for (const FParcelConfig& Config : ParcelConfigs)
	{
		if (Config.ParcelType == ParcelType)
		{
			Result.Add(Config);
		}
	}
	return Result;
}

float UDA_ParcelData::GetGlobalParameter(const FString& ParameterName, float DefaultValue) const
{
	if (const float* Value = GlobalParameters.Find(ParameterName))
	{
		return *Value;
	}
	return DefaultValue;
}
