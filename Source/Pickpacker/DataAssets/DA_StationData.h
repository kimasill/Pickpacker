// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DA_StationData.generated.h"

/**
 * Station Type Enumeration
 */
UENUM(BlueprintType)
enum class EStationType : uint8
{
	Scanner		UMETA(DisplayName = "Scanner"),
	Labeler		UMETA(DisplayName = "Labeler"),
	Sorter		UMETA(DisplayName = "Sorter"),
	Unknown		UMETA(DisplayName = "Unknown")
};

/**
 * Station Configuration - Defines station behavior and parameters
 */
USTRUCT(BlueprintType)
struct BLASTER_API FStationConfig
{
	GENERATED_BODY()

	/** Station type */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station Config")
	EStationType StationType = EStationType::Unknown;

	/** Station name */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station Config")
	FString StationName = TEXT("Default Station");

	/** Gameplay tag for this station */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station Config")
	FGameplayTag StationTag;

	/** Base processing time in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station Config")
	float BaseProcessingTime = 5.0f;

	/** Difficulty multiplier (affects processing time and error tolerance) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station Config")
	float DifficultyMultiplier = 1.0f;

	/** Error tolerance (0.0 = perfect required, 1.0 = any result accepted) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station Config")
	float ErrorTolerance = 0.1f;

	/** Suspicion points added per error */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station Config")
	float SuspicionPerError = 10.0f;

	/** Required input tags (what this station can process) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station Config")
	FGameplayTagContainer RequiredInputTags;

	/** Output tags (what this station produces) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station Config")
	FGameplayTagContainer OutputTags;

	/** Socket points for UI/FX/IO */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station Config")
	TArray<FString> SocketPoints;

	FStationConfig()
	{
		StationType = EStationType::Unknown;
		StationName = TEXT("Default Station");
		BaseProcessingTime = 5.0f;
		DifficultyMultiplier = 1.0f;
		ErrorTolerance = 0.1f;
		SuspicionPerError = 10.0f;
	}
};

/**
 * Station Data Asset - Contains all station configurations for a level
 */
UCLASS(BlueprintType)
class BLASTER_API UDA_StationData : public UDataAsset
{
	GENERATED_BODY()

public:
	UDA_StationData();

	/** All station configurations */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stations")
	TArray<FStationConfig> StationConfigs;

	/** Global station parameters */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Global Parameters")
	TMap<FString, float> GlobalParameters;

	/**
	 * Get station config by type
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station Data")
	bool GetStationConfigByType(EStationType StationType, FStationConfig& OutConfig) const;

	/**
	 * Get station config by tag
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station Data")
	bool GetStationConfigByTag(const FGameplayTag& StationTag, FStationConfig& OutConfig) const;

	/**
	 * Get all station configs of a specific type
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station Data")
	TArray<FStationConfig> GetStationConfigsByType(EStationType StationType) const;

	/**
	 * Get global parameter value
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station Data")
	float GetGlobalParameter(const FString& ParameterName, float DefaultValue = 0.0f) const;
};
