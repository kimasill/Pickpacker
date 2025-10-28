// Fill out your copyright notice in the Description page of Project Settings.

#include "DA_LabelRuleData.h"

UDA_LabelRuleData::UDA_LabelRuleData()
{
	// Initialize with default label rules
	FLabelRule StandardRule;
	StandardRule.RuleName = TEXT("Standard Label Rule");
	StandardRule.RuleType = ELabelRuleType::ExactMatch;
	StandardRule.ErrorTolerance = 0.1f;
	StandardRule.SuspicionPoints = 10.0f;
	StandardRule.ProcessingTimeMultiplier = 1.0f;
	LabelRules.Add(StandardRule);

	FLabelRule FragileRule;
	FragileRule.RuleName = TEXT("Fragile Label Rule");
	FragileRule.RuleType = ELabelRuleType::ExactMatch;
	FragileRule.ErrorTolerance = 0.05f;
	FragileRule.SuspicionPoints = 15.0f;
	FragileRule.ProcessingTimeMultiplier = 1.2f;
	LabelRules.Add(FragileRule);

	FLabelRule HeavyRule;
	HeavyRule.RuleName = TEXT("Heavy Label Rule");
	HeavyRule.RuleType = ELabelRuleType::RangeMatch;
	HeavyRule.ErrorTolerance = 0.15f;
	HeavyRule.SuspicionPoints = 8.0f;
	HeavyRule.ProcessingTimeMultiplier = 1.1f;
	LabelRules.Add(HeavyRule);

	FLabelRule ContrabandRule;
	ContrabandRule.RuleName = TEXT("Contraband Label Rule");
	ContrabandRule.RuleType = ELabelRuleType::CustomRule;
	ContrabandRule.ErrorTolerance = 0.0f; // Must be perfect
	ContrabandRule.SuspicionPoints = 50.0f;
	ContrabandRule.ProcessingTimeMultiplier = 1.5f;
	LabelRules.Add(ContrabandRule);
}

bool UDA_LabelRuleData::GetLabelRuleByTag(const FGameplayTag& RuleTag, FLabelRule& OutRule) const
{
	for (const FLabelRule& Rule : LabelRules)
	{
		if (Rule.RuleTag == RuleTag)
		{
			OutRule = Rule;
			return true;
		}
	}
	return false;
}

TArray<FLabelRule> UDA_LabelRuleData::GetLabelRulesForInput(const FGameplayTagContainer& InputTags) const
{
	TArray<FLabelRule> Result;
	for (const FLabelRule& Rule : LabelRules)
	{
		if (Rule.RequiredInputTags.HasAll(InputTags))
		{
			Result.Add(Rule);
		}
	}
	return Result;
}

bool UDA_LabelRuleData::ValidateLabel(const FGameplayTagContainer& InputTags, const FGameplayTagContainer& OutputTags, const FGameplayTag& RuleTag) const
{
	FLabelRule Rule;
	if (!GetLabelRuleByTag(RuleTag, Rule))
	{
		return false;
	}

	// Check if input tags match required tags
	if (!Rule.RequiredInputTags.HasAll(InputTags))
	{
		return false;
	}

	// Validate based on rule type
	switch (Rule.RuleType)
	{
	case ELabelRuleType::ExactMatch:
		return Rule.ExpectedOutputTags.HasAll(OutputTags) && OutputTags.HasAll(Rule.ExpectedOutputTags);

	case ELabelRuleType::PartialMatch:
		return Rule.ExpectedOutputTags.HasAny(OutputTags);

	case ELabelRuleType::RangeMatch:
		// For range match, we'd need custom logic based on CustomParameters
		return true; // Placeholder

	case ELabelRuleType::CustomRule:
		// Custom validation logic would go here
		return true; // Placeholder

	default:
		return false;
	}
}

float UDA_LabelRuleData::GetGlobalParameter(const FString& ParameterName, float DefaultValue) const
{
	if (const float* Value = GlobalParameters.Find(ParameterName))
	{
		return *Value;
	}
	return DefaultValue;
}
