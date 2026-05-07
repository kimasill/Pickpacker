// DA_NPCData.cpp

#include "DA_NPCData.h"

#include "DataAssets/NPCAnimationSet.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Csv/CsvParser.h"

namespace
{
	struct FCsvRowData
	{
		TMap<FString, FString> Values;
	};

	struct FDialogueChoiceBuildEntry
	{
		int32 LogicalChoiceIndex = INDEX_NONE;
		FDialogueChoice Choice;
		TArray<TPair<int32, FDialogueOutcome>> Outcomes;
		TArray<TPair<int32, FDialogueChoiceRequirement>> Requirements;
	};

	struct FDialogueNodeBuildEntry
	{
		int32 LogicalNodeIndex = INDEX_NONE;
		FDialogueNode Node;
		TMap<int32, FDialogueChoiceBuildEntry> ChoicesByIndex;
	};

	template <typename TRowType>
	const TRowType* FindRowSafe(const UDataTable* DataTable, const FName RowName, const FString& Context)
	{
		return DataTable ? DataTable->FindRow<TRowType>(RowName, Context, false) : nullptr;
	}

	int32 RemapNodeIndex(const TMap<int32, int32>& LogicalToCompiledIndex, int32 LogicalIndex)
	{
		if (LogicalIndex < 0)
		{
			return -1;
		}

		if (const int32* CompiledIndex = LogicalToCompiledIndex.Find(LogicalIndex))
		{
			return *CompiledIndex;
		}

		return -1;
	}

	FString NormalizeCsvHeader(const FString& Header)
	{
		FString Result = Header;
		Result.TrimStartAndEndInline();
		Result.ReplaceInline(TEXT("\ufeff"), TEXT(""));
		return Result;
	}

	FString NormalizeCsvValue(const FString& Value)
	{
		FString Result = Value;
		Result.TrimStartAndEndInline();
		return Result;
	}

	FString ResolveCsvPath(const FString& InPath)
	{
		FString ResolvedPath = NormalizeCsvValue(InPath);
		if (ResolvedPath.IsEmpty())
		{
			return FString();
		}

		if (FPaths::IsRelative(ResolvedPath))
		{
			ResolvedPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), ResolvedPath);
		}

		return ResolvedPath;
	}

	bool LoadCsvRows(const FString& InPath, TArray<FCsvRowData>& OutRows)
	{
		OutRows.Reset();

		const FString ResolvedPath = ResolveCsvPath(InPath);
		if (ResolvedPath.IsEmpty() || !FPaths::FileExists(ResolvedPath))
		{
			return false;
		}

		FString CsvContents;
		if (!FFileHelper::LoadFileToString(CsvContents, *ResolvedPath))
		{
			return false;
		}

		FCsvParser Parser(CsvContents);
		const FCsvParser::FRows& ParsedRows = Parser.GetRows();
		if (ParsedRows.Num() <= 1)
		{
			return false;
		}

		TArray<FString> Headers;
		for (const FString& HeaderCell : ParsedRows[0])
		{
			Headers.Add(NormalizeCsvHeader(HeaderCell));
		}

		for (int32 RowIndex = 1; RowIndex < ParsedRows.Num(); ++RowIndex)
		{
			const TArray<const TCHAR*>& ParsedRow = ParsedRows[RowIndex];
			if (ParsedRow.Num() == 0)
			{
				continue;
			}

			bool bHasAnyValue = false;
			FCsvRowData RowData;

			for (int32 ColumnIndex = 0; ColumnIndex < Headers.Num(); ++ColumnIndex)
			{
				const FString Header = Headers[ColumnIndex];
				if (Header.IsEmpty())
				{
					continue;
				}

				const FString CellValue = ColumnIndex < ParsedRow.Num()
					? NormalizeCsvValue(ParsedRow[ColumnIndex])
					: FString();
				if (!CellValue.IsEmpty())
				{
					bHasAnyValue = true;
				}
				RowData.Values.Add(Header, CellValue);
			}

			if (bHasAnyValue)
			{
				OutRows.Add(MoveTemp(RowData));
			}
		}

		return OutRows.Num() > 0;
	}

	FString GetCsvCell(const FCsvRowData& RowData, const TCHAR* ColumnName)
	{
		if (const FString* Value = RowData.Values.Find(ColumnName))
		{
			return *Value;
		}

		return FString();
	}

	bool ParseBoolValue(const FString& Value, bool DefaultValue = false)
	{
		const FString Lower = NormalizeCsvValue(Value).ToLower();
		if (Lower.IsEmpty())
		{
			return DefaultValue;
		}

		if (Lower == TEXT("true") || Lower == TEXT("1") || Lower == TEXT("yes"))
		{
			return true;
		}

		if (Lower == TEXT("false") || Lower == TEXT("0") || Lower == TEXT("no"))
		{
			return false;
		}

		return DefaultValue;
	}

	int32 ParseIntValue(const FString& Value, int32 DefaultValue = 0)
	{
		const FString Normalized = NormalizeCsvValue(Value);
		return Normalized.IsEmpty() ? DefaultValue : FCString::Atoi(*Normalized);
	}

	float ParseFloatValue(const FString& Value, float DefaultValue = 0.0f)
	{
		const FString Normalized = NormalizeCsvValue(Value);
		return Normalized.IsEmpty() ? DefaultValue : FCString::Atof(*Normalized);
	}

	TArray<TSoftObjectPtr<UMaterialInterface>> ParseMaterialOverrideList(const FString& Value)
	{
		TArray<TSoftObjectPtr<UMaterialInterface>> Result;
		const FString Normalized = NormalizeCsvValue(Value);
		if (Normalized.IsEmpty())
		{
			return Result;
		}

		TArray<FString> Entries;
		Normalized.ParseIntoArray(Entries, TEXT(";"), true);
		for (FString& Entry : Entries)
		{
			Entry = NormalizeCsvValue(Entry);
			if (!Entry.IsEmpty())
			{
				Result.Add(TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(Entry)));
			}
		}

		return Result;
	}

	template <typename TEnum>
	bool ParseEnumValue(const FString& Value, TEnum& OutValue)
	{
		const FString Normalized = NormalizeCsvValue(Value);
		if (Normalized.IsEmpty())
		{
			return false;
		}

		if (UEnum* EnumType = StaticEnum<TEnum>())
		{
			const FString Wanted = Normalized.ToLower();
			for (int32 Index = 0; Index < EnumType->NumEnums(); ++Index)
			{
				if (EnumType->HasMetaData(TEXT("Hidden"), Index))
				{
					continue;
				}

				const FString FullName = EnumType->GetNameStringByIndex(Index);
				const FString Suffix = FullName.Contains(TEXT("::")) ? FullName.RightChop(FullName.Find(TEXT("::")) + 2) : FullName;
				if (FullName.ToLower() == Wanted || Suffix.ToLower() == Wanted)
				{
					OutValue = static_cast<TEnum>(EnumType->GetValueByIndex(Index));
					return true;
				}
			}
		}

		return false;
	}

	FGameplayTag ParseGameplayTagValue(const FString& Value)
	{
		const FString Normalized = NormalizeCsvValue(Value);
		if (Normalized.IsEmpty())
		{
			return FGameplayTag();
		}

		return FGameplayTag::RequestGameplayTag(FName(*Normalized), false);
	}

	bool TryBuildConfigRowFromCsv(const FString& CsvPath, const FName RowName, FNPCConfigTableRow& OutRow)
	{
		TArray<FCsvRowData> CsvRows;
		if (!LoadCsvRows(CsvPath, CsvRows))
		{
			return false;
		}

		for (const FCsvRowData& CsvRow : CsvRows)
		{
			const FString NameValue = GetCsvCell(CsvRow, TEXT("Name"));
			const FString NPCIdValue = GetCsvCell(CsvRow, TEXT("NPCId"));
			if (!RowName.IsNone() && NameValue != RowName.ToString() && NPCIdValue != RowName.ToString())
			{
				continue;
			}

			OutRow = FNPCConfigTableRow();
			OutRow.NPCId = FName(*NormalizeCsvValue(NPCIdValue));
			OutRow.DisplayName = FText::FromString(GetCsvCell(CsvRow, TEXT("DisplayName")));
			ParseEnumValue(GetCsvCell(CsvRow, TEXT("DefaultDisposition")), OutRow.DefaultDisposition);
			ParseEnumValue(GetCsvCell(CsvRow, TEXT("DefaultRole")), OutRow.DefaultRole);
			ParseEnumValue(GetCsvCell(CsvRow, TEXT("HomeZone")), OutRow.HomeZone);
			OutRow.bEnableDialogue = ParseBoolValue(GetCsvCell(CsvRow, TEXT("bEnableDialogue")), OutRow.bEnableDialogue);
			OutRow.bEnableCombat = ParseBoolValue(GetCsvCell(CsvRow, TEXT("bEnableCombat")), OutRow.bEnableCombat);
			OutRow.bEnableLootTrade = ParseBoolValue(GetCsvCell(CsvRow, TEXT("bEnableLootTrade")), OutRow.bEnableLootTrade);
			OutRow.CombatSettings.MaxHealth = ParseFloatValue(GetCsvCell(CsvRow, TEXT("MaxHealth")), OutRow.CombatSettings.MaxHealth);
			OutRow.CombatSettings.AttackRange = ParseFloatValue(GetCsvCell(CsvRow, TEXT("AttackRange")), OutRow.CombatSettings.AttackRange);
			OutRow.CombatSettings.AttackDamage = ParseFloatValue(GetCsvCell(CsvRow, TEXT("AttackDamage")), OutRow.CombatSettings.AttackDamage);
			OutRow.CombatSettings.AttackCooldown = ParseFloatValue(GetCsvCell(CsvRow, TEXT("AttackCooldown")), OutRow.CombatSettings.AttackCooldown);
			ParseEnumValue(GetCsvCell(CsvRow, TEXT("EngagementPolicy")), OutRow.CombatSettings.EngagementPolicy);
			OutRow.CombatSettings.bRetaliateWhenDamaged = ParseBoolValue(GetCsvCell(CsvRow, TEXT("bRetaliateWhenDamaged")), OutRow.CombatSettings.bRetaliateWhenDamaged);
			OutRow.CombatSettings.bBecomeAggressiveWhenDamaged = ParseBoolValue(GetCsvCell(CsvRow, TEXT("bBecomeAggressiveWhenDamaged")), OutRow.CombatSettings.bBecomeAggressiveWhenDamaged);
			OutRow.CombatSettings.bAutoClearTarget = ParseBoolValue(GetCsvCell(CsvRow, TEXT("bAutoClearTarget")), OutRow.CombatSettings.bAutoClearTarget);
			OutRow.CombatSettings.LoseSightAggroGraceTime = ParseFloatValue(GetCsvCell(CsvRow, TEXT("LoseSightAggroGraceTime")), OutRow.CombatSettings.LoseSightAggroGraceTime);
			OutRow.CombatSettings.MaxChaseDistanceFromHome = ParseFloatValue(GetCsvCell(CsvRow, TEXT("MaxChaseDistanceFromHome")), OutRow.CombatSettings.MaxChaseDistanceFromHome);
			OutRow.CombatSettings.MaxTargetDistance = ParseFloatValue(GetCsvCell(CsvRow, TEXT("MaxTargetDistance")), OutRow.CombatSettings.MaxTargetDistance);
			OutRow.CombatSettings.ChaseSpeed = ParseFloatValue(GetCsvCell(CsvRow, TEXT("ChaseSpeed")), OutRow.CombatSettings.ChaseSpeed);
			OutRow.CombatSettings.PatrolSpeed = ParseFloatValue(GetCsvCell(CsvRow, TEXT("PatrolSpeed")), OutRow.CombatSettings.PatrolSpeed);
			OutRow.DisappearFlag = ParseGameplayTagValue(GetCsvCell(CsvRow, TEXT("DisappearFlag")));
			OutRow.HostileFlag = ParseGameplayTagValue(GetCsvCell(CsvRow, TEXT("HostileFlag")));
			OutRow.MeshOverride = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(GetCsvCell(CsvRow, TEXT("MeshOverride"))));
			OutRow.MaterialOverrides = ParseMaterialOverrideList(GetCsvCell(CsvRow, TEXT("MaterialOverrides")));
			if (OutRow.MaterialOverrides.Num() == 0)
			{
				OutRow.MaterialOverrides = ParseMaterialOverrideList(GetCsvCell(CsvRow, TEXT("MaterialOverride")));
			}
			OutRow.AnimClassOverride = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(GetCsvCell(CsvRow, TEXT("AnimClassOverride"))));
			OutRow.CombatBehaviorTree = TSoftObjectPtr<UBehaviorTree>(FSoftObjectPath(GetCsvCell(CsvRow, TEXT("CombatBehaviorTree"))));
			OutRow.CombatBlackboard = TSoftObjectPtr<UBlackboardData>(FSoftObjectPath(GetCsvCell(CsvRow, TEXT("CombatBlackboard"))));
			OutRow.AmbientBehaviorTree = TSoftObjectPtr<UBehaviorTree>(FSoftObjectPath(GetCsvCell(CsvRow, TEXT("AmbientBehaviorTree"))));
			OutRow.AmbientBlackboard = TSoftObjectPtr<UBlackboardData>(FSoftObjectPath(GetCsvCell(CsvRow, TEXT("AmbientBlackboard"))));
			return true;
		}

		return false;
	}

	bool TryGetDialogueRowsFromCsv(const FString& CsvPath, const FName NPCId, TArray<FNPCDialogueScriptRow>& OutRows)
	{
		OutRows.Reset();

		TArray<FCsvRowData> CsvRows;
		if (!LoadCsvRows(CsvPath, CsvRows))
		{
			return false;
		}

		for (const FCsvRowData& CsvRow : CsvRows)
		{
			if (FName(*NormalizeCsvValue(GetCsvCell(CsvRow, TEXT("NPCId")))) != NPCId)
			{
				continue;
			}

			FNPCDialogueScriptRow Row;
			Row.NPCId = NPCId;
			Row.NodeIndex = ParseIntValue(GetCsvCell(CsvRow, TEXT("NodeIndex")), 0);
			Row.SpeakerName = FText::FromString(GetCsvCell(CsvRow, TEXT("SpeakerName")));
			Row.DialogueText = FText::FromString(GetCsvCell(CsvRow, TEXT("DialogueText")));
			Row.NodeRequiredWorldFlag = ParseGameplayTagValue(GetCsvCell(CsvRow, TEXT("NodeRequiredWorldFlag")));
			Row.NodeAutoNextNodeIndex = ParseIntValue(GetCsvCell(CsvRow, TEXT("NodeAutoNextNodeIndex")), -1);
			Row.ChoiceIndex = ParseIntValue(GetCsvCell(CsvRow, TEXT("ChoiceIndex")), -1);
			Row.ChoiceText = FText::FromString(GetCsvCell(CsvRow, TEXT("ChoiceText")));
			Row.ChoiceRequiredWorldFlag = ParseGameplayTagValue(GetCsvCell(CsvRow, TEXT("ChoiceRequiredWorldFlag")));
			Row.ChoiceBlockingWorldFlag = ParseGameplayTagValue(GetCsvCell(CsvRow, TEXT("ChoiceBlockingWorldFlag")));
			Row.ChoiceNextNodeIndex = ParseIntValue(GetCsvCell(CsvRow, TEXT("ChoiceNextNodeIndex")), -1);
			Row.OutcomeOrder = ParseIntValue(GetCsvCell(CsvRow, TEXT("OutcomeOrder")), 0);
			Row.RequirementOrder = ParseIntValue(GetCsvCell(CsvRow, TEXT("RequirementOrder")), 0);
			ParseEnumValue(GetCsvCell(CsvRow, TEXT("ChoiceOutcomeType")), Row.ChoiceOutcomeType);
			Row.ChoiceOutcomeWorldFlag = ParseGameplayTagValue(GetCsvCell(CsvRow, TEXT("ChoiceOutcomeWorldFlag")));
			Row.ChoiceOutcomeIntValue = ParseIntValue(GetCsvCell(CsvRow, TEXT("ChoiceOutcomeIntValue")), 0);
			Row.ChoiceOutcomeFloatValue = FCString::Atof(*GetCsvCell(CsvRow, TEXT("ChoiceOutcomeFloatValue")));
			Row.ChoiceOutcomeTagPayload = ParseGameplayTagValue(GetCsvCell(CsvRow, TEXT("ChoiceOutcomeTagPayload")));
			Row.ChoiceOutcomeStringPayload = GetCsvCell(CsvRow, TEXT("ChoiceOutcomeStringPayload"));
			ParseEnumValue(GetCsvCell(CsvRow, TEXT("ChoiceRequirementType")), Row.ChoiceRequirementType);
			Row.ChoiceRequirementTag = ParseGameplayTagValue(GetCsvCell(CsvRow, TEXT("ChoiceRequirementTag")));
			Row.ChoiceRequirementThresholdValue = FCString::Atof(*GetCsvCell(CsvRow, TEXT("ChoiceRequirementThresholdValue")));
			ParseEnumValue(GetCsvCell(CsvRow, TEXT("ChoiceRequirementIndicatorType")), Row.ChoiceRequirementIndicatorType);
			Row.ChoiceRequirementIndicatorText = FText::FromString(GetCsvCell(CsvRow, TEXT("ChoiceRequirementIndicatorText")));
			Row.ChoiceRequirementIndicatorIcon = FSoftObjectPath(GetCsvCell(CsvRow, TEXT("ChoiceRequirementIndicatorIcon")));
			Row.bChoiceRequirementShowIndicatorWhenMet = ParseBoolValue(GetCsvCell(CsvRow, TEXT("bChoiceRequirementShowIndicatorWhenMet")), true);
			OutRows.Add(MoveTemp(Row));
		}

		return OutRows.Num() > 0;
	}

	UTexture2D* LoadTextureFromSoftPath(const FSoftObjectPath& ObjectPath)
	{
		return ObjectPath.IsValid() ? Cast<UTexture2D>(ObjectPath.TryLoad()) : nullptr;
	}

	UTexture2D* ResolveDialogueRequirementIndicatorIconFromTable(const UDataTable* DataTable, const FDialogueChoiceRequirement& Requirement)
	{
		if (!DataTable || DataTable->GetRowStruct() != FDialogueRequirementIndicatorIconRow::StaticStruct())
		{
			return nullptr;
		}

		TArray<FDialogueRequirementIndicatorIconRow*> Rows;
		DataTable->GetAllRows(TEXT("DialogueRequirementIndicatorIconLookup"), Rows);

		const FDialogueRequirementIndicatorIconRow* BestRow = nullptr;
		int32 BestScore = MIN_int32;

		for (const FDialogueRequirementIndicatorIconRow* Row : Rows)
		{
			if (!Row)
			{
				continue;
			}

			int32 Score = 0;

			if (Row->RequirementTag.IsValid())
			{
				if (!Requirement.Tag.IsValid() || Row->RequirementTag != Requirement.Tag)
				{
					continue;
				}

				Score += 100;
			}

			if (Row->IndicatorType != EDialogueChoiceIndicatorType::None)
			{
				if (Row->IndicatorType != Requirement.IndicatorType)
				{
					continue;
				}

				Score += 10;
			}

			if (!Row->Icon.IsNull())
			{
				Score += 1;
			}

			if (Score <= BestScore)
			{
				continue;
			}

			BestScore = Score;
			BestRow = Row;
		}

		return BestRow ? BestRow->Icon.LoadSynchronous() : nullptr;
	}
}

UDA_NPCData::UDA_NPCData()
{
}

FName UDA_NPCData::GetNPCId() const
{
	FNPCConfigTableRow ConfigRow;
	if (TryGetConfigRow(ConfigRow) && !ConfigRow.NPCId.IsNone())
	{
		return ConfigRow.NPCId;
	}

	if (!ConfigRowName.IsNone())
	{
		return ConfigRowName;
	}

	return Profile.NPCId;
}

FText UDA_NPCData::GetDisplayName() const
{
	FNPCConfigTableRow ConfigRow;
	if (TryGetConfigRow(ConfigRow) && !ConfigRow.DisplayName.IsEmpty())
	{
		return ConfigRow.DisplayName;
	}

	return Profile.DisplayName;
}

bool UDA_NPCData::TryGetConfigRow(FNPCConfigTableRow& OutRow) const
{
	OutRow = FNPCConfigTableRow();

	if (ConfigRowName.IsNone())
	{
		return false;
	}

	if (TryBuildConfigRowFromCsv(ConfigCsvFile.FilePath, ConfigRowName, OutRow))
	{
		UE_LOG(LogTemp, Log, TEXT("[DA_NPCData] Loaded config row '%s' from CSV '%s'."), *ConfigRowName.ToString(), *ConfigCsvFile.FilePath);
		return true;
	}

	if (!ConfigDataTable.IsNull())
	{
		const UDataTable* ConfigTable = ConfigDataTable.LoadSynchronous();
		if (ConfigTable)
		{
			if (const FNPCConfigTableRow* Row = FindRowSafe<FNPCConfigTableRow>(ConfigTable, ConfigRowName, TEXT("NPCConfigLookup")))
			{
				OutRow = *Row;
				return true;
			}
		}
	}

	return false;
}

void UDA_NPCData::BuildResolvedProfile(FNPCProfile& OutProfile) const
{
	OutProfile = Profile;

	FNPCConfigTableRow ConfigRow;
	if (!TryGetConfigRow(ConfigRow))
	{
		return;
	}

	if (!ConfigRow.NPCId.IsNone())
	{
		OutProfile.NPCId = ConfigRow.NPCId;
	}
	if (!ConfigRow.DisplayName.IsEmpty())
	{
		OutProfile.DisplayName = ConfigRow.DisplayName;
	}
	if (OutProfile.NPCId.IsNone() && !ConfigRowName.IsNone())
	{
		OutProfile.NPCId = ConfigRowName;
	}

	OutProfile.DefaultDisposition = ConfigRow.DefaultDisposition;
	OutProfile.DefaultRole = ConfigRow.DefaultRole;
	OutProfile.HomeZone = ConfigRow.HomeZone;
	OutProfile.bEnableDialogue = ConfigRow.bEnableDialogue;
	OutProfile.bEnableCombat = ConfigRow.bEnableCombat;
	OutProfile.bEnableLootTrade = ConfigRow.bEnableLootTrade;
	OutProfile.CombatSettings = ConfigRow.CombatSettings;
	OutProfile.DisappearFlag = ConfigRow.DisappearFlag;
	OutProfile.HostileFlag = ConfigRow.HostileFlag;
}

void UDA_NPCData::BuildResolvedDialogueNodes(const FName& EffectiveNPCId, TArray<FDialogueNode>& OutDialogueNodes) const
{
	OutDialogueNodes = Profile.DialogueNodes;

	FName NPCIdForLookup = EffectiveNPCId;
	if (NPCIdForLookup.IsNone())
	{
		NPCIdForLookup = GetNPCId();
	}

	if (NPCIdForLookup.IsNone())
	{
		return;
	}

	TArray<FNPCDialogueScriptRow> CsvRows;
	TMap<int32, FDialogueNodeBuildEntry> NodesByIndex;
	bool bFoundAnyRows = false;
	const UDataTable* DialogueIndicatorTable = DialogueIndicatorIconDataTable.IsNull()
		? nullptr
		: DialogueIndicatorIconDataTable.LoadSynchronous();

	auto AccumulateRow = [&NodesByIndex, &bFoundAnyRows, DialogueIndicatorTable](const FNPCDialogueScriptRow& Row)
	{
		bFoundAnyRows = true;

		FDialogueNodeBuildEntry& NodeEntry = NodesByIndex.FindOrAdd(Row.NodeIndex);
		NodeEntry.LogicalNodeIndex = Row.NodeIndex;
		NodeEntry.Node.SpeakerName = Row.SpeakerName;
		NodeEntry.Node.DialogueText = Row.DialogueText;
		NodeEntry.Node.RequiredWorldFlag = Row.NodeRequiredWorldFlag;
		NodeEntry.Node.AutoNextNodeIndex = Row.NodeAutoNextNodeIndex;

		if (Row.ChoiceIndex < 0)
		{
			return;
		}

		FDialogueChoiceBuildEntry& ChoiceEntry = NodeEntry.ChoicesByIndex.FindOrAdd(Row.ChoiceIndex);
		ChoiceEntry.LogicalChoiceIndex = Row.ChoiceIndex;
		ChoiceEntry.Choice.ChoiceText = Row.ChoiceText;
		ChoiceEntry.Choice.RequiredWorldFlag = Row.ChoiceRequiredWorldFlag;
		ChoiceEntry.Choice.BlockingWorldFlag = Row.ChoiceBlockingWorldFlag;
		ChoiceEntry.Choice.NextNodeIndex = Row.ChoiceNextNodeIndex;

		if (Row.ChoiceOutcomeType != EDialogueOutcomeType::None)
		{
			FDialogueOutcome Outcome;
			Outcome.OutcomeType = Row.ChoiceOutcomeType;
			Outcome.WorldFlag = Row.ChoiceOutcomeWorldFlag;
			Outcome.IntValue = Row.ChoiceOutcomeIntValue;
			Outcome.FloatValue = Row.ChoiceOutcomeFloatValue;
			Outcome.TagPayload = Row.ChoiceOutcomeTagPayload;
			Outcome.StringPayload = Row.ChoiceOutcomeStringPayload;
			ChoiceEntry.Outcomes.Add(TPair<int32, FDialogueOutcome>(Row.OutcomeOrder, Outcome));
		}

		const bool bHasExplicitRequirement = Row.ChoiceRequirementType != EDialogueChoiceRequirementType::None;
		const bool bHasIndicatorOnlyMetadata =
			Row.ChoiceRequirementIndicatorType != EDialogueChoiceIndicatorType::None ||
			!Row.ChoiceRequirementIndicatorText.IsEmpty() ||
			Row.ChoiceRequirementIndicatorIcon.IsValid();

		if (bHasExplicitRequirement || bHasIndicatorOnlyMetadata)
		{
			FDialogueChoiceRequirement Requirement;
			Requirement.RequirementType = Row.ChoiceRequirementType;
			Requirement.Tag = Row.ChoiceRequirementTag;
			Requirement.ThresholdValue = Row.ChoiceRequirementThresholdValue;
			Requirement.IndicatorType = Row.ChoiceRequirementIndicatorType;
			Requirement.IndicatorText = Row.ChoiceRequirementIndicatorText;
			Requirement.IndicatorIcon = LoadTextureFromSoftPath(Row.ChoiceRequirementIndicatorIcon);
			if (!Requirement.IndicatorIcon)
			{
				Requirement.IndicatorIcon = ResolveDialogueRequirementIndicatorIconFromTable(DialogueIndicatorTable, Requirement);
			}
			Requirement.bShowIndicatorWhenMet = Row.bChoiceRequirementShowIndicatorWhenMet;
			ChoiceEntry.Requirements.Add(TPair<int32, FDialogueChoiceRequirement>(Row.RequirementOrder, Requirement));
		}
	};

	if (!DialogueScriptDataTable.IsNull())
	{
		if (const UDataTable* DialogueTable = DialogueScriptDataTable.LoadSynchronous())
		{
			TArray<FNPCDialogueScriptRow*> Rows;
			DialogueTable->GetAllRows(TEXT("NPCDialogueScriptLookup"), Rows);
			for (const FNPCDialogueScriptRow* Row : Rows)
			{
				if (!Row || Row->NPCId != NPCIdForLookup)
				{
					continue;
				}

				AccumulateRow(*Row);
			}
		}
	}
	else if (TryGetDialogueRowsFromCsv(DialogueCsvFile.FilePath, NPCIdForLookup, CsvRows))
	{
		UE_LOG(LogTemp, Log, TEXT("[DA_NPCData] Loaded dialogue rows for NPC '%s' from CSV '%s'."), *NPCIdForLookup.ToString(), *DialogueCsvFile.FilePath);
		for (const FNPCDialogueScriptRow& Row : CsvRows)
		{
			AccumulateRow(Row);
		}
	}

	if (!bFoundAnyRows)
	{
		return;
	}

	TArray<int32> LogicalNodeIndices;
	NodesByIndex.GetKeys(LogicalNodeIndices);
	LogicalNodeIndices.Sort();

	TMap<int32, int32> LogicalToCompiledIndex;
	for (int32 CompiledIndex = 0; CompiledIndex < LogicalNodeIndices.Num(); ++CompiledIndex)
	{
		LogicalToCompiledIndex.Add(LogicalNodeIndices[CompiledIndex], CompiledIndex);
	}

	OutDialogueNodes.Reset();
	for (const int32 LogicalNodeIndex : LogicalNodeIndices)
	{
		FDialogueNodeBuildEntry& NodeEntry = NodesByIndex.FindChecked(LogicalNodeIndex);
		FDialogueNode CompiledNode = NodeEntry.Node;
		CompiledNode.AutoNextNodeIndex = RemapNodeIndex(LogicalToCompiledIndex, CompiledNode.AutoNextNodeIndex);
		CompiledNode.Choices.Reset();

		TArray<int32> LogicalChoiceIndices;
		NodeEntry.ChoicesByIndex.GetKeys(LogicalChoiceIndices);
		LogicalChoiceIndices.Sort();

		for (const int32 LogicalChoiceIndex : LogicalChoiceIndices)
		{
			FDialogueChoiceBuildEntry& ChoiceEntry = NodeEntry.ChoicesByIndex.FindChecked(LogicalChoiceIndex);
			FDialogueChoice CompiledChoice = ChoiceEntry.Choice;
			CompiledChoice.NextNodeIndex = RemapNodeIndex(LogicalToCompiledIndex, CompiledChoice.NextNodeIndex);
			CompiledChoice.Outcomes.Reset();
			CompiledChoice.UnlockRequirements.Reset();

			ChoiceEntry.Outcomes.Sort([](const TPair<int32, FDialogueOutcome>& A, const TPair<int32, FDialogueOutcome>& B)
			{
				return A.Key < B.Key;
			});

			for (const TPair<int32, FDialogueOutcome>& OutcomePair : ChoiceEntry.Outcomes)
			{
				CompiledChoice.Outcomes.Add(OutcomePair.Value);
			}

			ChoiceEntry.Requirements.Sort([](const TPair<int32, FDialogueChoiceRequirement>& A, const TPair<int32, FDialogueChoiceRequirement>& B)
			{
				return A.Key < B.Key;
			});

			for (const TPair<int32, FDialogueChoiceRequirement>& RequirementPair : ChoiceEntry.Requirements)
			{
				CompiledChoice.UnlockRequirements.Add(RequirementPair.Value);
			}

			CompiledNode.Choices.Add(CompiledChoice);
		}

		OutDialogueNodes.Add(CompiledNode);
	}
}

TSoftObjectPtr<USkeletalMesh> UDA_NPCData::GetResolvedMeshOverride() const
{
	FNPCConfigTableRow ConfigRow;
	if (TryGetConfigRow(ConfigRow) && !ConfigRow.MeshOverride.IsNull())
	{
		return ConfigRow.MeshOverride;
	}

	return MeshOverride;
}

TArray<TSoftObjectPtr<UMaterialInterface>> UDA_NPCData::GetResolvedMaterialOverrides() const
{
	FNPCConfigTableRow ConfigRow;
	if (TryGetConfigRow(ConfigRow) && ConfigRow.MaterialOverrides.Num() > 0)
	{
		return ConfigRow.MaterialOverrides;
	}

	return MaterialOverrides;
}

TSoftClassPtr<UAnimInstance> UDA_NPCData::GetResolvedAnimClassOverride() const
{
	FNPCConfigTableRow ConfigRow;
	if (TryGetConfigRow(ConfigRow) && !ConfigRow.AnimClassOverride.IsNull())
	{
		return ConfigRow.AnimClassOverride;
	}

	return AnimClassOverride;
}

TSoftObjectPtr<UNPCAnimationSet> UDA_NPCData::GetResolvedAnimationSet() const
{
	return AnimationSetOverride;
}

TSoftObjectPtr<UBehaviorTree> UDA_NPCData::GetResolvedCombatBehaviorTree() const
{
	FNPCConfigTableRow ConfigRow;
	if (TryGetConfigRow(ConfigRow) && !ConfigRow.CombatBehaviorTree.IsNull())
	{
		return ConfigRow.CombatBehaviorTree;
	}

	return CombatBehaviorTree;
}

TSoftObjectPtr<UBlackboardData> UDA_NPCData::GetResolvedCombatBlackboard() const
{
	FNPCConfigTableRow ConfigRow;
	if (TryGetConfigRow(ConfigRow) && !ConfigRow.CombatBlackboard.IsNull())
	{
		return ConfigRow.CombatBlackboard;
	}

	return CombatBlackboard;
}

TSoftObjectPtr<UBehaviorTree> UDA_NPCData::GetResolvedAmbientBehaviorTree() const
{
	FNPCConfigTableRow ConfigRow;
	if (TryGetConfigRow(ConfigRow) && !ConfigRow.AmbientBehaviorTree.IsNull())
	{
		return ConfigRow.AmbientBehaviorTree;
	}

	return AmbientBehaviorTree;
}

TSoftObjectPtr<UBlackboardData> UDA_NPCData::GetResolvedAmbientBlackboard() const
{
	FNPCConfigTableRow ConfigRow;
	if (TryGetConfigRow(ConfigRow) && !ConfigRow.AmbientBlackboard.IsNull())
	{
		return ConfigRow.AmbientBlackboard;
	}

	return AmbientBlackboard;
}
