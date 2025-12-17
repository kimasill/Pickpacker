// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Blaster/DataAssets/DA_ItemData.h"
#include "DA_ParcelData.generated.h"

class UStaticMesh;
class USoundBase;
class UNiagaraSystem;
class UMaterialInterface;

/**
 * Parcel Configuration - Defines parcel properties and behavior
 */
USTRUCT(BlueprintType)
struct BLASTER_API FParcelConfig
{
	GENERATED_BODY()

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

	/** Special properties */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config")
	TMap<FString, float> SpecialProperties;

	/** Base sell price when submitted */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config|Economy")
	int32 BasePrice = 10;

	/** Classification tag (ex: Parcel-Classification.Standard) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config|Tags")
	FGameplayTag ClassificationTag;

	/** Unpackaged mesh (what players see when it is open) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config|Appearance")
	TSoftObjectPtr<UStaticMesh> UnpackagedMeshAsset;

	/** Packaged mesh (what players see on the belt) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config|Appearance")
	TSoftObjectPtr<UStaticMesh> PackagedMeshAsset;

	/** Default item data contained inside the parcel */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config|Contents")
	FItemData ItemData;

	/** Audio overrides for this parcel (overrides global audio) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config|AV")
	TMap<FName, TSoftObjectPtr<USoundBase>> AudioOverrides;

	/** VFX overrides for this parcel (overrides global VFX) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config|AV")
	TMap<FName, TSoftObjectPtr<UNiagaraSystem>> VfxOverrides;

	/** Decal overrides for this parcel (overrides global decals) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config|AV")
	TMap<FName, TSoftObjectPtr<UMaterialInterface>> DecalOverrides;

	FParcelConfig()
	{
		ParcelName = TEXT("Default Parcel");
		BaseWeight = 1.0f;
		BaseDurability = 100.0f;
		InstabilityFactor = 0.0f;
		SuspicionPoints = 0.0f;
		ProcessingTime = 3.0f;
		BasePrice = 10;
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

	/** Global audio map (key: event name, value: sound asset) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Global Parameters|AV")
	TMap<FName, TSoftObjectPtr<USoundBase>> GlobalAudioMap;

	/** Global VFX map (key: event name, value: Niagara system) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Global Parameters|AV")
	TMap<FName, TSoftObjectPtr<UNiagaraSystem>> GlobalVfxMap;

	/** Global decal map (key: event name, value: decal material) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Global Parameters|AV")
	TMap<FName, TSoftObjectPtr<UMaterialInterface>> GlobalDecalMap;

	/**
	 * Get parcel config by tag
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel Data")
	bool GetParcelConfigByTag(const FGameplayTag& ParcelTag, FParcelConfig& OutConfig) const;

	/**
	 * Get parcel config by row name (index in array)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel Data")
	bool GetParcelConfigByName(const FName& RowName, FParcelConfig& OutConfig) const;

	/**
	 * Get global parameter value
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel Data")
	float GetGlobalParameter(const FString& ParameterName, float DefaultValue = 0.0f) const;
};
