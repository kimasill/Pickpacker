// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DA_LabelRuleData.generated.h"

/**
 * Label Rule Type Enumeration
 */
UENUM(BlueprintType)
enum class ELabelRuleType : uint8
{
	ExactMatch		UMETA(DisplayName = "Exact Match"),
	PartialMatch	UMETA(DisplayName = "Partial Match"),
	RangeMatch		UMETA(DisplayName = "Range Match"),
	CustomRule		UMETA(DisplayName = "Custom Rule"),
	Unknown			UMETA(DisplayName = "Unknown")
};

/**
 * Label Rule Configuration - Defines how labels should be validated
 */
USTRUCT(BlueprintType)
struct BLASTER_API FLabelRule
{
	GENERATED_BODY()

	/** Rule name */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Label Rule")
	FString RuleName = TEXT("Default Rule");

	/** Rule type */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Label Rule")
	ELabelRuleType RuleType = ELabelRuleType::Unknown;

	/** Gameplay tag for this rule */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Label Rule")
	FGameplayTag RuleTag;

	/** Required input tags */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Label Rule")
	FGameplayTagContainer RequiredInputTags;

	/** Expected output tags */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Label Rule")
	FGameplayTagContainer ExpectedOutputTags;

	/** Error tolerance (0.0 = perfect required, 1.0 = any result accepted) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Label Rule")
	float ErrorTolerance = 0.1f;

	/** Suspicion points for incorrect labeling */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Label Rule")
	float SuspicionPoints = 10.0f;

	/** Processing time multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Label Rule")
	float ProcessingTimeMultiplier = 1.0f;

	/** Custom validation parameters */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Label Rule")
	TMap<FString, float> CustomParameters;

	FLabelRule()
	{
		RuleName = TEXT("Default Rule");
		RuleType = ELabelRuleType::Unknown;
		ErrorTolerance = 0.1f;
		SuspicionPoints = 10.0f;
		ProcessingTimeMultiplier = 1.0f;
	}
};

/**
 * Label Rule Data Asset - Contains all label validation rules for a level
 */
UCLASS(BlueprintType)
class BLASTER_API UDA_LabelRuleData : public UDataAsset
{
	GENERATED_BODY()

public:
	UDA_LabelRuleData();

	/** All label rules */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Label Rules")
	TArray<FLabelRule> LabelRules;

	/** Global label parameters */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Global Parameters")
	TMap<FString, float> GlobalParameters;

	/**
	 * Get label rule by tag
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Label Rule Data")
	bool GetLabelRuleByTag(const FGameplayTag& RuleTag, FLabelRule& OutRule) const;

	/**
	 * Get all label rules for specific input tags
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Label Rule Data")
	TArray<FLabelRule> GetLabelRulesForInput(const FGameplayTagContainer& InputTags) const;

	/**
	 * Validate label against rules
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Label Rule Data")
	bool ValidateLabel(const FGameplayTagContainer& InputTags, const FGameplayTagContainer& OutputTags, const FGameplayTag& RuleTag) const;

	/**
	 * Get global parameter value
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Label Rule Data")
	float GetGlobalParameter(const FString& ParameterName, float DefaultValue = 0.0f) const;
};
