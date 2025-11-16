// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DA_LevelVariant.generated.h"

class UDA_StationData;
class UDA_ParcelData;
class UDA_LabelRuleData;

/**
 * Anchor Group Definition - Groups anchors with similar functionality
 */
USTRUCT(BlueprintType)
struct BLASTER_API FAnchorGroup
{
	GENERATED_BODY()

	/** Group name for identification */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anchor Group")
	FString GroupName = TEXT("Default");

	/** Gameplay tag that identifies this group */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anchor Group")
	FGameplayTag GroupTag;

	/** Weight for random selection (higher = more likely) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anchor Group")
	float SelectionWeight = 1.0f;

	/** Maximum number of anchors from this group to activate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anchor Group")
	int32 MaxActiveCount = -1; // -1 = no limit

	/** Minimum number of anchors from this group to activate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anchor Group")
	int32 MinActiveCount = 0;

	/** Tags that exclude this group when active */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anchor Group")
	FGameplayTagContainer ExcludedTags;

	FAnchorGroup()
	{
		GroupName = TEXT("Default");
		SelectionWeight = 1.0f;
		MaxActiveCount = -1;
		MinActiveCount = 0;
	}
};

/**
 * Anchor Replacement Rule - Defines how to replace anchors
 */
USTRUCT(BlueprintType)
struct BLASTER_API FAnchorReplacementRule
{
	GENERATED_BODY()

	/** Source anchor tag */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Replacement")
	FGameplayTag SourceTag;

	/** Replacement anchor tag */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Replacement")
	FGameplayTag ReplacementTag;

	/** Probability of replacement (0.0 - 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Replacement")
	float ReplacementProbability = 0.5f;

	/** Maximum number of replacements */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Replacement")
	int32 MaxReplacements = -1; // -1 = no limit

	FAnchorReplacementRule()
	{
		ReplacementProbability = 0.5f;
		MaxReplacements = -1;
	}
};

/**
 * Level Variant Data Asset - Defines anchor groups and randomization rules for a level
 */
UCLASS(BlueprintType)
class BLASTER_API UDA_LevelVariant : public UDataAsset
{
	GENERATED_BODY()

public:
	UDA_LevelVariant();

	/** Level name/identifier */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Info")
	FString LevelName = TEXT("Default Level");

	/** Level description */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Info")
	FString LevelDescription = TEXT("Default level description");

	/** Anchor groups for this level */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anchor Groups")
	TArray<FAnchorGroup> AnchorGroups;

	/** Replacement rules for anchors */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Replacement Rules")
	TArray<FAnchorReplacementRule> ReplacementRules;

	/** Global parameters that can be adjusted */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameters")
	TMap<FString, float> GlobalParameters;

	/** Station data for this level */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station Data")
	UDA_StationData* StationData = nullptr;

	/** Parcel data for this level */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Data")
	UDA_ParcelData* ParcelData = nullptr;

	/** Label rule data for this level */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Label Rules")
	UDA_LabelRuleData* LabelRuleData = nullptr;

	/**
	 * Get anchor group by tag
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Level Variant")
	bool GetAnchorGroupByTag(const FGameplayTag& GroupTag, FAnchorGroup& OutGroup) const;

	/**
	 * Get replacement rule by source tag
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Level Variant")
	bool GetReplacementRuleBySourceTag(const FGameplayTag& SourceTag, FAnchorReplacementRule& OutRule) const;

	/**
	 * Get global parameter value
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Level Variant")
	float GetGlobalParameter(const FString& ParameterName, float DefaultValue = 0.0f) const;
};
