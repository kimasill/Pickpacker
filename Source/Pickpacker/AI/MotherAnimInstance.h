// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "AI/MotherAIActor.h"
#include "MotherAnimInstance.generated.h"

/**
 * 
 */
UCLASS()
class PICKPACKER_API UMotherAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaTime) override;

private:
	/** Cached reference to Mother AI Actor */
	UPROPERTY(BlueprintReadOnly, Category = "Character", meta = (AllowPrivateAccess = "true"))
	AMotherAIActor* MotherAI;

	/** Movement Variables */
	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	float Speed;

	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	bool bIsInAir;

	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	bool bIsAccelerating;

	/** AI State */
	UPROPERTY(BlueprintReadOnly, Category = "AI State", meta = (AllowPrivateAccess = "true"))
	EMotherAIState CurrentAIState;

	/** Movement Direction */
	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	float Direction;

	/** Yaw Offset for movement blending */
	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	float YawOffset;

	/** Lean value for turning */
	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	float Lean;

	/** 처벌 몽타주 재생 중 여부 (스레드 안전) */
	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	bool bIsPlayingPunishmentMontage;

	/** 점검 몽타주 재생 중 여부 (스레드 안전) */
	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	bool bIsPlayingInspectionMontage;

	FRotator CharacterRotationLastFrame;
	FRotator CharacterRotation;
};
