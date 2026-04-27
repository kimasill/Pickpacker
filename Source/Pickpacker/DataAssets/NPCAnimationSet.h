#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NPCAnimationSet.generated.h"

class UAnimMontage;
class UAnimSequenceBase;
class UBlendSpace;

/**
 * Reusable animation asset bundle for ModularNPCActor-based blueprints.
 * ABP_NPC_Base should read from this asset instead of hardcoding individual sequences.
 */
UCLASS(BlueprintType)
class PICKPACKER_API UNPCAnimationSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Base locomotion blend space for ordinary movement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	TSoftObjectPtr<UBlendSpace> GroundedLocomotion;

	/** Optional neutral idle override when no blend space is available. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Idle")
	TSoftObjectPtr<UAnimSequenceBase> DefaultIdle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Idle")
	TSoftObjectPtr<UAnimSequenceBase> FriendlyIdle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Idle")
	TSoftObjectPtr<UAnimSequenceBase> NeutralIdle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Idle")
	TSoftObjectPtr<UAnimSequenceBase> HostileIdle;

	/** Loop or additive used while the NPC is talking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TSoftObjectPtr<UAnimSequenceBase> ConversationLoop;

	/** Optional pose to use when the NPC is marked missing/gone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State")
	TSoftObjectPtr<UAnimSequenceBase> MissingPose;

	/** Optional sequence/pose when the NPC has died. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State")
	TSoftObjectPtr<UAnimSequenceBase> DeathPose;

	/** Optional in-air loop for jump/fall capable NPCs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air")
	TSoftObjectPtr<UAnimSequenceBase> FallLoop;

	/** Optional additive / montage hooks for impact reactions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montage")
	TSoftObjectPtr<UAnimMontage> HitReactMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montage")
	TSoftObjectPtr<UAnimMontage> InteractionMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montage")
	TSoftObjectPtr<UAnimMontage> AttackMontage;
};
