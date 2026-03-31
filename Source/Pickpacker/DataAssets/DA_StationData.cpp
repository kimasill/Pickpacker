// Fill out your copyright notice in the Description page of Project Settings.

#include "DA_StationData.h"

UDA_StationData::UDA_StationData()
{
	// Initialize with default station configs
	FStationConfig ScannerConfig;
	ScannerConfig.StationType = EStationType::Scanner;
	ScannerConfig.StationName = TEXT("Scanner Station");
	ScannerConfig.BaseProcessingTime = 3.0f;
	ScannerConfig.DifficultyMultiplier = 1.0f;
	ScannerConfig.ErrorTolerance = 0.05f;
	ScannerConfig.SuspicionPerError = 5.0f;
	ScannerConfig.SocketPoints.Add(TEXT("ScannerUI"));
	ScannerConfig.SocketPoints.Add(TEXT("ScannerFX"));
	StationConfigs.Add(ScannerConfig);

	FStationConfig LabelerConfig;
	LabelerConfig.StationType = EStationType::Labeler;
	LabelerConfig.StationName = TEXT("Labeler Station");
	LabelerConfig.BaseProcessingTime = 4.0f;
	LabelerConfig.DifficultyMultiplier = 1.0f;
	LabelerConfig.ErrorTolerance = 0.1f;
	LabelerConfig.SuspicionPerError = 8.0f;
	LabelerConfig.SocketPoints.Add(TEXT("LabelerUI"));
	LabelerConfig.SocketPoints.Add(TEXT("LabelerFX"));
	StationConfigs.Add(LabelerConfig);

	FStationConfig SorterConfig;
	SorterConfig.StationType = EStationType::Sorter;
	SorterConfig.StationName = TEXT("Sorter Station");
	SorterConfig.BaseProcessingTime = 6.0f;
	SorterConfig.DifficultyMultiplier = 1.0f;
	SorterConfig.ErrorTolerance = 0.15f;
	SorterConfig.SuspicionPerError = 12.0f;
	SorterConfig.SocketPoints.Add(TEXT("SorterUI"));
	SorterConfig.SocketPoints.Add(TEXT("SorterFX"));
	StationConfigs.Add(SorterConfig);
}

bool UDA_StationData::GetStationConfigByType(EStationType StationType, FStationConfig& OutConfig) const
{
	for (const FStationConfig& Config : StationConfigs)
	{
		if (Config.StationType == StationType)
		{
			OutConfig = Config;
			return true;
		}
	}
	return false;
}

bool UDA_StationData::GetStationConfigByTag(const FGameplayTag& StationTag, FStationConfig& OutConfig) const
{
	for (const FStationConfig& Config : StationConfigs)
	{
		if (Config.StationTag == StationTag)
		{
			OutConfig = Config;
			return true;
		}
	}
	return false;
}

TArray<FStationConfig> UDA_StationData::GetStationConfigsByType(EStationType StationType) const
{
	TArray<FStationConfig> Result;
	for (const FStationConfig& Config : StationConfigs)
	{
		if (Config.StationType == StationType)
		{
			Result.Add(Config);
		}
	}
	return Result;
}

float UDA_StationData::GetGlobalParameter(const FString& ParameterName, float DefaultValue) const
{
	if (const float* Value = GlobalParameters.Find(ParameterName))
	{
		return *Value;
	}
	return DefaultValue;
}
