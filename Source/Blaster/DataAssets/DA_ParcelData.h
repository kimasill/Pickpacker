// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DA_ParcelData.generated.h"

/**
 * Parcel Type Enumeration
 */
UENUM(BlueprintType)
enum class EParcelType : uint8
{
	Standard	UMETA(DisplayName = "Standard"),
	Fragile		UMETA(DisplayName = "Fragile"),
	Heavy		UMETA(DisplayName = "Heavy"),
	Contraband	UMETA(DisplayName = "Contraband"),
	Unknown		UMETA(DisplayName = "Unknown")
};

/**
 * Parcel Configuration - Defines parcel properties and behavior
 */
USTRUCT(BlueprintType)
struct BLASTER_API FParcelConfig
{
	GENERATED_BODY()

	/** Parcel type */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config")
	EParcelType ParcelType = EParcelType::Unknown;

	/** Parcel name */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config")
	FString ParcelName = TEXT("Default Parcel");

	/** Gameplay tag for this parcel */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config")
	FGameplayTag ParcelTag;

	/** Base weight (affects carrying speed and suspicion) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config")
	float BaseWeight = 1.0f;

	/** Base durability (how much damage it can take) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config")
	float BaseDurability = 100.0f;

	/** Instability factor (affects random events) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config")
	float InstabilityFactor = 0.0f;

	/** Suspicion points when carrying */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config")
	float SuspicionPoints = 0.0f;

	/** Required processing time at stations */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config")
	float ProcessingTime = 3.0f;

	/** Required input tags (what stations can process this) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config")
	FGameplayTagContainer RequiredInputTags;

	/** Output tags (what this parcel produces) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config")
	FGameplayTagContainer OutputTags;

	/** Special properties */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config")
	TMap<FString, float> SpecialProperties;

	FParcelConfig()
	{
		ParcelType = EParcelType::Unknown;
		ParcelName = TEXT("Default Parcel");
		BaseWeight = 1.0f;
		BaseDurability = 100.0f;
		InstabilityFactor = 0.0f;
		SuspicionPoints = 0.0f;
		ProcessingTime = 3.0f;
	}
};

/**
 * Parcel Data Asset - Contains all parcel configurations for a level
 */
UCLASS(BlueprintType)
class BLASTER_API UDA_ParcelData : public UDataAsset
{
	GENERATED_BODY()

public:
	UDA_ParcelData();

	/** All parcel configurations */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcels")
	TArray<FParcelConfig> ParcelConfigs;

	/** Global parcel parameters */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Global Parameters")
	TMap<FString, float> GlobalParameters;

	/**
	 * Get parcel config by type
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel Data")
	bool GetParcelConfigByType(EParcelType ParcelType, FParcelConfig& OutConfig) const;

	/**
	 * Get parcel config by tag
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel Data")
	bool GetParcelConfigByTag(const FGameplayTag& ParcelTag, FParcelConfig& OutConfig) const;

	/**
	 * Get all parcel configs of a specific type
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel Data")
	TArray<FParcelConfig> GetParcelConfigsByType(EParcelType ParcelType) const;

	/**
	 * Get global parameter value
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel Data")
	float GetGlobalParameter(const FString& ParameterName, float DefaultValue = 0.0f) const;
};
