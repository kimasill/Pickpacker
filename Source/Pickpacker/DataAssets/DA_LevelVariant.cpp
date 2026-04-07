// Fill out your copyright notice in the Description page of Project Settings.

#include "DA_LevelVariant.h"
#include "DataAssets/DA_StationData.h"
#include "DataAssets/DA_ParcelData.h"
#include "DataAssets/DA_LabelRuleData.h"

UDA_LevelVariant::UDA_LevelVariant()
{
	LevelName = TEXT("Default Level");
	LevelDescription = TEXT("Default level description");
	StationData = nullptr;
	ParcelData = nullptr;
	LabelRuleData = nullptr;
}

bool UDA_LevelVariant::GetAnchorGroupByTag(const FGameplayTag& GroupTag, FAnchorGroup& OutGroup) const
{
	for (const FAnchorGroup& Group : AnchorGroups)
	{
		if (Group.GroupTag == GroupTag)
		{
			OutGroup = Group;
			return true;
		}
	}
	return false;
}

bool UDA_LevelVariant::GetReplacementRuleBySourceTag(const FGameplayTag& SourceTag, FAnchorReplacementRule& OutRule) const
{
	for (const FAnchorReplacementRule& Rule : ReplacementRules)
	{
		if (Rule.SourceTag == SourceTag)
		{
			OutRule = Rule;
			return true;
		}
	}
	return false;
}

float UDA_LevelVariant::GetGlobalParameter(const FString& ParameterName, float DefaultValue) const
{
	if (const float* Value = GlobalParameters.Find(ParameterName))
	{
		return *Value;
	}
	return DefaultValue;
}
