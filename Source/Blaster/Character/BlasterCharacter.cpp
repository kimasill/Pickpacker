// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "Blaster/Weapon/Weapon.h"
#include "Blaster/BlasterComponents/CombatComponent.h"
#include "Blaster/BlasterComponents/BuffComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "BlasterAnimInstance.h"
#include "Blaster/Blaster.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/GameMode/BlasterGameMode.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
#include "Particles/ParticleSystemComponent.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Blaster/Weapon/WeaponTypes.h"
#include "Components/BoxComponent.h"
#include "Blaster/BlasterComponents/LagCompensationComponent.h"
#include "Blaster/GameState/BlasterGameState.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Blaster/PlayerStart/TeamPlayerStart.h"
#include "Blaster/Components/InteractionComponent.h"
#include "Blaster/Components/CarryIKComponent.h"
#include "Blaster/Components/PlayerInventoryComponent.h"
#include "Blaster/Library/PickpackerSuspicionLibrary.h"
#include "Blaster/Subsystem/SuspicionManagerSubsystem.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimInstance.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/GameInstance.h"
#include "Blaster/Components/ParcelStateComponent.h"
#include "Blaster/Parcel/ParcelActor.h"

ABlasterCharacter::ABlasterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 1인칭 카메라를 Capsule에 직접 부착 (SpringArm 제거)
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(GetCapsuleComponent());
	FollowCamera->SetRelativeLocation(FVector(0.f, 0.f, EyeHeight));
	FollowCamera->bUsePawnControlRotation = true;


	bUseControllerRotationYaw = true;

	GetCharacterMovement()->bOrientRotationToMovement = false;

	OverHeadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverHeadWidget"));
	OverHeadWidget->SetupAttachment(RootComponent);

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	Combat->SetIsReplicated(true);

	Buff = CreateDefaultSubobject<UBuffComponent>(TEXT("BuffComponent"));
	Buff->SetIsReplicated(true);

	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));
	LagCompensation = CreateDefaultSubobject<ULagCompensationComponent>(TEXT("LagCompensation"));
	
	CarryIKComponent = CreateDefaultSubobject<UCarryIKComponent>(TEXT("CarryIKComponent"));
	PlayerInventoryComponent = CreateDefaultSubobject<UPlayerInventoryComponent>(TEXT("PlayerInventoryComponent"));

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionObjectType(ECC_SkeletalMesh);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 850.f);

	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	SetNetUpdateFrequency(66.f);
	SetMinNetUpdateFrequency(33.f);

	DissolveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DissolveTimelineComponent"));

	AttachedGrenade = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Attached Grenade"));
	AttachedGrenade->SetupAttachment(GetMesh(), FName("GrenadeSocket"));
	AttachedGrenade->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	/**
	* Hit boxes for server-side rewind
	*/

	head = CreateDefaultSubobject<UBoxComponent>(TEXT("head"));
	head->SetupAttachment(GetMesh(), FName("head"));
	HitCollisionBoxes.Add(FName("head"), head);

	pelvis = CreateDefaultSubobject<UBoxComponent>(TEXT("pelvis"));
	pelvis->SetupAttachment(GetMesh(), FName("pelvis"));
	HitCollisionBoxes.Add(FName("pelvis"), pelvis);

	spine_02 = CreateDefaultSubobject<UBoxComponent>(TEXT("spine_02"));
	spine_02->SetupAttachment(GetMesh(), FName("spine_02"));
	HitCollisionBoxes.Add(FName("spine_02"), spine_02);

	spine_03 = CreateDefaultSubobject<UBoxComponent>(TEXT("spine_03"));
	spine_03->SetupAttachment(GetMesh(), FName("spine_03"));
	HitCollisionBoxes.Add(FName("spine_03"), spine_03);

	upperarm_l = CreateDefaultSubobject<UBoxComponent>(TEXT("upperarm_l"));
	upperarm_l->SetupAttachment(GetMesh(), FName("upperarm_l"));
	HitCollisionBoxes.Add(FName("upperarm_l"), upperarm_l);

	upperarm_r = CreateDefaultSubobject<UBoxComponent>(TEXT("upperarm_r"));
	upperarm_r->SetupAttachment(GetMesh(), FName("upperarm_r"));
	HitCollisionBoxes.Add(FName("upperarm_r"), upperarm_r);

	lowerarm_l = CreateDefaultSubobject<UBoxComponent>(TEXT("lowerarm_l"));
	lowerarm_l->SetupAttachment(GetMesh(), FName("lowerarm_l"));
	HitCollisionBoxes.Add(FName("lowerarm_l"), lowerarm_l);

	lowerarm_r = CreateDefaultSubobject<UBoxComponent>(TEXT("lowerarm_r"));
	lowerarm_r->SetupAttachment(GetMesh(), FName("lowerarm_r"));
	HitCollisionBoxes.Add(FName("lowerarm_r"), lowerarm_r);

	hand_l = CreateDefaultSubobject<UBoxComponent>(TEXT("hand_l"));
	hand_l->SetupAttachment(GetMesh(), FName("hand_l"));
	HitCollisionBoxes.Add(FName("hand_l"), hand_l);

	hand_r = CreateDefaultSubobject<UBoxComponent>(TEXT("hand_r"));
	hand_r->SetupAttachment(GetMesh(), FName("hand_r"));
	HitCollisionBoxes.Add(FName("hand_r"), hand_r);

	blanket = CreateDefaultSubobject<UBoxComponent>(TEXT("blanket"));
	blanket->SetupAttachment(GetMesh(), FName("backpack"));
	HitCollisionBoxes.Add(FName("blanket"), blanket);

	backpack = CreateDefaultSubobject<UBoxComponent>(TEXT("backpack"));
	backpack->SetupAttachment(GetMesh(), FName("backpack"));
	HitCollisionBoxes.Add(FName("backpack"), backpack);

	thigh_l = CreateDefaultSubobject<UBoxComponent>(TEXT("thigh_l"));
	thigh_l->SetupAttachment(GetMesh(), FName("thigh_l"));
	HitCollisionBoxes.Add(FName("thigh_l"), thigh_l);

	thigh_r = CreateDefaultSubobject<UBoxComponent>(TEXT("thigh_r"));
	thigh_r->SetupAttachment(GetMesh(), FName("thigh_r"));
	HitCollisionBoxes.Add(FName("thigh_r"), thigh_r);

	calf_l = CreateDefaultSubobject<UBoxComponent>(TEXT("calf_l"));
	calf_l->SetupAttachment(GetMesh(), FName("calf_l"));
	HitCollisionBoxes.Add(FName("calf_l"), calf_l);

	calf_r = CreateDefaultSubobject<UBoxComponent>(TEXT("calf_r"));
	calf_r->SetupAttachment(GetMesh(), FName("calf_r"));
	HitCollisionBoxes.Add(FName("calf_r"), calf_r);

	foot_l = CreateDefaultSubobject<UBoxComponent>(TEXT("foot_l"));
	foot_l->SetupAttachment(GetMesh(), FName("foot_l"));
	HitCollisionBoxes.Add(FName("foot_l"), foot_l);

	foot_r = CreateDefaultSubobject<UBoxComponent>(TEXT("foot_r"));
	foot_r->SetupAttachment(GetMesh(), FName("foot_r"));
	HitCollisionBoxes.Add(FName("foot_r"), foot_r);

	for (auto Box : HitCollisionBoxes)
	{
		if (Box.Value)
		{
			Box.Value->SetCollisionObjectType(ECC_HitBox);
			Box.Value->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
			Box.Value->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
			Box.Value->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

void ABlasterCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyDebugCollisionVisibility();
}

void ABlasterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon, COND_OwnerOnly);
	DOREPLIFETIME(ABlasterCharacter, Health);
	DOREPLIFETIME(ABlasterCharacter, Shield);
	DOREPLIFETIME(ABlasterCharacter, bDisableGameplay);
	DOREPLIFETIME(ABlasterCharacter, CurrentSuspiciousBehavior);
	DOREPLIFETIME(ABlasterCharacter, bBeingPunished);
	DOREPLIFETIME(ABlasterCharacter, bEndingInProgress);
}

void ABlasterCharacter::OnRep_ReplicatedMovement()
{
	Super::OnRep_ReplicatedMovement();
	SimProxiesTurn();
	TimeSinceLastMovementReplication = 0.f;
}

void ABlasterCharacter::Elim(bool bPlayerLeftGame)
{
	DropOrDestroyWeapons();
	MulticastElim(bPlayerLeftGame);
	
}

void ABlasterCharacter::SetEndingInProgress(bool bInProgress)
{
	bEndingInProgress = bInProgress;
	bDisableGameplay = bInProgress;

	if (bEndingInProgress && GetCharacterMovement())
	{
		GetCharacterMovement()->DisableMovement();
	}
}

void ABlasterCharacter::MulticastElim_Implementation(bool bPlayerLeftGame)
{
	bLeftGame = bPlayerLeftGame;
	if (BlasterPlayerController)
	{
		BlasterPlayerController->SetHUDWeaponAmmo(0);
	}
	bElimmed = true;
	PlayElimMontage();
	// Start dissolve effect
	if (DissolveMaterialInstance)
	{
		DynamicDissolveMaterialInstance = UMaterialInstanceDynamic::Create(DissolveMaterialInstance, this);
		GetMesh()->SetMaterial(0, DynamicDissolveMaterialInstance);
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), 0.55f);
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Glow"), 200.f);
	}
	StartDissolve();

	// Disable character movement
	bDisableGameplay = true;
	GetCharacterMovement()->DisableMovement();
	if (Combat)
	{
		Combat->FireButtonPressed(false);
	}
	// Disable collision
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AttachedGrenade->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Spawn elim bot
	if (ElimBotEffect)
	{
		FVector ElimBotSpawnPoint(GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z + 200.f);
		ElimBotComponent = UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			ElimBotEffect,
			ElimBotSpawnPoint,
			GetActorRotation()
		);
	}
	if (ElimBotSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(
			this,
			ElimBotSound,
			GetActorLocation()
		);
	}
	bool bHideSniperScope = IsLocallyControlled() &&
		Combat &&
		Combat->bAiming &&
		Combat->EquippedWeapon &&
		Combat->EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle;
	if (bHideSniperScope)
	{
		ShowSniperScopeWidget(false);
	}
	if (CrownSystemComponent)
	{
		CrownSystemComponent->DestroyComponent();
	}
	GetWorldTimerManager().SetTimer(
		ElimTimer,
		this,
		&ABlasterCharacter::ElimTimerFinished,
		ElimDelay
	);
}

void ABlasterCharacter::ElimTimerFinished()
{
	BlasterGameMode = BlasterGameMode == nullptr ? GetWorld()->GetAuthGameMode<ABlasterGameMode>() : BlasterGameMode;
	if (BlasterGameMode && !bLeftGame)
	{
		BlasterGameMode->RequestRespawn(this, Controller);
	}

	if (bLeftGame && IsLocallyControlled())
	{
		OnLeftGame.Broadcast();
	}
}

void ABlasterCharacter::ServerLeaveGame_Implementation()
{
	BlasterGameMode = BlasterGameMode == nullptr ? GetWorld()->GetAuthGameMode<ABlasterGameMode>() : BlasterGameMode;
	BlasterPlayerState = BlasterPlayerState == nullptr ? GetPlayerState<ABlasterPlayerState>() : BlasterPlayerState;

	if (BlasterGameMode && BlasterPlayerState)
	{
		BlasterGameMode->PlayerLeftGame(BlasterPlayerState);
	}
}

void ABlasterCharacter::DropOrDestroyWeapon(AWeapon* Weapon)
{
	if (Weapon == nullptr) return;
	if (Weapon->bDestroyWeapon)
	{
		Weapon->Destroy();
	}
	else
	{
		Weapon->Dropped();
	}
}

void ABlasterCharacter::DropOrDestroyWeapons()
{
	if (Combat)
	{
		if (Combat->EquippedWeapon)
		{
			DropOrDestroyWeapon(Combat->EquippedWeapon);
		}
		if (Combat->SecondaryWeapon)
		{
			DropOrDestroyWeapon(Combat->SecondaryWeapon);
		}
	}
}

void ABlasterCharacter::OnPlayerStateInitialized()
{
	BlasterPlayerState->AddToScore(0.f);
	BlasterPlayerState->AddToDefeats(0);
	SetTeamColor(BlasterPlayerState->GetTeam());
	SetSpawnPoint();
}


void ABlasterCharacter::SetSpawnPoint()
{
	if (HasAuthority() && BlasterPlayerState->GetTeam() != ETeam::ET_NoTeam)
	{
		TArray<AActor*> PlayerStarts;
		UGameplayStatics::GetAllActorsOfClass(this, ATeamPlayerStart::StaticClass(), PlayerStarts);
		TArray<ATeamPlayerStart*> TeamPlayerStarts;
		for(auto Start: PlayerStarts)
		{
			ATeamPlayerStart* TeamStart = Cast<ATeamPlayerStart>(Start);
			if (TeamStart && TeamStart->Team == BlasterPlayerState->GetTeam())
			{
				TeamPlayerStarts.Add(TeamStart);
			}
		}
		if (TeamPlayerStarts.Num() > 0)
		{
			ATeamPlayerStart* ChosenPlayerStart = TeamPlayerStarts[FMath::RandRange(0, TeamPlayerStarts.Num() - 1)];
			SetActorLocationAndRotation(
				ChosenPlayerStart->GetActorLocation(),
				ChosenPlayerStart->GetActorRotation()
			);
		}
	}
}

void ABlasterCharacter::Destroyed()
{
	Super::Destroyed();

	if (ElimBotComponent)
	{
		ElimBotComponent->DestroyComponent();
	}

	BlasterGameMode = BlasterGameMode == nullptr ? GetWorld()->GetAuthGameMode<ABlasterGameMode>() : BlasterGameMode;
	bool bMatchNotInProgress = BlasterGameMode && BlasterGameMode->GetMatchState() != MatchState::InProgress;
	if (Combat && Combat->EquippedWeapon && bMatchNotInProgress)
	{
		Combat->EquippedWeapon->Destroy();
	}
}

void ABlasterCharacter::MulticastGainedTheLead_Implementation()
{
	if (CrownSystem == nullptr) return;
	if (CrownSystemComponent == nullptr)
	{
		CrownSystemComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			CrownSystem,
			GetMesh(),
			FName(),
			GetActorLocation(),
			GetActorRotation(),
			EAttachLocation::KeepWorldPosition,
			false
		);
	}
	if (CrownSystemComponent)
	{
		CrownSystemComponent->Activate();
	}
}
			

void ABlasterCharacter::MulticastLostTheLead_Implementation()
{
	if(CrownSystemComponent)
	{
		CrownSystemComponent->DestroyComponent();
	}
}

void ABlasterCharacter::Client_PlayHitCameraShake_Implementation(float Scale)
{
	// 로컬 컨트롤러에서만 실행
	ABlasterPlayerController* PC = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if (!PC || !CameraShakeClass) return;
	
	PC->ClientStartCameraShake(CameraShakeClass, Scale);
}

void ABlasterCharacter::SetTeamColor(ETeam Team)
{
	if (GetMesh() == nullptr || OriginalMaterial == nullptr) return;
	switch (Team)
	{
	case ETeam::ET_NoTeam:
		GetMesh()->SetMaterial(0, OriginalMaterial);
		DissolveMaterialInstance = BlueDissolveMatInst;
		break;
	case ETeam::ET_BlueTeam:
		GetMesh()->SetMaterial(0, BlueMaterial);
		DissolveMaterialInstance = BlueDissolveMatInst;
		break;
	case ETeam::ET_RedTeam:
		GetMesh()->SetMaterial(0, RedMaterial);
		DissolveMaterialInstance = RedDissolveMatInst;
		break;
	}
}

void ABlasterCharacter::ReportSuspiciousBehavior(ESuspiciousBehavior Behavior)
{
	if (!HasAuthority())
	{
		return;
	}

	UpdateSuspiciousBehavior(Behavior);
}

void ABlasterCharacter::BeginPlay()
{
	Super::BeginPlay();
	SpawnDefaultWeapon();
	UpdateHUDAmmo();
	UpdateHUDHealth();
	UpdateHUDShield();
	if (HasAuthority())
	{
		OnTakeAnyDamage.AddDynamic(this, &ABlasterCharacter::ReceiveDamage);
	}
	if (AttachedGrenade)
	{
		AttachedGrenade->SetVisibility(false);
	}

	ToggleHeadMesh(IsPerspectiveFirstPerson());
	ApplyDebugCollisionVisibility();
	
	// 1인칭 카메라 EyeHeight 설정
	if (FollowCamera)
	{
		FollowCamera->SetRelativeLocation(FVector(0.f, 0.f, EyeHeight));
	}
	
	// Mesh 회전 설정: Mesh는 Controller 회전을 따르지 않도록 설정 (카메라 흔들림 방지)
	if (GetMesh())
	{
		// SkeletalMeshComponent에는 컨트롤러 회전 플래그가 없으므로 World/Relative 회전을 유지하도록 기본값 사용
		GetMesh()->SetUsingAbsoluteRotation(false);
	}
 	
 	// Reset cached speed on begin play
}

void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RotateInPlace(DeltaTime);
	HideCameraIfCharacterClose();
	HideCarriedCameraIfCharacterClose();
	RotateCameraToPunisher(DeltaTime);
	UpdateMovementSpeedFromCarriedParcel();
	PollInit();
}

void ABlasterCharacter::RotateInPlace(float DeltaTime)
{
	if (Combat && Combat->bHoldingTheFlag)
	{
		bUseControllerRotationYaw = false;
		GetCharacterMovement()->bOrientRotationToMovement = true;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}
	if(Combat && Combat->EquippedWeapon) GetCharacterMovement()->bOrientRotationToMovement = false;
	if (Combat && Combat->EquippedWeapon) bUseControllerRotationYaw = true;	
	if (bDisableGameplay)
	{
		bUseControllerRotationYaw = false;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}
	if (GetLocalRole() > ENetRole::ROLE_SimulatedProxy && IsLocallyControlled())
	{
		AimOffset(DeltaTime);
	}
	else
	{
		TimeSinceLastMovementReplication += DeltaTime;
		if (TimeSinceLastMovementReplication > 0.25f)
		{
			OnRep_ReplicatedMovement();
		}
		CalculateAO_Pitch();
	}
}

void ABlasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ABlasterCharacter::Jump);

	PlayerInputComponent->BindAxis("MoveForward", this, &ABlasterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ABlasterCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &ABlasterCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &ABlasterCharacter::LookUp);

	PlayerInputComponent->BindAction("Equip", IE_Pressed, this, &ABlasterCharacter::EquipButtonPressed);
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ABlasterCharacter::CrouchButtonPressed);
	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &ABlasterCharacter::AimButtonPressed);
	PlayerInputComponent->BindAction("Aim", IE_Released, this, &ABlasterCharacter::AimButtonReleased);
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ABlasterCharacter::FireButtonPressed);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &ABlasterCharacter::FireButtonReleased);
	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &ABlasterCharacter::ReloadButtonPressed);
	PlayerInputComponent->BindAction("ThrowGrenade", IE_Pressed, this, &ABlasterCharacter::GrenadeButtonPressed);	
	
	// Enhanced Input: Bind inventory slot actions (1-9)
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Single inventory action: put carried parcel into inventory
		if (InventoryInputAction)
		{
			EnhancedInputComponent->BindAction(InventoryInputAction, ETriggerEvent::Started, this, &ABlasterCharacter::OnInventoryAction);
		}
		if (InventorySlotOneAction)
		{
			EnhancedInputComponent->BindAction(InventorySlotOneAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::OnInventorySlotOne);
		}
		if (InventorySlotTwoAction)
		{
			EnhancedInputComponent->BindAction(InventorySlotTwoAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::OnInventorySlotTwo);
		}
		if (InventorySlotThreeAction)
		{
			EnhancedInputComponent->BindAction(InventorySlotThreeAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::OnInventorySlotThree);
		}
		if (InventorySlotFourAction)
		{
			EnhancedInputComponent->BindAction(InventorySlotFourAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::OnInventorySlotFour);
		}
		if (InventorySlotFiveAction)
		{
			EnhancedInputComponent->BindAction(InventorySlotFiveAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::OnInventorySlotFive);
		}
		if (InventorySlotSixAction)
		{
			EnhancedInputComponent->BindAction(InventorySlotSixAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::OnInventorySlotSix);
		}
		if (InventorySlotSevenAction)
		{
			EnhancedInputComponent->BindAction(InventorySlotSevenAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::OnInventorySlotSeven);
		}
		if (InventorySlotEightAction)
		{
			EnhancedInputComponent->BindAction(InventorySlotEightAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::OnInventorySlotEight);
		}
		if (InventorySlotNineAction)
		{
			EnhancedInputComponent->BindAction(InventorySlotNineAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::OnInventorySlotNine);
		}
	}
}

void ABlasterCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (Combat)
	{
		Combat->Character = this;
	}
	if (Buff)
	{
		Buff->Character = this;
		Buff->SetInitialSpeeds(
			GetCharacterMovement()->MaxWalkSpeed,
			GetCharacterMovement()->MaxWalkSpeedCrouched
		);
		Buff->SetInitialJumpVelocity(GetCharacterMovement()->JumpZVelocity);
	}
	if (LagCompensation)
	{
		LagCompensation->Character = this;
		if (Controller)
		{
			LagCompensation->Controller = Cast<ABlasterPlayerController>(Controller);
		}
	}
}

void ABlasterCharacter::PlayFireMontage(bool bAiming)
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && FireWeaponMontage)
	{
		AnimInstance->Montage_Play(FireWeaponMontage);
		FName SectionName;
		SectionName = bAiming ? FName("RifleAim") : FName("RifleHip");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::PlayReloadMontage()
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ReloadMontage)
	{
		AnimInstance->Montage_Play(ReloadMontage);
		FName SectionName;

		switch (Combat->EquippedWeapon->GetWeaponType())
		{
		case EWeaponType::EWT_AssaultRifle:
			SectionName = FName("Rifle");
			break;
		case EWeaponType::EWT_RocketLauncher:
			SectionName = FName("RocketLauncher");
			break;
		case EWeaponType::EWT_Pistol:
			SectionName = FName("Pistol");
			break;
		case EWeaponType::EWT_SubmachineGun:
			SectionName = FName("Pistol");
			break;
		case EWeaponType::EWT_Shotgun:
			SectionName = FName("Shotgun");
			break;
		case EWeaponType::EWT_SniperRifle:
			SectionName = FName("SniperRifle");
			break;
		case EWeaponType::EWT_GrenadeLauncher:
			SectionName = FName("GrenadeLauncher");
			break;
		}

		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::PlayElimMontage()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ElimMontage)
	{
		AnimInstance->Montage_Play(ElimMontage);
	}
}

void ABlasterCharacter::PlayThrowGrenadeMontage()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ThrowGrenadeMontage)
	{
		AnimInstance->Montage_Play(ThrowGrenadeMontage);
	}
}

void ABlasterCharacter::PlaySwapMontage()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && SwapMontage)
	{
		AnimInstance->Montage_Play(SwapMontage);
	}
}

void ABlasterCharacter::PlayHitReactMontage()
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HitReactMontage)
	{
		AnimInstance->Montage_Play(HitReactMontage);
		FName SectionName("FromFront");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::GrenadeButtonPressed()
{
	if (Combat)
	{
		if (Combat->bHoldingTheFlag) return;
		Combat->ThrowGrenade();
	}
}

void ABlasterCharacter::ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser)
{
	BlasterGameMode = BlasterGameMode == nullptr ? GetWorld()->GetAuthGameMode<ABlasterGameMode>() : BlasterGameMode;
	if (bElimmed || BlasterGameMode == nullptr) return;
	Damage = BlasterGameMode->CalculateDamage(InstigatorController, Controller, Damage);
	
	float DamageToHealth = Damage;
	if (Shield > 0.f)
	{
		if (Shield >= Damage)
		{
			Shield = FMath::Clamp(Shield - Damage, 0.f, MaxShield);
			DamageToHealth = 0.f;
		}
		else
		{
			DamageToHealth = FMath::Clamp(DamageToHealth - Shield, 0.f, Damage);
			Shield = 0.f;
		}
	}

	Health = FMath::Clamp(Health - DamageToHealth, 0.f, MaxHealth);

	UpdateHUDHealth();
	UpdateHUDShield();
	PlayHitReactMontage();
	
	if (bBeingPunished) {
		Client_PlayHitCameraShake(HitCameraShakeScale);
	}
	

	if (Health == 0.f)
	{		
		if (BlasterGameMode)
		{
			BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
			ABlasterPlayerController* AttackerController = Cast<ABlasterPlayerController>(InstigatorController);
			BlasterGameMode->PlayerEliminated(this, BlasterPlayerController, AttackerController);
		}
	}
}

void ABlasterCharacter::MoveForward(float Value)
{
	if (bDisableGameplay || bBeingPunished || bEndingInProgress) return;
	if (Controller != nullptr && Value != 0.f)
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X));
		AddMovementInput(Direction, Value);
	}
}

void ABlasterCharacter::MoveRight(float Value)
{
	if (bDisableGameplay || bBeingPunished || bEndingInProgress) return;
	if (Controller != nullptr && Value != 0.f)
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y));
		AddMovementInput(Direction, Value);
	}
}

void ABlasterCharacter::Turn(float Value)
{
	if (bBeingPunished) return; // 처벌 중에는 회전 불가
	AddControllerYawInput(Value);
}

void ABlasterCharacter::LookUp(float Value)
{
    if (bDisableGameplay || bBeingPunished) return;
    AddControllerPitchInput(Value);
}

void ABlasterCharacter::EquipButtonPressed()
{
	if (bDisableGameplay) return;
	if (Combat)
	{
		if (Combat->bHoldingTheFlag) return;
		if (Combat->CombatState == ECombatState::ECS_Unoccupied) ServerEquipButtonPressed();
		bool bSwap = Combat->ShouldSwapWeapons() &&
			!HasAuthority() &&
			Combat->CombatState == ECombatState::ECS_Unoccupied &&
			OverlappingWeapon == nullptr;

		if (bSwap)
		{
			PlaySwapMontage();
			Combat->CombatState = ECombatState::ECS_SwappingWeapons;
			bFinishedSwapping = false;
		}
	}
}

void ABlasterCharacter::ServerEquipButtonPressed_Implementation()
{
	if (Combat)
	{
		if (OverlappingWeapon)
		{
			Combat->EquipWeapon(OverlappingWeapon);
		}
		else if (Combat->ShouldSwapWeapons())
		{
			Combat->SwapWeapons();
		}
	}
}

void ABlasterCharacter::CrouchButtonPressed()
{
	if (Combat && Combat->bHoldingTheFlag) return;
	if (bDisableGameplay) return;
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void ABlasterCharacter::ReloadButtonPressed()
{
	if (Combat && Combat->bHoldingTheFlag) return;
	if (bDisableGameplay) return;
	if (Combat)
	{
		Combat->Reload();
	}
}

void ABlasterCharacter::AimButtonPressed()
{
	if (Combat && Combat->bHoldingTheFlag) return;
	if (bDisableGameplay) return;
	if (Combat)
	{
		Combat->SetAiming(true);
	}
}

void ABlasterCharacter::AimButtonReleased()
{
	if (bDisableGameplay) return;
	if (Combat)
	{
		Combat->SetAiming(false);
	}
}

float ABlasterCharacter::CalculateSpeed()
{
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	return Velocity.Size();
}

void ABlasterCharacter::AimOffset(float DeltaTime)
{
	if (Combat && Combat->EquippedWeapon == nullptr) return;
	float Speed = CalculateSpeed();
	bool bIsInAir = GetCharacterMovement()->IsFalling();

	if (Speed == 0.f && !bIsInAir) // standing still, not jumping
	{
		bRotateRootBone = true;
		FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;
		if (TurningInPlace == ETurningInPlace::ETIP_NotTurning)
		{
			InterpAO_Yaw = AO_Yaw;
		}
		bUseControllerRotationYaw = true;
		TurnInPlace(DeltaTime);
	}
	if (Speed > 0.f || bIsInAir) // running, or jumping
	{
		bRotateRootBone = false;
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		AO_Yaw = 0.f;
		bUseControllerRotationYaw = true;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}

	CalculateAO_Pitch();
}

void ABlasterCharacter::CalculateAO_Pitch()
{
	AO_Pitch = GetBaseAimRotation().Pitch;
	if (AO_Pitch > 90.f && !IsLocallyControlled())
	{
		// map pitch from [270, 360) to [-90, 0)
		FVector2D InRange(270.f, 360.f);
		FVector2D OutRange(-90.f, 0.f);
		AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
	}
}

void ABlasterCharacter::SimProxiesTurn()
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;
	bRotateRootBone = false;
	float Speed = CalculateSpeed();
	if (Speed > 0.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}

	ProxyRotationLastFrame = ProxyRotation;
	ProxyRotation = GetActorRotation();
	ProxyYaw = UKismetMathLibrary::NormalizedDeltaRotator(ProxyRotation, ProxyRotationLastFrame).Yaw;

	if (FMath::Abs(ProxyYaw) > TurnThreshold)
	{
		if (ProxyYaw > TurnThreshold)
		{
			TurningInPlace = ETurningInPlace::ETIP_Right;
		}
		else if (ProxyYaw < -TurnThreshold)
		{
			TurningInPlace = ETurningInPlace::ETIP_Left;
		}
		else
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		}
		return;
	}
	TurningInPlace = ETurningInPlace::ETIP_NotTurning;

}

void ABlasterCharacter::Jump()
{
	if (Combat && Combat->bHoldingTheFlag) return;
	if (bDisableGameplay) return;
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Super::Jump();
	}
}

void ABlasterCharacter::FireButtonPressed()
{
	if (Combat && Combat->bHoldingTheFlag) return;
	if (bDisableGameplay) return;
	if (Combat)
	{
		Combat->FireButtonPressed(true);
	}
}

void ABlasterCharacter::FireButtonReleased()
{
	if (Combat && Combat->bHoldingTheFlag) return;
	if (bDisableGameplay) return;
	if (Combat)
	{
		Combat->FireButtonPressed(false);
	}
}

void ABlasterCharacter::TurnInPlace(float DeltaTime)
{
	if (AO_Yaw > 90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Right;
	}
	else if (AO_Yaw < -90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Left;
	}
	if (TurningInPlace != ETurningInPlace::ETIP_NotTurning)
	{
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 4.f);
		AO_Yaw = InterpAO_Yaw;
		if (FMath::Abs(AO_Yaw) < 15.f)
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		}
	}
}

void ABlasterCharacter::InventoryInteractionButtonPressed()
{
	if (bDisableGameplay) return;
	// Enhanced input inventory action now routed to OnInventoryAction
	OnInventoryAction(FInputActionValue());
}

void ABlasterCharacter::EquipInventoryItem(int32 SlotIndex)
{
	if (bDisableGameplay) return;
	
	// Equip item from inventory (inventory interaction key is separate)
	if (PlayerInventoryComponent)
	{
		PlayerInventoryComponent->EquipItemFromInventory(SlotIndex);
	}
}

// Enhanced Input: Inventory slot handlers
void ABlasterCharacter::OnInventorySlotOne(const FInputActionValue& Value)
{
	EquipInventoryItem(0);
}

void ABlasterCharacter::OnInventorySlotTwo(const FInputActionValue& Value)
{
	EquipInventoryItem(1);
}

void ABlasterCharacter::OnInventorySlotThree(const FInputActionValue& Value)
{
	EquipInventoryItem(2);
}

void ABlasterCharacter::OnInventorySlotFour(const FInputActionValue& Value)
{
	EquipInventoryItem(3);
}

void ABlasterCharacter::OnInventorySlotFive(const FInputActionValue& Value)
{
	EquipInventoryItem(4);
}

void ABlasterCharacter::OnInventorySlotSix(const FInputActionValue& Value)
{
	EquipInventoryItem(5);
}

void ABlasterCharacter::OnInventorySlotSeven(const FInputActionValue& Value)
{
	EquipInventoryItem(6);
}

void ABlasterCharacter::OnInventorySlotEight(const FInputActionValue& Value)
{
	EquipInventoryItem(7);
}

void ABlasterCharacter::OnInventorySlotNine(const FInputActionValue& Value)
{
	EquipInventoryItem(8);
}

void ABlasterCharacter::OnInventoryAction(const FInputActionValue& Value)
{
    if (bDisableGameplay) return;

    if (!InteractionComponent || !PlayerInventoryComponent)
    {
        return;
    }

    // If carrying a parcel, put into inventory (auto slot)
    if (AParcelActor* Carried = InteractionComponent->GetCarriedParcel())
    {
        if (PlayerInventoryComponent->PutCarriedParcelIntoInventory(-1))
        {
            InteractionComponent->SetCarriedParcel(nullptr);
        }
        return;
    }

    // If not carrying, nothing to do here (taking from inventory uses slot keys)
}

void ABlasterCharacter::UpdateMovementSpeedFromCarriedParcel()
{
	if (!InteractionComponent || !GetCharacterMovement())
	{
		return;
	}

	// Get base speed from Buff component or use default
	float BaseSpeed = 600.0f; // Default walk speed
	if (Buff && Buff->InitialBaseSpeed > 0.0f)
	{
		BaseSpeed = Buff->InitialBaseSpeed;
	}
	
	// Cache original speed if not already cached
	if (CachedOriginalMaxWalkSpeed < 0.0f)
	{
		CachedOriginalMaxWalkSpeed = BaseSpeed;
	}

	// Calculate total weight from carried parcel and inventory
	float TotalWeight = 0.0f;
	
	// Weight from carried parcel
	AParcelActor* CarriedParcel = InteractionComponent->GetCarriedParcel();
	if (CarriedParcel)
	{
		if (UParcelStateComponent* ParcelState = CarriedParcel->GetParcelStateComponent())
		{
			const FParcelState& State = ParcelState->GetParcelState();
			TotalWeight += State.Weight;
		}
	}

	// Weight from inventory parcels
	if (PlayerInventoryComponent)
	{
		const TArray<AParcelActor*>& InventoryItems = PlayerInventoryComponent->GetCollectedItems();
		for (AParcelActor* Item : InventoryItems)
		{
			if (Item && Item->GetParcelStateComponent())
			{
				const FParcelState& State = Item->GetParcelStateComponent()->GetParcelState();
				TotalWeight += State.Weight;
			}
		}
	}

	// Calculate speed multiplier based on total weight
	// Use same logic as ParcelStateComponent::GetMovementSpeedMultiplier()
	float SpeedMultiplier = 1.0f;
	
	if (TotalWeight > 0.0f)
	{
		// Use same weight thresholds as ParcelStateComponent
		const float MediumWeightThreshold = 10.0f;
		const float HeavyWeightThreshold = 20.0f;
		const float MediumMovementPenalty = 0.9f; // 10% reduction
		const float HeavyMovementPenalty = 0.75f; // 25% reduction
		
		// Calculate base multiplier from total weight
		if (TotalWeight >= HeavyWeightThreshold)
		{
			SpeedMultiplier = HeavyMovementPenalty;
		}
		else if (TotalWeight >= MediumWeightThreshold)
		{
			SpeedMultiplier = MediumMovementPenalty;
		}
		
		// If carrying a parcel with two-person carry bonus, apply it
		if (CarriedParcel && CarriedParcel->GetParcelStateComponent())
		{
			UParcelStateComponent* ParcelState = CarriedParcel->GetParcelStateComponent();
			
			// Check if two-person carry is active (this affects the carried parcel's multiplier)
			// GetEffectiveMovementSpeedMultiplier includes two-person bonus
			float CarriedParcelBaseMultiplier = ParcelState->GetMovementSpeedMultiplier();
			float CarriedParcelEffectiveMultiplier = ParcelState->GetEffectiveMovementSpeedMultiplier();
			
			// If two-person carry is active, the effective multiplier is better
			// We need to apply this bonus to the total weight calculation
			if (CarriedParcelEffectiveMultiplier > CarriedParcelBaseMultiplier)
			{
				// Two-person carry is active, apply bonus
				// The bonus improves movement speed, so we adjust the multiplier upward
				float TwoPersonBonus = CarriedParcelEffectiveMultiplier / CarriedParcelBaseMultiplier;
				SpeedMultiplier *= TwoPersonBonus;
			}
		}
	}

	// Apply speed multiplier to base speed
	float NewSpeed = CachedOriginalMaxWalkSpeed * SpeedMultiplier;
	GetCharacterMovement()->MaxWalkSpeed = NewSpeed;
	GetCharacterMovement()->MaxWalkSpeedCrouched = NewSpeed * 0.5f; // Crouch speed is typically half
}

void ABlasterCharacter::HideCameraIfCharacterClose()
{
	if (!IsLocallyControlled()) return;
	if (PerspectiveSettings.Perspective == EPerspective::EPT_FirstPerson) return;

	const float DistanceToCamera = (FollowCamera->GetComponentLocation() - GetActorLocation()).Size();
	
	if ((FollowCamera->GetComponentLocation() - GetActorLocation()).Size() < CameraThreshold)
	{
		GetMesh()->SetVisibility(false);
		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
		{
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = true;
		}
		if (Combat && Combat->SecondaryWeapon && Combat->SecondaryWeapon->GetWeaponMesh())
		{
			Combat->SecondaryWeapon->GetWeaponMesh()->bOwnerNoSee = true;
		}
	}
	else
	{
		GetMesh()->SetVisibility(true);
		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
		{
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = false;
		}
		if (Combat && Combat->SecondaryWeapon && Combat->SecondaryWeapon->GetWeaponMesh())
		{
			Combat->SecondaryWeapon->GetWeaponMesh()->bOwnerNoSee = false;
		}
	}

}

void ABlasterCharacter::HideCarriedCameraIfCharacterClose()
{
	if (!IsLocallyControlled()) return;
	if (!InteractionComponent) return;
	
	AParcelActor* CarriedParcel = InteractionComponent->GetCarriedParcel();
	if (!CarriedParcel || !CarriedParcel->IsAttached()) return;

	
	const FVector CameraLoc = FollowCamera->GetComponentLocation();
	float DistanceToParcel = 0.f;

	if (UStaticMeshComponent* ParcelMesh = CarriedParcel->GetParcelMesh())
	{
		const FBoxSphereBounds B = ParcelMesh->CalcBounds(ParcelMesh->GetComponentTransform());
		DistanceToParcel = FVector::Dist(CameraLoc, B.Origin);
	}
	else
	{
		DistanceToParcel = FVector::Dist(CameraLoc, CarriedParcel->GetActorLocation());
	}
	const bool bShouldHideParcel = DistanceToParcel < CarriedHideCameraThreshold;

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	CarriedParcel->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	
	for (UPrimitiveComponent* Prim : PrimitiveComponents)
	{
		if (!Prim) continue;

		if (bShouldHideParcel)
		{
			Prim->SetOwnerNoSee(true);
		}
		else
		{
			Prim->SetOwnerNoSee(false);
		}
	}
}

void ABlasterCharacter::ToggleHeadMesh(bool bHideHeadMesh)
{
    if (IsLocallyControlled())
    {
        if (USkeletalMeshComponent* SkeletalMesh = GetMesh())
        {
            if (bHideHeadMesh) {
                auto HideIfExists = [SkeletalMesh](FName BoneName)
                    {
                        if (SkeletalMesh->GetBoneIndex(BoneName) != INDEX_NONE)
                        {
                            // hide rendering of the bone
                            SkeletalMesh->HideBoneByName(BoneName, EPhysBodyOp::PBO_None);
                        }
                    };
                HideIfExists(FName("head"));
                HideIfExists(FName("hair_front"));
                HideIfExists(FName("hair_back"));
                HideIfExists(FName("neck_01"));

                SkeletalMesh->SetCastHiddenShadow(true);
            }
            else if (!bHideHeadMesh)
            {
                auto UnhideIfExists = [SkeletalMesh](FName BoneName)
                    {
                        if (SkeletalMesh->GetBoneIndex(BoneName) != INDEX_NONE)
                        {
                            // unhide rendering of the bone
                            SkeletalMesh->UnHideBoneByName(BoneName);
                        }
                    };
                UnhideIfExists(FName("head"));
                UnhideIfExists(FName("hair_front"));
                UnhideIfExists(FName("hair_back"));
                UnhideIfExists(FName("neck_01"));
                SkeletalMesh->SetCastHiddenShadow(false);
            }
        }
    }
}

void ABlasterCharacter::RefreshDebugCollisionVisibility()
{
    ApplyDebugCollisionVisibility();
}

void ABlasterCharacter::ApplyDebugCollisionVisibility()
{
    auto ApplyVisibility = [](UPrimitiveComponent* Component, bool bShouldShow)
    {
        if (!Component) return;
        Component->SetHiddenInGame(!bShouldShow);
        Component->SetVisibility(bShouldShow, true);
    };

    const bool bDefaultShow = DebugCollisionVisibility.bShowHitCollisionBoxes;

    for (TPair<FName, UBoxComponent*>& Pair : HitCollisionBoxes)
    {
        UBoxComponent* BoxComponent = Pair.Value;
        if (!BoxComponent) continue;

        bool bShouldShow = bDefaultShow;
        if (const bool* Override = DebugCollisionVisibility.ComponentVisibilities.Find(Pair.Key))
        {
            bShouldShow = *Override;
        }

        ApplyVisibility(BoxComponent, bShouldShow);
    }

    for (const TPair<FName, bool>& OverridePair : DebugCollisionVisibility.ComponentVisibilities)
    {
        if (HitCollisionBoxes.Contains(OverridePair.Key))
        {
            continue;
        }

        if (UPrimitiveComponent* OverrideComponent = FindDebugPrimitiveByName(OverridePair.Key))
        {
            ApplyVisibility(OverrideComponent, OverridePair.Value);
        }
    }
}

UPrimitiveComponent* ABlasterCharacter::FindDebugPrimitiveByName(const FName ComponentName) const
{
    if (ComponentName.IsNone())
    {
        return nullptr;
    }

    if (UBoxComponent* const* HitBox = HitCollisionBoxes.Find(ComponentName))
    {
        return *HitBox;
    }

    TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
    GetComponents(PrimitiveComponents);

    for (UPrimitiveComponent* Component : PrimitiveComponents)
    {
        if (Component && Component->GetFName() == ComponentName)
        {
            return Component;
        }
    }

    return nullptr;
}

bool ABlasterCharacter::IsPerspectiveFirstPerson() const
{
    return PerspectiveSettings.Perspective == EPerspective::EPT_FirstPerson;
}

bool ABlasterCharacter::IsPerspectiveThirdPersonLike() const
{
    return PerspectiveSettings.Perspective == EPerspective::EPT_ThirdPerson ||
        PerspectiveSettings.Perspective == EPerspective::EPT_Sequence;
}


void ABlasterCharacter::OnRep_Health(float LastHealth)
{
	UpdateHUDHealth();
	if (Health < LastHealth)
	{
		PlayHitReactMontage();
	}
}

void ABlasterCharacter::OnRep_Shield(float LastShield)
{
	UpdateHUDShield();
	if (Shield < LastShield)
	{
		PlayHitReactMontage();
	}
}

void ABlasterCharacter::UpdateHUDHealth()
{
	BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if (BlasterPlayerController)
	{
		BlasterPlayerController->SetHUDHealth(Health, MaxHealth);
	}
}

void ABlasterCharacter::UpdateHUDShield()
{
	BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if (BlasterPlayerController)
	{
		BlasterPlayerController->SetHUDShield(Shield, MaxShield);
	}
}

void ABlasterCharacter::UpdateHUDAmmo()
{
	BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if (BlasterPlayerController && Combat && Combat->EquippedWeapon)
	{
		BlasterPlayerController->SetHUDCarriedAmmo(Combat->CarriedAmmo);
		BlasterPlayerController->SetHUDWeaponAmmo(Combat->EquippedWeapon->GetAmmo());
	}
}

void ABlasterCharacter::SpawnDefaultWeapon()
{
	BlasterGameMode = BlasterGameMode == nullptr ? GetWorld()->GetAuthGameMode<ABlasterGameMode>() : BlasterGameMode;
	UWorld* World = GetWorld();
	if (BlasterGameMode && World && !bElimmed && DefaultWeaponClass)
	{
		AWeapon* StartingWeapon = World->SpawnActor<AWeapon>(DefaultWeaponClass);
		StartingWeapon->bDestroyWeapon = true;
		if (Combat)
		{
			Combat->EquipWeapon(StartingWeapon);
		}
	}
}

void ABlasterCharacter::PollInit()
{
	if (BlasterPlayerState == nullptr)
	{
		BlasterPlayerState = GetPlayerState<ABlasterPlayerState>();
		if (BlasterPlayerState)
		{
			OnPlayerStateInitialized();
			
			ABlasterGameState* BlasterGameState = Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this));
			if (BlasterGameState && BlasterGameState->TopScoringPlayers.Contains(BlasterPlayerState)) 
			{
				MulticastGainedTheLead();
			}
		}
	}
}

void ABlasterCharacter::UpdateDissolveMaterial(float DissolveValue)
{
	if (DynamicDissolveMaterialInstance)
	{
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), DissolveValue);
	}
}

void ABlasterCharacter::StartDissolve()
{
	DissolveTrack.BindDynamic(this, &ABlasterCharacter::UpdateDissolveMaterial);
	if (DissolveCurve && DissolveTimeline)
	{
		DissolveTimeline->AddInterpFloat(DissolveCurve, DissolveTrack);
		DissolveTimeline->Play();
	}
}

void ABlasterCharacter::SetOverlappingWeapon(AWeapon* Weapon)
{
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(false);
	}
	OverlappingWeapon = Weapon;
	if (IsLocallyControlled())
	{
		if (OverlappingWeapon)
		{
			OverlappingWeapon->ShowPickupWidget(true);
		}
	}
}

void ABlasterCharacter::OnRep_OverlappingWeapon(AWeapon* LastWeapon)
{
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}
	if (LastWeapon)
	{
		LastWeapon->ShowPickupWidget(false);
	}
}

bool ABlasterCharacter::IsWeaponEquipped()
{
	return (Combat && Combat->EquippedWeapon);
}

bool ABlasterCharacter::IsAiming()
{
	return (Combat && Combat->bAiming);
}

AWeapon* ABlasterCharacter::GetEquippedWeapon()
{
	if (Combat == nullptr) return nullptr;
	return Combat->EquippedWeapon;
}

FVector ABlasterCharacter::GetHitTarget() const
{
	if (Combat == nullptr) return FVector();
	return Combat->HitTarget;
}

ECombatState ABlasterCharacter::GetCombatState() const
{
	if (Combat == nullptr) return ECombatState::ECS_MAX;
	return Combat->CombatState;
}

bool ABlasterCharacter::IsLocallyReloading()
{
	if (Combat == nullptr) return false;
	return Combat->bLocallyReloading;
}

bool ABlasterCharacter::IsHoldingTheFlag() const
{
	if (Combat == nullptr) return false;
	return Combat->bHoldingTheFlag;	
}

ETeam ABlasterCharacter::GetTeam()
{
	BlasterPlayerState = BlasterPlayerState == nullptr ? GetPlayerState<ABlasterPlayerState>() : BlasterPlayerState;
	if (BlasterPlayerState == nullptr) return ETeam::ET_NoTeam;
	return BlasterPlayerState->GetTeam();	
}

void ABlasterCharacter::SetHoldingTheFlag(bool bHoldingFlag)
{
	if (Combat == nullptr) return;
	Combat->bHoldingTheFlag = bHoldingFlag;
}

void ABlasterCharacter::SetSuspiciousBehavior(ESuspiciousBehavior Behavior)
{
	if (!HasAuthority())
	{
		return;
	}

	ESuspiciousBehavior OldBehavior = CurrentSuspiciousBehavior;
	CurrentSuspiciousBehavior = Behavior;

	if (OldBehavior != Behavior)
	{
		UE_LOG(LogTemp, Log, TEXT("[BlasterCharacter] Suspicious behavior changed: %s -> %s"), 
			*UEnum::GetValueAsString(OldBehavior), *UEnum::GetValueAsString(Behavior));
		
		OnRep_CurrentSuspiciousBehavior(OldBehavior);
	}
}

void ABlasterCharacter::UpdateSuspiciousBehavior(ESuspiciousBehavior NewBehavior)
{
	if (NewBehavior == ESuspiciousBehavior::None)
	{
		return;
	}

	// SuspicionManager에 이벤트 브로드캐스트 (같은 행동이어도 시간이 지나면 다시 감지되도록)
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (USuspicionManagerSubsystem* SuspicionManager = GameInstance->GetSubsystem<USuspicionManagerSubsystem>())
			{
				SuspicionManager->BroadcastSuspicionEvent(this, NewBehavior);
				UE_LOG(LogTemp, Log, TEXT("[BlasterCharacter] Broadcasted suspicion event: %s"), 
					*UEnum::GetValueAsString(NewBehavior));
			}
		}
	}

	// 상태는 항상 업데이트 (같은 행동이어도 시간 갱신)
	SetSuspiciousBehavior(NewBehavior);
}

void ABlasterCharacter::OnRep_CurrentSuspiciousBehavior(ESuspiciousBehavior OldBehavior)
{
	// 클라이언트에서 의심 행동 상태 변경 알림
	if (CurrentSuspiciousBehavior != ESuspiciousBehavior::None)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("[BlasterCharacter] Client: Suspicious behavior replicated: %s"), 
			*UEnum::GetValueAsString(CurrentSuspiciousBehavior));
	}
}

void ABlasterCharacter::SetBeingPunished(bool bPunishing, AActor* Punisher)
{
	if (!HasAuthority())
	{
		return;
	}

	this->bBeingPunished = bPunishing;
	Multicast_SetBeingPunished(bBeingPunished, Punisher);
}

void ABlasterCharacter::Multicast_SetBeingPunished_Implementation(bool bPunishing, AActor* Punisher)
{
	this->bBeingPunished = bPunishing;

	if (bBeingPunished && Punisher)
	{
		// 움직임 멈추기
		GetCharacterMovement()->DisableMovement();
		GetCharacterMovement()->StopMovementImmediately();
		
		// 입력 비활성화
		bDisableGameplay = true;
		
		// Controller 회전 비활성화
		if (Controller)
		{
			Controller->SetIgnoreLookInput(true);
			Controller->SetIgnoreMoveInput(true);
		}

		// Mother AI를 향하도록 캐릭터 회전 (즉시)
		FVector ToPunisher = (Punisher->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		ToPunisher.Z = 0.0f; // 수평 회전만
		
		if (!ToPunisher.IsNearlyZero())
		{
			FRotator TargetRotation = ToPunisher.Rotation();
			SetActorRotation(TargetRotation);
		}

		// 카메라 회전 설정 (부드럽게)
		PunisherActor = Punisher;
		bShouldRotateCameraToPunisher = true;
		bCameraRotationComplete = false;

		// 목표 카메라 회전 계산 (Mother AI를 바라보도록)
		if (Punisher && Controller)
		{
			FVector CameraLocation = FollowCamera ? FollowCamera->GetComponentLocation() : GetActorLocation();
			FVector ToPunisherFromCamera = (Punisher->GetActorLocation() - CameraLocation).GetSafeNormal();
			TargetCameraRotation = ToPunisherFromCamera.Rotation();
		}

		UE_LOG(LogTemp, Warning, TEXT("[BlasterCharacter] Player %s is being punished, starting camera rotation to face %s"), 
			*GetName(), *Punisher->GetName());
	}
	else
	{
		// 카메라 회전 초기화
		bShouldRotateCameraToPunisher = false;
		bCameraRotationComplete = false;
		PunisherActor = nullptr;

		// 움직임 복구 - 명시적으로 활성화
		if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
		{
			MovementComp->SetMovementMode(MOVE_Walking);
			MovementComp->SetComponentTickEnabled(true);
		}

		// 입력 활성화
		bDisableGameplay = false;

		// Controller 회전 활성화 - 명시적으로 리셋
		if (Controller)
		{
			Controller->ResetIgnoreLookInput();
			Controller->ResetIgnoreMoveInput();

			// 추가 보장: 입력 무시 플래그 강제 해제
			Controller->SetIgnoreLookInput(false);
			Controller->SetIgnoreMoveInput(false);
		}

		UE_LOG(LogTemp, Log, TEXT("[BlasterCharacter] Player %s punishment ended, movement restored"), *GetName());
	}
}

void ABlasterCharacter::RotateCameraToPunisher(float DeltaTime)
{
	if (!bShouldRotateCameraToPunisher || !PunisherActor || !Controller)
	{
		return;
	}

	// 현재 카메라 회전
	FRotator CurrentRotation = Controller->GetControlRotation();

	// 목표 회전 업데이트 (Punisher가 움직일 수 있으므로)
	FVector CameraLocation = FollowCamera ? FollowCamera->GetComponentLocation() : GetActorLocation();
	FVector ToPunisherFromCamera = (PunisherActor->GetActorLocation() - CameraLocation).GetSafeNormal();
	TargetCameraRotation = ToPunisherFromCamera.Rotation();

	// 부드럽게 회전
	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetCameraRotation, DeltaTime, CameraRotationSpeed);
	Controller->SetControlRotation(NewRotation);

	// 회전 완료 확인 (목표 회전과의 차이가 작으면 완료)
	FRotator DeltaRotation = (TargetCameraRotation - NewRotation).GetNormalized();
	float YawDiff = FMath::Abs(DeltaRotation.Yaw);
	float PitchDiff = FMath::Abs(DeltaRotation.Pitch);

	if (YawDiff < 5.0f && PitchDiff < 5.0f && !bCameraRotationComplete)
	{
		bCameraRotationComplete = true;
		Controller->SetControlRotation(TargetCameraRotation); // 정확한 위치로 설정
		UE_LOG(LogTemp, Log, TEXT("[BlasterCharacter] Camera rotation to punisher completed"));
	}
}
