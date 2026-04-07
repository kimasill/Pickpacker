// NPC Data Asset - Defines NPC configurations via data

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "PickpackerTypes/CoreLoopTypes.h"
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

	/** Anim blueprint override */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Visuals")
	TSoftClassPtr<UAnimInstance> AnimClassOverride;

	/** Optional behaviour tree for combat AI */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|AI")
	TSoftObjectPtr<UObject> CombatBehaviorTree;

	// --- Helpers --------------------------------------------------------

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC")
	FName GetNPCId() const { return Profile.NPCId; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC")
	FText GetDisplayName() const { return Profile.DisplayName; }
};
