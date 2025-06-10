#include "BlasterAnimInstance.h"
#include "BlasterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

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
	bIsCrouched = BlasterCharacter->bIsCrouched; // Check if the character is crouched
	bIsAiming = BlasterCharacter->IsAiming(); // Check if the character is aiming
}