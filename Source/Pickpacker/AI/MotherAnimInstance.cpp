

#include "MotherAnimInstance.h"
#include "AI/MotherAIActor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"

void UMotherAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	MotherAI = Cast<AMotherAIActor>(TryGetPawnOwner());
}

void UMotherAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
	Super::NativeUpdateAnimation(DeltaTime);

	if (MotherAI == nullptr)
	{
		MotherAI = Cast<AMotherAIActor>(TryGetPawnOwner());
	}

	if (MotherAI == nullptr)
	{
		return;
	}

	// Update movement variables
	FVector Velocity = MotherAI->GetVelocity();
	Velocity.Z = 0.0f; // Ignore vertical velocity
	Speed = Velocity.Size();

	bIsInAir = MotherAI->GetCharacterMovement()->IsFalling();
	bIsAccelerating = MotherAI->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.0f;

	// Update AI state
	CurrentAIState = MotherAI->GetAIState();

	// Calculate direction
	FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(Velocity);
	FRotator ActorRotation = MotherAI->GetActorRotation();
	Direction = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, ActorRotation).Yaw;

	// Calculate yaw offset
	FRotator AimRotation = MotherAI->GetActorRotation();
	FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(AimRotation, MovementRotation);
	YawOffset = DeltaRot.Yaw;

	// Calculate lean
	CharacterRotationLastFrame = CharacterRotation;
	CharacterRotation = MotherAI->GetActorRotation();
	const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotation, CharacterRotationLastFrame);
	const float Target = Delta.Yaw / DeltaTime;
	const float Interp = FMath::FInterpTo(Lean, Target, DeltaTime, 6.0f);
	Lean = FMath::Clamp(Interp, -90.0f, 90.0f);

	if (UAnimInstance* AnimInstance = MotherAI->GetMesh()->GetAnimInstance())
	{
		if (UAnimMontage* PunishmentMontage = MotherAI->GetPunishmentMontage())
		{
			bIsPlayingPunishmentMontage = AnimInstance->Montage_IsPlaying(PunishmentMontage);
		}
		else
		{
			bIsPlayingPunishmentMontage = false;
		}

		if (UAnimMontage* InspectionMontage = MotherAI->GetInspectionMontage())
		{
			bIsPlayingInspectionMontage = AnimInstance->Montage_IsPlaying(InspectionMontage);
		}
		else
		{
			bIsPlayingInspectionMontage = false;
		}
	}
}