#include "PPAIAnimInstanceBase.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

void UPPAIAnimInstanceBase::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	CacheCharacterOwner();
}

void UPPAIAnimInstanceBase::NativeUpdateAnimation(float DeltaTime)
{
	Super::NativeUpdateAnimation(DeltaTime);

	if (CharacterOwner == nullptr)
	{
		CacheCharacterOwner();
	}

	if (CharacterOwner == nullptr)
	{
		return;
	}

	UpdateCommonMovementData(DeltaTime);
	UpdateCharacterSpecificData(DeltaTime);
}

void UPPAIAnimInstanceBase::CacheCharacterOwner()
{
	CharacterOwner = Cast<ACharacter>(TryGetPawnOwner());
}

void UPPAIAnimInstanceBase::UpdateCharacterSpecificData(float DeltaTime)
{
}

void UPPAIAnimInstanceBase::UpdateAirborneState(bool bInRawIsInAir, float DeltaTime)
{
	bRawIsInAir = bInRawIsInAir;
	AirReenterLockoutRemaining = FMath::Max(0.0f, AirReenterLockoutRemaining - DeltaTime);

	if (bRawIsInAir)
	{
		RawAirborneTime += DeltaTime;
		RawGroundedTime = 0.0f;
	}
	else
	{
		RawGroundedTime += DeltaTime;
		RawAirborneTime = 0.0f;
	}

	if (!bIsInAir)
	{
		bIsInAir = bRawIsInAir && AirReenterLockoutRemaining <= 0.0f && RawAirborneTime >= AirEnterDelay;
		return;
	}

	if (!bRawIsInAir && RawGroundedTime >= AirExitDelay)
	{
		bIsInAir = false;
		AirReenterLockoutRemaining = AirReenterLockoutAfterLanding;
	}
}

void UPPAIAnimInstanceBase::UpdateCommonMovementData(float DeltaTime)
{
	if (CharacterOwner == nullptr)
	{
		return;
	}

	const FVector RawVelocity = CharacterOwner->GetVelocity();
	FVector HorizontalVelocity = RawVelocity;
	HorizontalVelocity.Z = 0.0f;

	Speed = HorizontalVelocity.Size();

	if (const UCharacterMovementComponent* MovementComponent = CharacterOwner->GetCharacterMovement())
	{
		UpdateAirborneState(MovementComponent->IsFalling(), DeltaTime);
		bIsAccelerating = MovementComponent->GetCurrentAcceleration().SizeSquared() > 0.0f;
	}
	else
	{
		UpdateAirborneState(false, DeltaTime);
		bIsAccelerating = false;
	}

	const FRotator ActorRotation = CharacterOwner->GetActorRotation();
	if (Speed > KINDA_SMALL_NUMBER)
	{
		const FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(HorizontalVelocity);
		Direction = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, ActorRotation).Yaw;
		YawOffset = UKismetMathLibrary::NormalizedDeltaRotator(ActorRotation, MovementRotation).Yaw;
	}
	else
	{
		Direction = 0.0f;
		YawOffset = 0.0f;
	}

	CharacterRotationLastFrame = CharacterRotation;
	CharacterRotation = ActorRotation;

	if (DeltaTime > UE_SMALL_NUMBER)
	{
		const FRotator DeltaRotation =
			UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotation, CharacterRotationLastFrame);
		const float TargetLean = DeltaRotation.Yaw / DeltaTime;
		const float InterpolatedLean = FMath::FInterpTo(Lean, TargetLean, DeltaTime, 6.0f);
		Lean = FMath::Clamp(InterpolatedLean, -90.0f, 90.0f);
	}
}
