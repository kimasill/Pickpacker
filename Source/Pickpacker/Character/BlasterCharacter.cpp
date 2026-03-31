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
#include "Blaster/HUD/OverHeadWidget.h"
#include "Blaster/GameMode/LobbyGameMode.h"
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
#include "Blaster/GameState/LobbyGameState.h"
#include "Blaster/Environment/DeathLocationActor.h"
#include "Components/InputComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimInstance.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Blaster/Components/ParcelStateComponent.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/AI/MotherAIActor.h"
#include "Blaster/Environment/ConveyorBeltActor.h"
#include "Blaster/Environment/LadderActor.h"

namespace
{
	static void AppendDebugLog_BlasterCharacter(const FString& JsonLine)
	{
#if !UE_BUILD_SHIPPING
		const FString LogDir = FPaths::ProjectSavedDir() + TEXT("Logs/BlasterDebug.log");
		FFileHelper::SaveStringToFile(JsonLine + LINE_TERMINATOR, *LogDir, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
#endif
	}
}

ABlasterCharacter::ABlasterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 1인칭 카메라 Capsule에 직접 부착 (SpringArm 제거)
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

	// Shadow-only head proxy (hidden, but casts shadow)
	HeadShadowProxy = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeadShadowProxy"));
	HeadShadowProxy->SetupAttachment(GetMesh());
	HeadShadowProxy->SetLeaderPoseComponent(GetMesh());
	HeadShadowProxy->SetHiddenInGame(true);
	HeadShadowProxy->SetCastHiddenShadow(true);
	HeadShadowProxy->SetCastShadow(true);
	HeadShadowProxy->SetVisibility(true, true);
	HeadShadowProxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadShadowProxy->SetComponentTickEnabled(false);
	HeadShadowProxy->bOwnerNoSee = false;
	HeadShadowProxy->bOnlyOwnerSee = false;

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

	EffectSocketBody = CreateDefaultSubobject<USceneComponent>(TEXT("EffectSocketBody"));
	EffectSocketBody->SetupAttachment(GetMesh(), FName("spine_02"));

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
	DOREPLIFETIME(ABlasterCharacter, bOutOfLives);
	DOREPLIFETIME(ABlasterCharacter, bIsOnLadder);
	DOREPLIFETIME(ABlasterCharacter, CurrentLadder);
	DOREPLIFETIME(ABlasterCharacter, LadderCurrentDistance);
}

void ABlasterCharacter::OnRep_ReplicatedMovement()
{
	Super::OnRep_ReplicatedMovement();
	SimProxiesTurn();
	TimeSinceLastMovementReplication = 0.f;
}

void ABlasterCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	UpdateOverheadWidget();
	// #region agent log
	AppendDebugLog_BlasterCharacter(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H45\",\"location\":\"BlasterCharacter.cpp:236\",\"message\":\"OnRep_PlayerState\",\"data\":{\"name\":\"%s\",\"world\":\"%s\"},\"timestamp\":%lld}"),
		GetPlayerState() ? *GetPlayerState()->GetPlayerName() : TEXT("none"),
		GetWorld() ? *GetWorld()->GetMapName() : TEXT("none"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
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

	// #region agent log
	AppendDebugLog_BlasterCharacter(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H42\",\"location\":\"BlasterCharacter.cpp:242\",\"message\":\"SetEndingInProgress\",\"data\":{\"inProgress\":%s,\"world\":\"%s\",\"movementMode\":%d},\"timestamp\":%lld}"),
		bInProgress ? TEXT("true") : TEXT("false"),
		GetWorld() ? *GetWorld()->GetMapName() : TEXT("none"),
		GetCharacterMovement() ? static_cast<int32>(GetCharacterMovement()->MovementMode) : -1,
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion

	if (bEndingInProgress && GetCharacterMovement())
	{
		GetCharacterMovement()->DisableMovement();
	}
	else if (GetCharacterMovement())
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
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
	// 로컬 컨트롤러에서 실행
	ABlasterPlayerController* PC = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if (!PC || !CameraShakeClass) return;
	
	PC->ClientStartCameraShake(CameraShakeClass, Scale);
}

void ABlasterCharacter::Client_PlayPunishmentCameraShake_Implementation()
{
	Client_PlayHitCameraShake(PunishmentCameraShakeScale);
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
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		OnlineSessionInterface = OnlineSub->GetSessionInterface();
		if(GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1, 
				5.f, 
				FColor::Blue, 
				FString::Printf(TEXT("OnlineSubsystem: %s"), *OnlineSub->GetSubsystemName().ToString())
				);
		}
	}

	ToggleHeadMesh(IsPerspectiveFirstPerson());
	ApplyDebugCollisionVisibility();
	
	// 1인칭 카메라 EyeHeight 설정
	if (FollowCamera)
	{
		FollowCamera->SetRelativeLocation(FVector(0.f, 0.f, EyeHeight));
	}
	
	// Mesh 회전 설정: Mesh는 Controller 회전 플래그를 쓰지 않으므로 World/Relative 회전 좌표 기본 사용
	if (GetMesh())
	{
		// SkeletalMeshComponent는 컨트롤러 회전 플래그를 쓰므로 World/Relative 회전 좌표 기본 사용
		GetMesh()->SetUsingAbsoluteRotation(false);
	}
 	
 	// Reset cached speed on begin play

	// OverHeadWidget 업데이트 (로비에서 플레이어 이름/준비 상태 표시)
	// 시간이 지날 때까지 기다려 PlayerState가 회전 초기화된 후 업데이트
	FTimerHandle TempHandle;
	GetWorld()->GetTimerManager().SetTimer(
		TempHandle,
		this,
		&ABlasterCharacter::UpdateOverheadWidget,
		0.1f,
		false
	);
}

void ABlasterCharacter::UpdateOverheadWidget()
{
	if (!OverHeadWidget)
	{
		return;
	}

	// 자기 자신은 머리 위 이름 표시 안 함 (SetOwnerNoSee는 WidgetComponent에서 불안정)
	OverHeadWidget->SetHiddenInGame(IsLocallyControlled());

	UOverHeadWidget* Widget = Cast<UOverHeadWidget>(OverHeadWidget->GetWidget());
	if (!Widget)
	{
		return;
	}

	APlayerState* PS = GetPlayerState();
	if (!PS)
	{
		return;
	}

	// 플레이어 이름 설정
	FString PlayerName = PS->GetPlayerName();
	if (PlayerName.IsEmpty())
	{
		PlayerName = FString::Printf(TEXT("플레이어%d"), PS->GetPlayerId());
	}

	// 준비 상태 확인
	bool bIsReady = false;
	const bool bInLobby = GetWorld() && GetWorld()->GetGameState<ALobbyGameState>() != nullptr;
	if (ABlasterPlayerState* BlasterPS = Cast<ABlasterPlayerState>(PS))
	{
		bIsReady = BlasterPS->IsReady();
	}
	else
	{
		// PlayerState 캐스팅 실패 시 기본값 적용 (로비 초기화 타이밍 보완)
		bIsReady = PS->IsOnlyASpectator() ? false : bIsReady;
	}

	// 위젯 업데이트
	Widget->SetPlayerName(PlayerName);
	if (bInLobby)
	{
		Widget->SetReadyStatus(bIsReady);
	}
	else
	{
		// 로비가 아니면 준비 표시 숨김
		if (Widget->ReadyStatusText)
		{
			Widget->ReadyStatusText->SetText(FText());
		}
	}
}

void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 스플라인 기반 사다리 클라이밍 (서버에서 실행, 발 기준)
	if (bIsOnLadder && HasAuthority())
	{
		// nullptr 선행 검사 (BP/멀티플레이어에서 CurrentLadder 단절 시 크래시 방지)
		if (!CurrentLadder || !IsValid(CurrentLadder))
		{
			StopLadder(false);
			return;
		}
		if (!CurrentLadder->HasValidSpline())
		{
			StopLadder(false);
			return;
		}
		const float SplineLength = CurrentLadder->GetSplineLength();
		const UCapsuleComponent* Capsule = GetCapsuleComponent();
		const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.f;

		if (SplineLength > KINDA_SMALL_NUMBER)
		{
			// 거리 업데이트 (발 기준이므로 바닥 별도 감지 불필요)
			LadderCurrentDistance += LadderInputAxis * LadderClimbSpeed * DeltaTime;
			LadderCurrentDistance = FMath::Clamp(LadderCurrentDistance, 0.f, SplineLength);

			// 스플라인 위치 = 발(캡슐 하단) 기준
			const FVector FeetSplineLocation = CurrentLadder->GetLocationAtDistanceAlongSpline(LadderCurrentDistance);
			const FVector ActorLocation = FeetSplineLocation + FVector(0.f, 0.f, CapsuleHalfHeight);

			SetActorLocation(ActorLocation, false);

			// 회전: 사다리 방향(벽 쪽)을 향하도록 GetLadderForwardVector 사용
			FVector Direction = CurrentLadder ? CurrentLadder->GetLadderForwardVector() : FVector(1.f, 0.f, 0.f);
			if (!Direction.IsNearlyZero())
			{
				const FRotator NewRotation(0.f, Direction.Rotation().Yaw, 0.f);
				SetActorRotation(NewRotation);
			}

			// 꼭대기/바닥 도달 시 이동만 멈춤 (탈출은 Jump 키로)
			if (LadderCurrentDistance >= SplineLength - LadderTopExitThreshold)
			{
				if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
				{
					MoveComp->StopMovementImmediately();
					LadderInputAxis = 0.f;
				}
			}
			else if (LadderCurrentDistance <= LadderBottomExitThreshold)
			{
				LadderInputAxis = 0.f;
			}
		}
	}

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

	// Enhanced Input으로 변환
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Movement action (2D Vector - WASD)
		if (MovementAction)
		{
			EnhancedInputComponent->BindAction(MovementAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::OnMovement);
			EnhancedInputComponent->BindAction(MovementAction, ETriggerEvent::Completed, this, &ABlasterCharacter::OnMovementCompleted);
			EnhancedInputComponent->BindAction(MovementAction, ETriggerEvent::Canceled, this, &ABlasterCharacter::OnMovementCompleted);
		}
		
		// Look action (2D Vector - Mouse X/Y)
		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::OnLook);
		}
		
		// Combat actions
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ABlasterCharacter::OnJumpAction);
		}
		if (EquipAction)
		{
			EnhancedInputComponent->BindAction(EquipAction, ETriggerEvent::Started, this, &ABlasterCharacter::OnEquipAction);
		}
		if (CrouchAction)
		{
			EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &ABlasterCharacter::OnCrouchAction);
		}
		if (ReloadAction)
		{
			EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &ABlasterCharacter::OnReloadAction);
		}
		if (AimAction)
		{
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &ABlasterCharacter::OnAimAction);
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &ABlasterCharacter::OnAimActionReleased);
		}
		if (FireAction)
		{
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &ABlasterCharacter::OnFireAction);
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &ABlasterCharacter::OnFireActionReleased);
		}
		if (GrenadeAction)
		{
			EnhancedInputComponent->BindAction(GrenadeAction, ETriggerEvent::Started, this, &ABlasterCharacter::OnGrenadeAction);
		}
		
		// Inventory actions
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
		DropCarriedParcelIfAny();
		Crouch();
	}
}

// Enhanced Input handlers
void ABlasterCharacter::OnMovement(const FInputActionValue& Value)
{
	// 2D Vector 입력 처리 (WASD)
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (bIsOnLadder && CurrentLadder)
	{
		LadderInputAxis = MovementVector.Y;
		ServerUpdateLadderInput(LadderInputAxis);
		return;
	}

	// 사다리 범위 안 + 미부착: 이동 입력으로는 부착되지 않음 (Jump 키로만 부착)
	LadderInputAxis = 0.f;
	// X축: 전후 이동 (W/S)
	MoveForward(MovementVector.Y);
	// Y축: 좌우 이동 (A/D)
	MoveRight(MovementVector.X);
}

void ABlasterCharacter::OnMovementCompleted(const FInputActionValue& Value)
{
	LadderInputAxis = 0.f;
	ServerUpdateLadderInput(0.f);
	if (bIsOnLadder)
	{
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
		}
	}
}

void ABlasterCharacter::SetCanClimbLadder(bool bInCanClimb, ALadderActor* Ladder)
{
	bCanClimb = bInCanClimb;
	CurrentLadder = bInCanClimb ? Ladder : nullptr;
}

void ABlasterCharacter::StartLadder(ALadderActor* LadderActor)
{
	if (!LadderActor || !LadderActor->bCanAccess)
	{
		return;
	}

	// 동일 사다리 재부착(트리거 안에 머문 경우)은 블록 생략
	if (GetWorld() && CurrentLadder != LadderActor)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - LastLadderExitTime < LadderReenterBlockSeconds)
		{
			return;
		}
	}

	if (!HasAuthority())
	{
		ServerStartLadder(LadderActor);
		return;
	}

	if (bIsOnLadder && CurrentLadder == LadderActor)
	{
		return;
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		CachedMovementMode = MoveComp->MovementMode;
		CachedCustomMovementMode = MoveComp->CustomMovementMode;
		CachedMaxFlySpeed = MoveComp->MaxFlySpeed;
		MoveComp->StopMovementImmediately();
		MoveComp->SetMovementMode(MOVE_Flying);
		MoveComp->MaxFlySpeed = LadderClimbSpeed;
		MoveComp->GravityScale = 0.f;
	}

	CurrentLadder = LadderActor;
	bIsOnLadder = true;
	LadderInputAxis = 0.f;

	// 스플라인 거리 기반: 발(캡슐 하단) 위치 기준으로 초기화
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.f;
	const FVector FeetLocation = GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
	LadderCurrentDistance = LadderActor->FindDistanceAlongSplineForWorldLocation(FeetLocation);
}

void ABlasterCharacter::StopLadder(bool bPlaceAtTop)
{
	if (!HasAuthority())
	{
		ServerStopLadder(bPlaceAtTop);
		return;
	}

	if (!bIsOnLadder)
	{
		return;
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->SetMovementMode(CachedMovementMode, CachedCustomMovementMode);
		MoveComp->GravityScale = 1.f;
		if (CachedMaxFlySpeed > 0.f)
		{
			MoveComp->MaxFlySpeed = CachedMaxFlySpeed;
		}
	}

	if (CurrentLadder)
	{
		if (bPlaceAtTop)
		{
			HandleTopExit();
		}
		else if (LadderCurrentDistance <= LadderBottomExitThreshold)
		{
			// 스플라인 끝: 바닥에 배치
			const FVector TraceStart = CurrentLadder->HasValidSpline()
				? CurrentLadder->GetLocationAtDistanceAlongSpline(LadderCurrentDistance)
				: (GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f));
			HandleBottomExit(TraceStart, false);
		}
	}

	bIsOnLadder = false;
	// CurrentLadder는 overlap End 시에만 초기화 (트리거 안에 있으면 재부착 가능)
	LadderInputAxis = 0.f;
	LadderCurrentDistance = 0.f;

	if (GetWorld())
	{
		LastLadderExitTime = GetWorld()->GetTimeSeconds();
	}
}

void ABlasterCharacter::HandleTopExit()
{
	if (!CurrentLadder || !GetWorld())
	{
		return;
	}

	// GetDirectionAtDistanceAlongSpline 대신 GetLadderUpVector/GetActorForwardVector 사용 (spline 크래시 방지)
	const float SplineLength = CurrentLadder->GetSplineLength();
	FVector EndLocation = CurrentLadder->HasValidSpline()
		? CurrentLadder->GetLocationAtDistanceAlongSpline(SplineLength)
		: CurrentLadder->GetTopLocation();
	FVector Forward = CurrentLadder->GetActorForwardVector();
	if (Forward.IsNearlyZero())
	{
		Forward = CurrentLadder->GetLadderUpVector();
	}
	FVector TargetLocation = EndLocation + (Forward * 50.f);

	// LineTrace: 위에서 아래로 바닥 체크
	const FVector TraceStart = TargetLocation + FVector(0.f, 0.f, 50.f);
	const FVector TraceEnd = TargetLocation - FVector(0.f, 0.f, 200.f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LadderTopExit), false, this);
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params);

	if (bHit && Hit.bBlockingHit)
	{
		// 캡슐 바닥이 바닥에 닿도록 오프셋
		float CapsuleHalfHeight = 0.f;
		if (const UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
		const FVector FinalLocation = Hit.ImpactPoint + FVector(0.f, 0.f, CapsuleHalfHeight);
		SetActorLocation(FinalLocation);
	}
	else
	{
		// 히트 없으면 TargetLocation 사용
		SetActorLocation(TargetLocation);
	}
}

void ABlasterCharacter::HandleBottomExit(const FVector& Point, bool bUseAsFloorPoint)
{
	if (!GetWorld())
	{
		return;
	}

	float CapsuleHalfHeight = 88.f;
	if (const UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	}

	FVector FloorPoint = Point;
	if (!bUseAsFloorPoint)
	{
		// Point에서 아래로 트레이스
		const FVector TraceStart = Point;
		const FVector TraceEnd = Point - FVector(0.f, 0.f, 200.f);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(LadderBottomExit), false, this);
		if (CurrentLadder)
		{
			Params.AddIgnoredActor(CurrentLadder);
		}
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
		{
			FloorPoint = Hit.ImpactPoint;
		}
	}

	// 사다리 하단 X,Y에 배치하여 트리거 안에 있도록 (재부착 가능)
	FVector FinalLocation = FloorPoint + FVector(0.f, 0.f, CapsuleHalfHeight);
	if (IsValid(CurrentLadder))
	{
		const FVector LadderBottomXY = CurrentLadder->GetLocationAtDistanceAlongSpline(0.f);
		FinalLocation.X = LadderBottomXY.X;
		FinalLocation.Y = LadderBottomXY.Y;
	}
	SetActorLocation(FinalLocation);
}

bool ABlasterCharacter::CanExitLadderAtTop() const
{
	if (!CurrentLadder || !GetWorld())
	{
		return false;
	}

	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule)
	{
		return false;
	}

	const FVector Start = CurrentLadder->GetTopLocation();
	const FVector End = Start + CurrentLadder->GetActorForwardVector() * LadderExitForwardOffset;
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LadderExit), false, this);
	FHitResult Hit;
	const bool bBlocked = GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn, Shape, Params);
	return !bBlocked;
}

void ABlasterCharacter::ServerStartLadder_Implementation(ALadderActor* LadderActor)
{
	StartLadder(LadderActor);
}

void ABlasterCharacter::ServerStopLadder_Implementation(bool bPlaceAtTop)
{
	StopLadder(bPlaceAtTop);
}

void ABlasterCharacter::ServerUpdateLadderInput_Implementation(float AxisValue)
{
	LadderInputAxis = AxisValue;
}

void ABlasterCharacter::OnRep_LadderState()
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		if (bIsOnLadder)
		{
			MoveComp->StopMovementImmediately();
			MoveComp->SetMovementMode(MOVE_Flying);
			MoveComp->MaxFlySpeed = LadderClimbSpeed;
			MoveComp->GravityScale = 0.f;
		}
		else
		{
			MoveComp->SetMovementMode(MOVE_Walking);
			MoveComp->GravityScale = 1.f;
		}
	}
}


void ABlasterCharacter::OnLook(const FInputActionValue& Value)
{
	// 2D Vector 입력 처리 (마우스 X/Y)
	const FVector2D LookVector = Value.Get<FVector2D>();
	
	// X축: 좌우 회전 (마우스 X)
	Turn(LookVector.X);
	
	// Y축: 상하 회전 (마우스 Y)
	LookUp(LookVector.Y);
}

void ABlasterCharacter::OnJumpAction(const FInputActionValue& Value)
{
	// 사다리 범위 내: Jump = 부착 / 떨어지기 / 위층 이동
	if (bCanClimb && CurrentLadder)
	{
		if (!bIsOnLadder)
		{
			StartLadder(CurrentLadder);
		}
		else
		{
			// Jump로 탈출: 꼭대기+위층 공간 있으면 위층, 아니면 바닥/떨어지기 (StopLadder에서 바닥 배치 처리)
			const float SplineLength = CurrentLadder->GetSplineLength();
			const bool bNearTop = LadderCurrentDistance >= SplineLength - LadderTopExitThreshold;
			if (bNearTop && CanExitLadderAtTop())
			{
				StopLadder(true);
			}
			else
			{
				StopLadder(false);
			}
		}
		return;
	}
	//Jump();
}

void ABlasterCharacter::OnEquipAction(const FInputActionValue& Value)
{
	EquipButtonPressed();
}

void ABlasterCharacter::OnCrouchAction(const FInputActionValue& Value)
{
	CrouchButtonPressed();
}

void ABlasterCharacter::OnReloadAction(const FInputActionValue& Value)
{
	ReloadButtonPressed();
}

void ABlasterCharacter::OnAimAction(const FInputActionValue& Value)
{
	AimButtonPressed();
}

void ABlasterCharacter::OnAimActionReleased(const FInputActionValue& Value)
{
	AimButtonReleased();
}

void ABlasterCharacter::OnFireAction(const FInputActionValue& Value)
{
	FireButtonPressed();
}

void ABlasterCharacter::OnFireActionReleased(const FInputActionValue& Value)
{
	FireButtonReleased();
}

void ABlasterCharacter::OnGrenadeAction(const FInputActionValue& Value)
{
	GrenadeButtonPressed();
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
	// 사다리 처리는 OnJumpAction에서 Jump 호출 전에 처리됨
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Super::Jump();
	}
}

void ABlasterCharacter::Crouch(bool bClientSimulation)
{
	if (Combat && Combat->bHoldingTheFlag) return;
	if (bDisableGameplay) return;
	if (bIsCrouched) return;

	// 캡슐 즉시 적용, 캡슐/이동 맞춤
	Super::Crouch(bClientSimulation);

	// 몽타주 재생 (서버 멀티캐스트)
	PlayCrouchMontageLocal(true);
	if (HasAuthority())
	{
		MulticastPlayCrouchMontage(true);
	}
}

void ABlasterCharacter::UnCrouch(bool bClientSimulation)
{
	if (!bIsCrouched) return;

	// 캡슐 먼저 해제
	Super::UnCrouch(bClientSimulation);

	// 몽타주 재생 (서버 멀티캐스트)
	PlayCrouchMontageLocal(false);
	if (HasAuthority())
	{
		MulticastPlayCrouchMontage(false);
	}
}

void ABlasterCharacter::FinishFolding()
{	
	Super::Crouch();
}
void ABlasterCharacter::FinishUnFolding()
{
	Super::UnCrouch();
}

void ABlasterCharacter::PlayCrouchMontageLocal(bool bCrouching)
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	if (bCrouching && CrouchMontage)
	{
		AnimInstance->Montage_Play(CrouchMontage);
	}
	else if (!bCrouching && UnCrouchMontage)
	{
		AnimInstance->Montage_Play(UnCrouchMontage);
	}
}

void ABlasterCharacter::MulticastPlayCrouchMontage_Implementation(bool bCrouching)
{
	PlayCrouchMontageLocal(bCrouching);
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

void ABlasterCharacter::DropCarriedParcelIfAny()
{
	if (!InteractionComponent)
	{
		return;
	}

	if (AParcelActor* Carried = InteractionComponent->GetCarriedParcel())
	{
		// 드랍 시 서버에서 소켓 처리되도록 요청
		Carried->RequestDrop(FVector::ZeroVector);
	}
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
            auto HideIfExists = [SkeletalMesh](FName BoneName)
            {
                if (SkeletalMesh->GetBoneIndex(BoneName) != INDEX_NONE)
                {
                    SkeletalMesh->HideBoneByName(BoneName, EPhysBodyOp::PBO_None);
                }
            };

            auto UnhideIfExists = [SkeletalMesh](FName BoneName)
            {
                if (SkeletalMesh->GetBoneIndex(BoneName) != INDEX_NONE)
                {
                    SkeletalMesh->UnHideBoneByName(BoneName);
                }
            };

	            // 메쉬는 계속 헤더/좌도 머리가 가림
            SkeletalMesh->SetOwnerNoSee(false);

            const TArray<FName> HeadBones = {
                FName("head"),
                FName("hair_front"),
                FName("hair_back"),
                FName("neck_01")
            };

            if (bHideHeadMesh)
            {
                for (const FName& Bone : HeadBones) { HideIfExists(Bone); }
                UpdateHeadShadowProxyVisibility(true);
            }
            else
            {
                for (const FName& Bone : HeadBones) { UnhideIfExists(Bone); }
                UpdateHeadShadowProxyVisibility(false);
            }
        }
    }
}

void ABlasterCharacter::UpdateHeadShadowProxyVisibility(bool bEnableShadowProxy)
{
    if (!HeadShadowProxy || !GetMesh())
    {
        return;
    }

    // Ensure proxy uses the same mesh/skeleton
    if (HeadShadowProxy->GetSkeletalMeshAsset() != GetMesh()->GetSkeletalMeshAsset())
    {
        HeadShadowProxy->SetSkeletalMesh(GetMesh()->GetSkeletalMeshAsset());
    }

    if (bEnableShadowProxy)
    {
        // Hide all bones, then unhide head-related bones so only 머리 부분이 그림자만 보임
        const int32 BoneCount = HeadShadowProxy->GetNumBones();
        for (int32 Index = 0; Index < BoneCount; ++Index)
        {
            HeadShadowProxy->HideBone(Index, EPhysBodyOp::PBO_None);
        }

        const TArray<FName> HeadBones = {
            FName("head"),
            FName("hair_front"),
            FName("hair_back"),
            FName("neck_01")
        };
        for (const FName& Bone : HeadBones)
        {
            HeadShadowProxy->UnHideBoneByName(Bone);
        }

        HeadShadowProxy->SetHiddenInGame(true);  // 숨겨져 있음
        HeadShadowProxy->SetVisibility(true, true); // CastHiddenShadow 그림자만 보임
        HeadShadowProxy->SetCastHiddenShadow(true);
        HeadShadowProxy->SetCastShadow(true);
    }
    else
    {
        HeadShadowProxy->SetHiddenInGame(true);
        HeadShadowProxy->SetVisibility(true, true);
		// 해제 시 기본 상태 복원 (모두 언하이드)
        const int32 BoneCount = HeadShadowProxy->GetNumBones();
        for (int32 Index = 0; Index < BoneCount; ++Index)
        {
            HeadShadowProxy->UnHideBone(Index);
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

	// SuspicionManager로 이벤트 브로드캐스트 (같은 동작 반복 시 시간 갱신되면 표시 감소되도록)
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

	// 상태/위젯 업데이트 (같은 동작 반복 시 시간 갱신)
	SetSuspiciousBehavior(NewBehavior);
}

void ABlasterCharacter::OnRep_CurrentSuspiciousBehavior(ESuspiciousBehavior OldBehavior)
{
	// 클라이언트에 의심 동작 상태 변화 표시
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
		// 이동 멈춤
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

		// Mother AI를 향해 캐릭터 회전 (즉시)
		FVector ToPunisher = (Punisher->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		ToPunisher.Z = 0.0f; // 수평 회전
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

		if (bOutOfLives)
		{
			return;
		}

		// 이동 복구 - 명시적으로 재설정
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

			// 추적 보장: 입력 무시 플래그 강제 해제
			Controller->SetIgnoreLookInput(false);
			Controller->SetIgnoreMoveInput(false);
		}

		UE_LOG(LogTemp, Log, TEXT("[BlasterCharacter] Player %s punishment ended, movement restored"), *GetName());
	}
}

void ABlasterCharacter::HandleOutOfLives()
{
	if (!HasAuthority() || bOutOfLives)
	{
		return;
	}

	bOutOfLives = true;
	bDisableGameplay = true;

	// 처벌 몽타주 종료(실행 중인 경우)
	if (AMotherAIActor* Mother = Cast<AMotherAIActor>(PunisherActor))
	{
		Mother->StopPunishmentForTarget(this);
	}

	// 이동/입력 차단
	if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
	{
		MovementComp->DisableMovement();
		MovementComp->StopMovementImmediately();
	}
	if (Controller)
	{
		Controller->SetIgnoreLookInput(true);
		Controller->SetIgnoreMoveInput(true);
	}

	DropCarriedParcelIfAny();

	Multicast_HandleOutOfLives();

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		Client_HandleOutOfLives();
	}

	// 사망 몽타주 종료 처리(Notify에서 호출되도록 변경)
}

void ABlasterCharacter::MoveToConveyorEntry()
{
	if (!GetWorld())
	{
		return;
	}

	if (bUseCustomDeathLocation)
	{
		TSubclassOf<AActor> SearchClass = DeathLocationActorClass;
		if (!SearchClass)
		{
			SearchClass = ADeathLocationActor::StaticClass();
		}

		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(this, SearchClass, FoundActors);
		if (FoundActors.Num() > 0)
		{
			AActor* BestActor = FoundActors[0];
			float BestDistanceSq = FVector::DistSquared(GetActorLocation(), BestActor->GetActorLocation());
			for (int32 Index = 1; Index < FoundActors.Num(); ++Index)
			{
				AActor* Candidate = FoundActors[Index];
				const float DistanceSq = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
				if (DistanceSq < BestDistanceSq)
				{
					BestDistanceSq = DistanceSq;
					BestActor = Candidate;
				}
			}
			SetActorLocation(BestActor->GetActorLocation());
		}
		else
		{
			SetActorLocation(CustomDeathLocation);
		}
		if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
		{
			// 사망 상태에서 중력으로 떨어지도록 하강 모드로 전환
			MovementComp->SetMovementMode(MOVE_Falling);
			MovementComp->Activate(true);
			MovementComp->SetComponentTickEnabled(true);
		}
		return;
	}

	TArray<AActor*> Conveyors;
	UGameplayStatics::GetAllActorsOfClass(this, AConveyorBeltActor::StaticClass(), Conveyors);
	if (Conveyors.Num() == 0)
	{
		return;
	}

	if (AConveyorBeltActor* Belt = Cast<AConveyorBeltActor>(Conveyors[0]))
	{
		const FVector EntryLocation = Belt->GetConveyorEntryLocation();
		SetActorLocation(EntryLocation);
		if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
		{
			// 사망 상태에서 중력으로 떨어지도록 하강 모드로 전환
			MovementComp->SetMovementMode(MOVE_Falling);
			MovementComp->Activate(true);
			MovementComp->SetComponentTickEnabled(true);
		}
	}
}

void ABlasterCharacter::Multicast_HandleOutOfLives_Implementation()
{
	if (bOutOfLivesVisualsApplied)
	{
		return;
	}
	bOutOfLivesVisualsApplied = true;

	// 사망 시작 시 로컬 카메라를 3인칭으로 전환
	if (bUseDeathThirdPersonCamera && IsLocallyControlled() && FollowCamera)
	{
		if (!bCachedDeathCamera)
		{
			CachedDeathCameraLocation = FollowCamera->GetRelativeLocation();
			CachedDeathCameraUsePawnControlRotation = FollowCamera->bUsePawnControlRotation;
			bCachedDeathCamera = true;
		}

		FollowCamera->SetRelativeLocation(FVector(-DeathThirdPersonDistance, 0.f, DeathThirdPersonHeight));
		FollowCamera->SetRelativeRotation(FRotator(DeathThirdPersonRotationPitch, 0.f, 0.f));
		FollowCamera->bUsePawnControlRotation = true;

		// 1인칭 머리 숨김 해제, 3인칭에서 머리가 보이도록
		ToggleHeadMesh(false);
	}

	// 사망 몽타주(재생)
	if (DeathMontage && GetMesh() && GetMesh()->GetAnimInstance())
	{
		USkeletalMeshComponent* MeshComp = GetMesh();
		UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
		// 기존 몽타주 재생 중이면 사망 몽타주 재생 막히는 경우가 있어 처리
		AnimInstance->StopAllMontages(0.0f);
		MeshComp->bPauseAnims = false;

		bDeath = true;
		AnimInstance->Montage_Play(DeathMontage);
	}
	else if (GetMesh())
	{
		GetMesh()->bPauseAnims = true;
	}
	if (ElimEffect)
	{		
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			ElimEffect,
			EffectSocketBody,
			FName(""),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			false
		);
	}
	if (ElimSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(
			this,
			ElimSound,
			GetActorLocation()
		);
	}
}

void ABlasterCharacter::Client_HandleOutOfLives_Implementation()
{
	Multicast_HandleOutOfLives_Implementation();
}

void ABlasterCharacter::OnRep_OutOfLives()
{
	if (bOutOfLives)
	{
		Multicast_HandleOutOfLives_Implementation();
	}
}

void ABlasterCharacter::Multicast_FreezeDeathPose_Implementation()
{
	if (GetMesh())
	{
		GetMesh()->bPauseAnims = true;
	}
}

void ABlasterCharacter::HandleDeathMontageFinished()
{
	if (!HasAuthority())
	{
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, DeathFadeOutDuration, FLinearColor::Black, false, true);
		}
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			DeathFadeTimerHandle,
			this,
			&ABlasterCharacter::HandleDeathFadeOutComplete,
			FMath::Max(0.0f, DeathFadeOutDuration),
			false
		);
	}
}

void ABlasterCharacter::HandleDeathFadeOutComplete()
{
	if (!HasAuthority())
	{
		return;
	}

	MoveToConveyorEntry();

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, DeathFadeInDuration, FLinearColor::Black, false, false);
		}
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

	// 목표 회전 업데이트 (Punisher가 이동할 수 있으므로)
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
		Controller->SetControlRotation(TargetCameraRotation); // 정확히 위치 설정
		UE_LOG(LogTemp, Log, TEXT("[BlasterCharacter] Camera rotation to punisher completed"));
	}
}
