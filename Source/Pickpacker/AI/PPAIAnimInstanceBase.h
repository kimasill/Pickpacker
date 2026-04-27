#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PPAIAnimInstanceBase.generated.h"

class ACharacter;

/**
 * Shared locomotion data for AI/NPC animation blueprints.
 * Character-specific gameplay state should stay in thin subclasses.
 */
UCLASS(Abstract)
class PICKPACKER_API UPPAIAnimInstanceBase : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaTime) override;

protected:
	virtual void CacheCharacterOwner();
	virtual void UpdateCharacterSpecificData(float DeltaTime);
	void UpdateCommonMovementData(float DeltaTime);
	void UpdateAirborneState(bool bRawIsInAir, float DeltaTime);

	UFUNCTION(BlueprintPure, Category = "Character")
	ACharacter* GetCharacterOwner() const { return CharacterOwner; }

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Character", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ACharacter> CharacterOwner = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	float Speed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	bool bIsInAir = false;

	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	bool bRawIsInAir = false;

	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	bool bIsAccelerating = false;

	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	float Direction = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	float YawOffset = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	float Lean = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	FRotator CharacterRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	FRotator CharacterRotationLastFrame = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Tuning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AirEnterDelay = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Tuning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AirExitDelay = 0.10f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Tuning", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AirReenterLockoutAfterLanding = 0.15f;

	UPROPERTY(Transient)
	float RawAirborneTime = 0.0f;

	UPROPERTY(Transient)
	float RawGroundedTime = 0.0f;

	UPROPERTY(Transient)
	float AirReenterLockoutRemaining = 0.0f;
};
