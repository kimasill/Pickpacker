// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DataAssets/DA_ItemData.h"
#include "DA_ParcelData.generated.h"

class UStaticMesh;
class USoundBase;
class UNiagaraSystem;
class UMaterialInterface;
class AParcelActor;

/**
 * 포장에 포함될 콘텐츠 (클래스 + 수량)
 */
USTRUCT(BlueprintType)
struct PICKPACKER_API FParcelPackageContent
{
	GENERATED_BODY()

	/** 포함될 Parcel RowName (이 값이 우선 사용됨) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Package", meta = (GetOptions = "GetParcelRowOptions"))
	FName ParcelRowName = NAME_None;

	/** (레거시) 직접 클래스 지정 - RowName이 없을 때만 사용 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Package")
	TSubclassOf<AParcelActor> ParcelClass;

	/** 스폰 수량 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Package", meta = (ClampMin = "1"))
	int32 Count = 1;
};

/**
 * 포장 레시피: 특정 태그를 가진 언팩 파슬을 Min~Max 수량 범위로 모아 포장된 번들 생성
 */
USTRUCT(BlueprintType)
struct PICKPACKER_API FParcelPackageRecipe
{
	GENERATED_BODY()

	/** 레시피 이름 (에디터에서 식별용) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Package")
	FString RecipeName = TEXT("Default Package Recipe");

	/** 이 태그들 중 하나를 가진 언팩 파슬이 Min~Max 수량 범위로 모이면 포장됨 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Package")
	TArray<FGameplayTag> TargetParcelTags;

	/** RowName 목록 중 하나를 기준으로 포장 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Package", meta = (GetOptions = "GetParcelRowOptions"))
	TArray<FName> TargetParcelRowNames;

	/** 최소 수량 (unit 합산, 이 값 이상이어야 포장 가능) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Package", meta = (ClampMin = "1"))
	int32 MinRequiredCount = 1;

	/** 최소 포함 개수 (내용물 종류 수 아님, 물건 개수. 최소 이 개수만큼의 물건이 있어야 포장 가능) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Package", meta = (ClampMin = "1"))
	int32 MinContentCount = 1;

	/** 최대 수량 (unit 합산, 이 값 이하여야 해당 박스로 포장됨) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Package", meta = (ClampMin = "1"))
	int32 MaxRequiredCount = 1;

	/** [레거시] 기존 에셋 호환용 - RequiredCount>0이면 Min/Max 마이그레이션에 사용됨 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Package", meta = (ClampMin = "0"))
	int32 RequiredCount = 0;

	/** 포장 시 소모되는 크레딧 (0이면 미소모) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Package|Economy", meta = (ClampMin = "0"))
	int32 CreditCost = 0;

	/** 포장 결과물로 사용할 메쉬(랜덤 선택) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Package")
	TArray<TSoftObjectPtr<UStaticMesh>> PackagedMeshes;

	/** 포장 상태에서 사용할 그립 타입 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Package|Grip")
	EGripType GripType = EGripType::None;

	/** 인벤토리에 넣기 가능 여부 (true면 PackedParcelActor도 인벤토리 수집/보관 가능) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Package|Inventory")
	bool bCanBePutInInventory = false;

	/** 포장 결과물로 사용할 클래스 (기본 ParcelActor) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Package")
	TSubclassOf<AParcelActor> PackagedClass;
};

/**
 * Parcel Configuration - Defines parcel properties and behavior
 */
USTRUCT(BlueprintType)
struct PICKPACKER_API FParcelConfig
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

	/** 포장 수량 계산에 사용되는 공간 차지 단위 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config|Packaging", meta = (ClampMin = "1"))
	int32 PackagingSpaceUnits = 1;

	/** Classification tag (ex: Parcel-Classification.Standard) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config|Tags")
	FGameplayTag ClassificationTag;

	/** Unpackaged mesh (what players see when it is open) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config|Appearance")
	TSoftObjectPtr<UStaticMesh> UnpackagedMeshAsset;

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
		PackagingSpaceUnits = 1;
	}
};

/**
 * Parcel Data Asset - Contains all parcel configurations for a level
 */
UCLASS(BlueprintType)
class PICKPACKER_API UDA_ParcelData : public UDataAsset
{
	GENERATED_BODY()

public:
	UDA_ParcelData();

	virtual void PostLoad() override;

	/** All parcel configurations */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcels")
	TArray<FParcelConfig> ParcelConfigs;

	/** Packaging recipes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcels|Packaging")
	TArray<FParcelPackageRecipe> PackageRecipes;

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

	/** Parcel row 옵션 제공 (에디터 드롭다운용) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel Data")
	TArray<FName> GetParcelRowOptions() const;

	/**
	 * Get global parameter value
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel Data")
	float GetGlobalParameter(const FString& ParameterName, float DefaultValue = 0.0f) const;
};
