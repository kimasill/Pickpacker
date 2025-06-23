#include "BlasterAnimInstance.h"
#include "BlasterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Blaster/Weapon/Weapon.h"
#include "Blaster/BlasterTypes/CombatState.h"

void UBlasterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	BlasterCharacter = Cast<ABlasterCharacter>(TryGetPawnOwner());
	// Initialize any variables or states here
}

void UBlasterAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
	Super::NativeUpdateAnimation(DeltaTime);
	// Update animation variables or states here
	if(BlasterCharacter == nullptr)
	{
		BlasterCharacter = Cast<ABlasterCharacter>(TryGetPawnOwner());
	}
	if (BlasterCharacter == nullptr) return;

	FVector Velocity = BlasterCharacter->GetVelocity();
	Velocity.Z = 0; // Ignore vertical velocity for ground movement
	Speed = Velocity.Size(); // Calculate speed based on horizontal velocity

	bIsInAir = BlasterCharacter->GetCharacterMovement()->IsFalling(); // Check if the character is in the air
	bIsAccelerating = BlasterCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.0f ? true : false; // Check if the character is accelerating
	bWeaponEquipped = BlasterCharacter->IsWeaponEquipped(); // Check if the character has a weapon equipped
	EquippedWeapon = BlasterCharacter->GetEquippedWeapon(); // Get the currently equipped weapon
	bIsCrouched = BlasterCharacter->bIsCrouched; // Check if the character is crouched
	bIsAiming = BlasterCharacter->IsAiming(); // Check if the character is aiming
	TurningInPlace = BlasterCharacter->GetTurningInPlace();
	bRotateRootBone = BlasterCharacter->ShouldRotateRootBone(); // Check if the root bone should rotate
	bElimmed = BlasterCharacter->IsElimmed(); // Check if the character is eliminated

	FRotator AimRotation = BlasterCharacter->GetBaseAimRotation();
	FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(BlasterCharacter->GetVelocity());
	FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(AimRotation, MovementRotation); // Calculate the yaw offset between aim and movement
	DeltaRotation = FMath::RInterpTo(DeltaRotation, DeltaRot, DeltaTime, 15.f);
	YawOffset = DeltaRotation.Yaw; // Store the yaw offset for animations

	CharacterRotationLastFrame = CharacterRotation;
	CharacterRotation = BlasterCharacter->GetActorRotation(); // Get the character's current rotation
	const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotation, CharacterRotationLastFrame);
	const float Target = Delta.Yaw / DeltaTime; // Calculate the target yaw rate
	const float Interp = FMath::FInterpTo(Lean, Target, DeltaTime, 6.0f); // Interpolate the yaw rate
	Lean = FMath::Clamp(Interp, -90.0f, 90.0f); // Clamp the lean value to prevent excessive rotation

	AO_Yaw = BlasterCharacter->GetAO_Yaw(); // Get the aim offset yaw
	AO_Pitch = BlasterCharacter->GetAO_Pitch(); // Get the aim offset pitch

	if (bWeaponEquipped && EquippedWeapon && EquippedWeapon->GetWeaponMesh() && BlasterCharacter->GetMesh()) 
	{
		LeftHandTransform = EquippedWeapon->GetWeaponMesh()->GetSocketTransform(FName("LeftHandSocket"), ERelativeTransformSpace::RTS_World);
		FVector OutPosition;
		FRotator OutRotation;
		BlasterCharacter->GetMesh()->TransformToBoneSpace(FName("hand_r"), LeftHandTransform.GetLocation(), FRotator::ZeroRotator, OutPosition, OutRotation);
		LeftHandTransform.SetLocation(OutPosition);
		LeftHandTransform.SetRotation(FQuat(OutRotation)); // Set the left hand transform based on the weapon's socket

		if (BlasterCharacter->IsLocallyControlled()) {
			bLocallyControlled = true; // Check if the character is locally controlled
			FTransform RightHandTransform = EquippedWeapon->GetWeaponMesh()->GetSocketTransform(FName("hand_r"), ERelativeTransformSpace::RTS_World);
			FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(RightHandTransform.GetLocation(), RightHandTransform.GetLocation() + (RightHandTransform.GetLocation() - BlasterCharacter->GetHitTarget()));
			RightHandRotation = FMath::RInterpTo(RightHandRotation, LookAtRotation, DeltaTime, 30.0f); // Interpolate the right hand rotation towards the look-at rotation
		}

		
		FTransform MuzzleTipTransform = EquippedWeapon->GetWeaponMesh()->GetSocketTransform(FName("MuzzleFlash"), ERelativeTransformSpace::RTS_World);
		FVector MuzzleX(FRotationMatrix(MuzzleTipTransform.GetRotation().Rotator()).GetUnitAxis(EAxis::X));
		DrawDebugLine(GetWorld(), MuzzleTipTransform.GetLocation(), MuzzleTipTransform.GetLocation() + MuzzleX * 1000.0f, FColor::Red, false, -1.0f, 0, 2.0f);
		DrawDebugLine(GetWorld(), MuzzleTipTransform.GetLocation(), BlasterCharacter->GetHitTarget(), FColor::Orange);
	}

	bUseFABRIK = BlasterCharacter->GetCombatState() != ECombatState::ECS_Reloading; // Check if FABRIK is used for animation
	bUseAimOffsets = BlasterCharacter->GetCombatState() != ECombatState::ECS_Reloading; // Check if aim offsets are used for animation
	bTransformRightHand = BlasterCharacter->GetCombatState() != ECombatState::ECS_Reloading; // Check if the right hand transform is used for animation
}
