// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blaster/DataAssets/DA_ParcelData.h"
#include "ParcelAVLibrary.generated.h"

class USoundBase;
class UNiagaraSystem;
class UMaterialInterface;
class UAudioComponent;
class UNiagaraComponent;

/**
 * Static library for resolving audio/VFX assets for parcels
 */
UCLASS()
class BLASTER_API UParcelAVLibrary : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Resolve sound asset for a given event key
	 * Priority: Config.Overrides -> GlobalMap
	 */
	static USoundBase* ResolveSound(const FParcelConfig& Config, UDA_ParcelData* DataAsset, FName EventKey);

	/**
	 * Resolve VFX asset for a given event key
	 * Priority: Config.Overrides -> GlobalMap
	 */
	static UNiagaraSystem* ResolveVfx(const FParcelConfig& Config, UDA_ParcelData* DataAsset, FName EventKey);

	/**
	 * Resolve decal material for a given event key
	 * Priority: Config.Overrides -> GlobalMap
	 */
	static UMaterialInterface* ResolveDecal(const FParcelConfig& Config, UDA_ParcelData* DataAsset, FName EventKey);

	/**
	 * Get special property value from config
	 */
	static float GetSpecialProperty(const FParcelConfig& Config, const FString& PropertyName, float DefaultValue = 0.0f);
};

