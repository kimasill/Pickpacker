// NPC Data Asset - Defines NPC configurations via data

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "Engine/DataTable.h"
#include "Engine/EngineTypes.h"
#include "PickpackerTypes/CoreLoopTypes.h"
#include "DataAssets/NPCDataTableRows.h"
#include "DA_NPCData.generated.h"

/**
 * Data asset containing one or more NPC profile definitions.
 * Assigned to the modular NPC actor to drive its behaviour.
 */
UCLASS(BlueprintType)
class PICKPACKER_API UDA_NPCData : public UDataAsset
{
	GENERATED_BODY()

public:
	UDA_NPCData();

	/** The NPC profile that drives the actor's behaviour */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FNPCProfile Profile;

	/** Skeletal mesh override (optional – can also be set on actor) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals")
	TSoftObjectPtr<USkeletalMesh> MeshOverride;

	/** Optional material overrides applied by material slot index. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals")
	TArray<TSoftObjectPtr<class UMaterialInterface>> MaterialOverrides;

	/** Anim blueprint override */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals")
	TSoftClassPtr<UAnimInstance> AnimClassOverride;

	/** Animation asset bundle used by ABP_NPC_Base style animation blueprints. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals")
	TSoftObjectPtr<class UNPCAnimationSet> AnimationSetOverride;

	/** Optional behaviour tree for combat AI */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|AI")
	TSoftObjectPtr<UBehaviorTree> CombatBehaviorTree;

	/** Optional blackboard override for combat AI */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|AI")
	TSoftObjectPtr<UBlackboardData> CombatBlackboard;

	/** Optional behaviour tree for non-hostile ambient movement/patrol */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|AI")
	TSoftObjectPtr<UBehaviorTree> AmbientBehaviorTree;

	/** Optional blackboard override for ambient AI */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|AI")
	TSoftObjectPtr<UBlackboardData> AmbientBlackboard;

	/** Optional config table imported from CSV/Excel */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Import")
	TSoftObjectPtr<UDataTable> ConfigDataTable;

	/** Optional CSV source path used when ConfigDataTable is empty or unavailable */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Import")
	FFilePath ConfigCsvFile;

	/** Row name to read from ConfigDataTable */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Import")
	FName ConfigRowName = NAME_None;

	/** Optional dialogue script table imported from CSV/Excel */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Import")
	TSoftObjectPtr<UDataTable> DialogueScriptDataTable;

	/** Optional table that maps dialogue requirement tags/types to badge icons. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Dialogue|Indicators")
	TSoftObjectPtr<UDataTable> DialogueIndicatorIconDataTable;

	/** Optional CSV source path used when DialogueScriptDataTable is empty or unavailable */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Import")
	FFilePath DialogueCsvFile;

	// --- Helpers --------------------------------------------------------

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC")
	FName GetNPCId() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC")
	FText GetDisplayName() const;

	bool TryGetConfigRow(FNPCConfigTableRow& OutRow) const;
	void BuildResolvedProfile(FNPCProfile& OutProfile) const;
	void BuildResolvedDialogueNodes(const FName& EffectiveNPCId, TArray<FDialogueNode>& OutDialogueNodes) const;
	TSoftObjectPtr<USkeletalMesh> GetResolvedMeshOverride() const;
	TArray<TSoftObjectPtr<class UMaterialInterface>> GetResolvedMaterialOverrides() const;
	TSoftClassPtr<UAnimInstance> GetResolvedAnimClassOverride() const;
	TSoftObjectPtr<class UNPCAnimationSet> GetResolvedAnimationSet() const;
	TSoftObjectPtr<UBehaviorTree> GetResolvedCombatBehaviorTree() const;
	TSoftObjectPtr<UBlackboardData> GetResolvedCombatBlackboard() const;
	TSoftObjectPtr<UBehaviorTree> GetResolvedAmbientBehaviorTree() const;
	TSoftObjectPtr<UBlackboardData> GetResolvedAmbientBlackboard() const;
};
