// Fill out your copyright notice in the Description page of Project Settings.

#include "ParcelAVLibrary.h"
#include "DataAssets/DA_ParcelData.h"
#include "Sound/SoundBase.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInterface.h"

USoundBase* UParcelAVLibrary::ResolveSound(const FParcelConfig& Config, UDA_ParcelData* DataAsset, FName EventKey)
{
    if (EventKey == NAME_None)
    {
        return nullptr;
    }

    // 1. Check config overrides first
    if (const TSoftObjectPtr<USoundBase>* OverridePtr = Config.AudioOverrides.Find(EventKey))
    {
        if (OverridePtr && !OverridePtr->IsNull())
        {
            return OverridePtr->LoadSynchronous();
        }
    }

    // 2. Check global map
    if (DataAsset)
    {
        if (const TSoftObjectPtr<USoundBase>* GlobalPtr = DataAsset->GlobalAudioMap.Find(EventKey))
        {
            if (GlobalPtr && !GlobalPtr->IsNull())
            {
                return GlobalPtr->LoadSynchronous();
            }
        }
    }

    return nullptr;
}

UNiagaraSystem* UParcelAVLibrary::ResolveVfx(const FParcelConfig& Config, UDA_ParcelData* DataAsset, FName EventKey)
{
    if (EventKey == NAME_None)
    {
        return nullptr;
    }

    // 1. Check config overrides first
    if (const TSoftObjectPtr<UNiagaraSystem>* OverridePtr = Config.VfxOverrides.Find(EventKey))
    {
        if (OverridePtr && !OverridePtr->IsNull())
        {
            return OverridePtr->LoadSynchronous();
        }
    }

    // 2. Check global map
    if (DataAsset)
    {
        if (const TSoftObjectPtr<UNiagaraSystem>* GlobalPtr = DataAsset->GlobalVfxMap.Find(EventKey))
        {
            if (GlobalPtr && !GlobalPtr->IsNull())
            {
                return GlobalPtr->LoadSynchronous();
            }
        }
    }

    return nullptr;
}

UMaterialInterface* UParcelAVLibrary::ResolveDecal(const FParcelConfig& Config, UDA_ParcelData* DataAsset, FName EventKey)
{
    if (EventKey == NAME_None)
    {
        return nullptr;
    }

    // 1. Check config overrides first
    if (const TSoftObjectPtr<UMaterialInterface>* OverridePtr = Config.DecalOverrides.Find(EventKey))
    {
        if (OverridePtr && !OverridePtr->IsNull())
        {
            return OverridePtr->LoadSynchronous();
        }
    }

    // 2. Check global map
    if (DataAsset)
    {
        if (const TSoftObjectPtr<UMaterialInterface>* GlobalPtr = DataAsset->GlobalDecalMap.Find(EventKey))
        {
            if (GlobalPtr && !GlobalPtr->IsNull())
            {
                return GlobalPtr->LoadSynchronous();
            }
        }
    }

    return nullptr;
}

float UParcelAVLibrary::GetSpecialProperty(const FParcelConfig& Config, const FString& PropertyName, float DefaultValue)
{
    if (const float* Value = Config.SpecialProperties.Find(PropertyName))
    {
        return *Value;
    }
    return DefaultValue;
}

