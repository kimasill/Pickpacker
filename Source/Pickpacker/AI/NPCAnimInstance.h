#pragma once

#include "CoreMinimal.h"
#include "AI/PPAIAnimInstanceBase.h"
#include "PickpackerTypes/CoreLoopTypes.h"
#include "NPCAnimInstance.generated.h"

class AModularNPCActor;
class UDA_NPCData;
class UNPCAnimationSet;
class UAnimSequenceBase;
class UBlendSpace;

UENUM(BlueprintType)
enum class ENPCTopLevelAnimState : uint8
{
	Grounded UMETA(DisplayName = "Grounded"),
	Conversation UMETA(DisplayName = "Conversation"),
	Air UMETA(DisplayName = "Air"),
	Dead UMETA(DisplayName = "Dead"),
	Missing UMETA(DisplayName = "Missing")
};

/**
 * Shared anim instance layer for ModularNPCActor-based NPCs.
 * Keeps generic locomotion in the base class and exposes NPC state to ABPs.
 */
UCLASS()
class PICKPACKER_API UNPCAnimInstance : public UPPAIAnimInstanceBase
{
	GENERATED_BODY()

protected:
	virtual void CacheCharacterOwner() override;
	virtual void UpdateCharacterSpecificData(float DeltaTime) override;
	void RefreshAnimationSet();

public:
	UFUNCTION(BlueprintPure, Category = "Animation|Resolved", meta = (BlueprintThreadSafe))
	bool HasGroundedLocomotion() const;

	UFUNCTION(BlueprintPure, Category = "Animation|Resolved", meta = (BlueprintThreadSafe))
	bool ShouldPlayGroundedLocomotion() const;

	UFUNCTION(BlueprintPure, Category = "Animation|Resolved", meta = (BlueprintThreadSafe))
	UAnimSequenceBase* GetPreferredIdleAsset() const;

	UFUNCTION(BlueprintPure, Category = "Animation|Resolved", meta = (BlueprintThreadSafe))
	UAnimSequenceBase* GetConversationAsset() const;

	UFUNCTION(BlueprintPure, Category = "Animation|Resolved", meta = (BlueprintThreadSafe))
	UAnimSequenceBase* GetFallAsset() const;

	UFUNCTION(BlueprintPure, Category = "Animation|Resolved", meta = (BlueprintThreadSafe))
	UAnimSequenceBase* GetDeathAsset() const;

	UFUNCTION(BlueprintPure, Category = "Animation|Resolved", meta = (BlueprintThreadSafe))
	UAnimSequenceBase* GetMissingAsset() const;

private:
	UPROPERTY(BlueprintReadOnly, Category = "Character", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AModularNPCActor> NPCOwner = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNPCAnimationSet> AnimationSet = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Resolved", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBlendSpace> GroundedLocomotion = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Resolved", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimSequenceBase> DefaultIdle = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Resolved", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimSequenceBase> FriendlyIdle = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Resolved", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimSequenceBase> NeutralIdle = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Resolved", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimSequenceBase> HostileIdle = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Resolved", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimSequenceBase> ConversationLoop = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Resolved", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimSequenceBase> MissingPose = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Resolved", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimSequenceBase> DeathPose = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Resolved", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimSequenceBase> FallLoop = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "NPC", meta = (AllowPrivateAccess = "true"))
	ENPCDisposition CurrentDisposition = ENPCDisposition::Neutral;

	UPROPERTY(BlueprintReadOnly, Category = "NPC", meta = (AllowPrivateAccess = "true"))
	ENPCRole CurrentRole = ENPCRole::None;

	UPROPERTY(BlueprintReadOnly, Category = "NPC", meta = (AllowPrivateAccess = "true"))
	bool bIsFriendly = false;

	UPROPERTY(BlueprintReadOnly, Category = "NPC", meta = (AllowPrivateAccess = "true"))
	bool bIsNeutral = true;

	UPROPERTY(BlueprintReadOnly, Category = "NPC", meta = (AllowPrivateAccess = "true"))
	bool bIsHostile = false;

	UPROPERTY(BlueprintReadOnly, Category = "NPC", meta = (AllowPrivateAccess = "true"))
	bool bIsMissing = false;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Combat", meta = (AllowPrivateAccess = "true"))
	bool bCombatActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Combat", meta = (AllowPrivateAccess = "true"))
	bool bIsDead = false;

	UPROPERTY(BlueprintReadOnly, Category = "NPC|Dialogue", meta = (AllowPrivateAccess = "true"))
	bool bInConversation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|State", meta = (AllowPrivateAccess = "true"))
	ENPCTopLevelAnimState CurrentTopLevelAnimState = ENPCTopLevelAnimState::Grounded;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|State", meta = (AllowPrivateAccess = "true"))
	bool bHasConversationState = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|State", meta = (AllowPrivateAccess = "true"))
	bool bHasAirState = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|State", meta = (AllowPrivateAccess = "true"))
	bool bHasDeadState = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|State", meta = (AllowPrivateAccess = "true"))
	bool bHasMissingState = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Tuning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float LocomotionStartSpeed = 3.0f;

	UPROPERTY(Transient)
	TObjectPtr<UDA_NPCData> CachedNPCData = nullptr;
};
