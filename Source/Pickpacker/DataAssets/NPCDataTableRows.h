#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "PickpackerTypes/CoreLoopTypes.h"
#include "NPCDataTableRows.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct PICKPACKER_API FNPCConfigTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	FName NPCId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	ENPCDisposition DefaultDisposition = ENPCDisposition::Neutral;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	ENPCRole DefaultRole = ENPCRole::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	EZoneDifficulty HomeZone = EZoneDifficulty::Low;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Modules")
	bool bEnableDialogue = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Modules")
	bool bEnableCombat = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Modules")
	bool bEnableLootTrade = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Narrative")
	FGameplayTag DisappearFlag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Narrative")
	FGameplayTag HostileFlag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Visuals")
	TSoftObjectPtr<USkeletalMesh> MeshOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Visuals")
	TSoftClassPtr<UAnimInstance> AnimClassOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|AI")
	TSoftObjectPtr<UBehaviorTree> CombatBehaviorTree;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|AI")
	TSoftObjectPtr<UBlackboardData> CombatBlackboard;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|AI")
	TSoftObjectPtr<UBehaviorTree> AmbientBehaviorTree;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|AI")
	TSoftObjectPtr<UBlackboardData> AmbientBlackboard;
};

USTRUCT(BlueprintType)
struct PICKPACKER_API FNPCDialogueScriptRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName NPCId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 NodeIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText SpeakerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText DialogueText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag NodeRequiredWorldFlag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 NodeAutoNextNodeIndex = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 ChoiceIndex = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText ChoiceText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag ChoiceRequiredWorldFlag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag ChoiceBlockingWorldFlag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 ChoiceNextNodeIndex = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 OutcomeOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 RequirementOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EDialogueOutcomeType ChoiceOutcomeType = EDialogueOutcomeType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag ChoiceOutcomeWorldFlag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 ChoiceOutcomeIntValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	float ChoiceOutcomeFloatValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag ChoiceOutcomeTagPayload;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FString ChoiceOutcomeStringPayload;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EDialogueChoiceRequirementType ChoiceRequirementType = EDialogueChoiceRequirementType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag ChoiceRequirementTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	float ChoiceRequirementThresholdValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EDialogueChoiceIndicatorType ChoiceRequirementIndicatorType = EDialogueChoiceIndicatorType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText ChoiceRequirementIndicatorText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FSoftObjectPath ChoiceRequirementIndicatorIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bChoiceRequirementShowIndicatorWhenMet = true;
};

USTRUCT(BlueprintType)
struct PICKPACKER_API FDialogueRequirementIndicatorIconRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Specific requirement tag to match. Leave empty for a generic type-level fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag RequirementTag;

	/** Indicator type to match. Leave as None to act as a tag-only fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EDialogueChoiceIndicatorType IndicatorType = EDialogueChoiceIndicatorType::None;

	/** Icon used when this row matches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TSoftObjectPtr<UTexture2D> Icon;
};
