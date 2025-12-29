// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstance.h"
#include "Particles/ParticleSystem.h"
#include "Camera/CameraShakeBase.h"
#include "Blaster/BlasterTypes/TurningInPlace.h"
#include "Blaster/Interfaces/InteractWithCrosshairInterface.h"
#include "Components/TimelineComponent.h"
#include "Blaster/BlasterTypes/CombatState.h"
#include "Blaster/BlasterComponents/CombatComponent.h"
#include "Blaster/BlasterTypes/Team.h"
#include "Blaster/PickpackerTypes/PickpackerTypes.h"
#include "InputActionValue.h"
#include "BlasterCharacter.generated.h"

class UInputAction;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLeftGame);

UENUM(BlueprintType)
enum class EPerspective : uint8 {
	EPT_FirstPerson UMETA(DisplayName = "FirstPerson"),
	EPT_ThirdPerson UMETA(DisplayName = "ThirdPerson"),
	EPT_Sequence UMETA(DisplayName = "Sequence")
};

USTRUCT(BlueprintType)
struct FPerspectiveSettings {
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	EPerspective Perspective = EPerspective::EPT_FirstPerson;
};

USTRUCT(BlueprintType)
struct FDebugCollisionVisibilitySettings
{
	GENERATED_BODY()

public:
	/** Toggle all hit collision boxes at once */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Collision")
	bool bShowHitCollisionBoxes = false;

	/** Optional per-component overrides keyed by component name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Collision")
	TMap<FName, bool> ComponentVisibilities;
};

UCLASS()
class BLASTER_API ABlasterCharacter : public ACharacter, public IInteractWithCrosshairInterface
{
	GENERATED_BODY()

public:

	ABlasterCharacter();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** OverHeadWidget 업데이트 (플레이어 이름, 준비 상태 등) */
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void UpdateOverheadWidget();

	/**
	* Play Montages
	*/

	void PlayFireMontage(bool bAiming);
	void PlayReloadMontage();
	void PlayElimMontage();
	void PlayThrowGrenadeMontage();
	void PlaySwapMontage();

	virtual void OnRep_ReplicatedMovement() override;
	void Elim(bool bPlayerLeftGame);
	UFUNCTION(NetMulticast, Reliable)
	void MulticastElim(bool bPlayerLeftGame);
	virtual void Destroyed() override;

	UPROPERTY(Replicated)
	bool bDisableGameplay = false; // Disable gameplay for this character, used in the lobby

	/** 처벌 중인지 (Mother AI 처벌 시) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Punishment")
	bool bBeingPunished = false;

	/** 엔딩 진행 중 여부 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Ending")
	bool bEndingInProgress = false;

	UFUNCTION(BlueprintImplementableEvent)
	void ShowSniperScopeWidget(bool bShowSniperScope);
	void UpdateHUDHealth();
	void UpdateHUDShield();
	void UpdateHUDAmmo();
	void SpawnDefaultWeapon();

	UFUNCTION(BlueprintCallable, Category = "Ending")
	void SetEndingInProgress(bool bInProgress);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ending")
	bool IsEndingInProgress() const { return bEndingInProgress; }


	UPROPERTY()
	TMap<FName, class UBoxComponent*> HitCollisionBoxes;

	bool bFinishedSwapping = false;

	// 카메라 흔들림 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Shake")
	TSubclassOf<UCameraShakeBase> CameraShakeClass;

	// 흔들림 세기 스케일
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Shake", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HitCameraShakeScale = 1.0f;

	// 소유 클라이언트 RPC
	UFUNCTION(Client, Reliable)
	void Client_PlayHitCameraShake(float Scale = 1.0f);

	UFUNCTION(Server, Reliable)
	void ServerLeaveGame();

	FOnLeftGame OnLeftGame;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastGainedTheLead();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastLostTheLead();

	void SetTeamColor(ETeam Team);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPerspectiveSettings PerspectiveSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Collision")
	FDebugCollisionVisibilitySettings DebugCollisionVisibility;

	UFUNCTION(BlueprintCallable, Category = "Debug|Collision")
	void RefreshDebugCollisionVisibility();

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Socket")
	FName CarrySocketName = FName("CarrySocket");

	UFUNCTION(BlueprintCallable, Category = "Suspicion")
	void ReportSuspiciousBehavior(ESuspiciousBehavior Behavior);
	/** 처벌 상태 설정 (Mother AI 처벌 시 호출) */
	UFUNCTION(BlueprintCallable, Category = "Punishment")
	void SetBeingPunished(bool bPunishing, AActor* Punisher = nullptr);

protected:
	virtual void BeginPlay() override;
	
	// Legacy input functions (deprecated, kept for compatibility)
	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);
	void EquipButtonPressed();
	void CrouchButtonPressed();
	void ReloadButtonPressed();
	void AimButtonPressed();
	void AimButtonReleased();
	void GrenadeButtonPressed();
	void InventoryInteractionButtonPressed();
	void EquipInventoryItem(int32 SlotIndex);

	/** Enhanced Input: Movement actions (2D Vector - WASD) */
	void OnMovement(const FInputActionValue& Value);
	
	/** Enhanced Input: Look actions (2D Vector - Mouse X/Y) */
	void OnLook(const FInputActionValue& Value);
	
	/** Enhanced Input: Combat actions */
	void OnJumpAction(const FInputActionValue& Value);
	void OnEquipAction(const FInputActionValue& Value);
	void OnCrouchAction(const FInputActionValue& Value);
	void OnReloadAction(const FInputActionValue& Value);
	void OnAimAction(const FInputActionValue& Value);
	void OnAimActionReleased(const FInputActionValue& Value);
	void OnFireAction(const FInputActionValue& Value);
	void OnFireActionReleased(const FInputActionValue& Value);
	void OnGrenadeAction(const FInputActionValue& Value);

	/** Enhanced Input: Single inventory action (put carried parcel into inventory) */
	void OnInventoryAction(const FInputActionValue& Value);

	/** Enhanced Input: Inventory slot actions (1-9) */
	void OnInventorySlotOne(const FInputActionValue& Value);
	void OnInventorySlotTwo(const FInputActionValue& Value);
	void OnInventorySlotThree(const FInputActionValue& Value);
	void OnInventorySlotFour(const FInputActionValue& Value);
	void OnInventorySlotFive(const FInputActionValue& Value);
	void OnInventorySlotSix(const FInputActionValue& Value);
	void OnInventorySlotSeven(const FInputActionValue& Value);
	void OnInventorySlotEight(const FInputActionValue& Value);
	void OnInventorySlotNine(const FInputActionValue& Value);

public:
	/** Enhanced Input: Movement input action (2D Vector - WASD) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* MovementAction;
	
	/** Enhanced Input: Look input action (2D Vector - Mouse X/Y) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* LookAction;
	
	/** Enhanced Input: Combat input actions */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* JumpAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* EquipAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* CrouchAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* ReloadAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* AimAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* FireAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* GrenadeAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InventoryInputAction;
	/** Enhanced Input: Inventory slot input actions */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InventorySlotOneAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InventorySlotTwoAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InventorySlotThreeAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InventorySlotFourAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InventorySlotFiveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InventorySlotSixAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InventorySlotSevenAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InventorySlotEightAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InventorySlotNineAction;
	void AimOffset(float DeltaTime);
	void CalculateAO_Pitch();
	void SimProxiesTurn();
	virtual void Jump() override;
	virtual void Crouch(bool bClientSimulation = false) override;
	virtual void UnCrouch(bool bClientSimulation = false) override;

	UFUNCTION(BlueprintCallable)
	void FinishFolding();
	UFUNCTION(BlueprintCallable)
	void FinishUnFolding();

	void FireButtonPressed();
	void FireButtonReleased();
	void PlayHitReactMontage();
	void DropOrDestroyWeapon(AWeapon* Weapon);
	void DropOrDestroyWeapons();
	void SetSpawnPoint();
	void OnPlayerStateInitialized();

	UFUNCTION()
	void ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, class AController* InstigatorController, AActor* DamageCauser);
	
	// Poll for any relevant classes and initialize our HUD
	void PollInit();
	void RotateInPlace(float DeltaTime);

	// Expose these to BP (not private) to satisfy UHT001
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	class UInteractionComponent* InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carry IK")
	class UCarryIKComponent* CarryIKComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	class UPlayerInventoryComponent* PlayerInventoryComponent;

	/** 현재 의심 행동 상태 */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentSuspiciousBehavior, BlueprintReadOnly, Category = "Suspicion")
	ESuspiciousBehavior CurrentSuspiciousBehavior = ESuspiciousBehavior::None;

	UFUNCTION()
	void OnRep_CurrentSuspiciousBehavior(ESuspiciousBehavior OldBehavior);

	UFUNCTION(BlueprintCallable, Category = "Suspicion")
	void UpdateSuspiciousBehavior(ESuspiciousBehavior NewBehavior);
	/** 의심 행동 상태 설정 */
	UFUNCTION(BlueprintCallable, Category = "Suspicion")
	void SetSuspiciousBehavior(ESuspiciousBehavior Behavior);

	/** 현재 의심 행동 상태 가져오기 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Suspicion")
	ESuspiciousBehavior GetCurrentSuspiciousBehavior() const { return CurrentSuspiciousBehavior; }

	

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetBeingPunished(bool bPunishing, AActor* Punisher);

	/** 처벌 중인지 확인 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Punishment")
	bool IsBeingPunished() const { return bBeingPunished; }

	/** 카메라 회전이 완료되었는지 확인 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Punishment")
	bool IsCameraRotationComplete() const { return bCameraRotationComplete; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float CarriedHideCameraThreshold = 50.f; // Distance to hide camera when character is carrying an item


#pragma region Hit box
	/**
	* Hit boxes used for server-side rewind
	*/

	UPROPERTY(EditAnywhere)
	class UBoxComponent* head;

	UPROPERTY(EditAnywhere)
	UBoxComponent* pelvis;

	UPROPERTY(EditAnywhere)
	UBoxComponent* spine_02;

	UPROPERTY(EditAnywhere)
	UBoxComponent* spine_03;

	UPROPERTY(EditAnywhere)
	UBoxComponent* upperarm_l;

	UPROPERTY(EditAnywhere)
	UBoxComponent* upperarm_r;

	UPROPERTY(EditAnywhere)
	UBoxComponent* lowerarm_l;

	UPROPERTY(EditAnywhere)
	UBoxComponent* lowerarm_r;

	UPROPERTY(EditAnywhere)
	UBoxComponent* hand_l;

	UPROPERTY(EditAnywhere)
	UBoxComponent* hand_r;

	UPROPERTY(EditAnywhere)
	UBoxComponent* backpack;

	UPROPERTY(EditAnywhere)
	UBoxComponent* blanket;

	UPROPERTY(EditAnywhere)
	UBoxComponent* thigh_l;

	UPROPERTY(EditAnywhere)
	UBoxComponent* thigh_r;

	UPROPERTY(EditAnywhere)
	UBoxComponent* calf_l;

	UPROPERTY(EditAnywhere)
	UBoxComponent* calf_r;

	UPROPERTY(EditAnywhere)
	UBoxComponent* foot_l;

	UPROPERTY(EditAnywhere)
	UBoxComponent* foot_r;
#pragma endregion
private:
	UPROPERTY(VisibleAnywhere, Category = Camera)
	class UCameraComponent* FollowCamera;

	/** 1인칭 카메라 높이 (눈 위치) */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float EyeHeight = 64.f;
		
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UWidgetComponent* OverHeadWidget;

	UPROPERTY(ReplicatedUsing = OnRep_OverlappingWeapon)
	class AWeapon* OverlappingWeapon;

	UFUNCTION()
	void OnRep_OverlappingWeapon(AWeapon* LastWeapon);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UCombatComponent* Combat;
	UPROPERTY(VisibleAnywhere)
	class UBuffComponent* Buff;

	UPROPERTY(VisibleAnywhere)
	class ULagCompensationComponent* LagCompensation;

	UFUNCTION(Server, Reliable)
	void ServerEquipButtonPressed();

	float AO_Yaw;
	float InterpAO_Yaw;
	float AO_Pitch;
	FRotator StartingAimRotation;

	ETurningInPlace TurningInPlace;
	void TurnInPlace(float DeltaTime);

	UPROPERTY(EditAnywhere, Category = Combat)
	class UAnimMontage* FireWeaponMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* ReloadMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* HitReactMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* ElimMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* ThrowGrenadeMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* SwapMontage;
	
	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* CrouchMontage;
	
	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* UnCrouchMontage;

	void HideCameraIfCharacterClose();
	void HideCarriedCameraIfCharacterClose();

	void ToggleHeadMesh(bool bHideHeadMesh);

	UPROPERTY(EditAnywhere)
	float CameraThreshold = 200.f; // Distance to hide camera when character is close

	/** 처벌 시 카메라 회전 관련 변수 */
	UPROPERTY()
	bool bShouldRotateCameraToPunisher = false;

	UPROPERTY()
	FRotator TargetCameraRotation;

	UPROPERTY(EditAnywhere, Category = "Punishment")
	float CameraRotationSpeed = 2.0f; // 카메라 회전 속도

	UPROPERTY()
	bool bCameraRotationComplete = false;

	UPROPERTY()
	AActor* PunisherActor = nullptr;

	/** 처벌 시 카메라 회전 처리 */
	void RotateCameraToPunisher(float DeltaTime);

	/**
	 * Update movement speed based on carried parcel weight
	 */
	void UpdateMovementSpeedFromCarriedParcel();

	/** Cached original max walk speed for weight calculations */
	float CachedOriginalMaxWalkSpeed = -1.0f;

	bool bRotateRootBone;
	float TurnThreshold = 0.5f; // Threshold to start turning in place
	FRotator ProxyRotationLastFrame;
	FRotator ProxyRotation;
	float ProxyYaw;
	float TimeSinceLastMovementReplication;
	float CalculateSpeed();

	/**
	* Player health
	*/
	UPROPERTY(EditAnywhere, Category = "Player Stats")
	float MaxHealth = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Health, VisibleAnywhere, Category = "Player Stats")
	float Health = 100.f;

	UFUNCTION()
	void OnRep_Health(float LastHealth);

	/**
	* Player Shields
	*/

	UPROPERTY(EditAnywhere, Category = "Player Stats")
	float MaxShield = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Shield, VisibleAnywhere, Category = "Player Stats")
	float Shield = 0.f;

	UFUNCTION()
	void OnRep_Shield(float LastShield);


	UPROPERTY()
	class ABlasterPlayerController* BlasterPlayerController;

	bool bElimmed = false;

	FTimerHandle ElimTimer;

	UPROPERTY(EditDefaultsOnly)
	float ElimDelay = 3.f; // Delay before player can respawn after elimination
	void ElimTimerFinished();

	bool bLeftGame = false;

	
	/**
	* Dissolve effect
	*/
	UPROPERTY(VisibleAnywhere)
	UTimelineComponent* DissolveTimeline;
	FOnTimelineFloat DissolveTrack;

	UFUNCTION()
	void UpdateDissolveMaterial(float DissolveValue);
	void StartDissolve();

	UPROPERTY(EditAnywhere)
	UCurveFloat* DissolveCurve;

	// Dynamic instance that we can change at runtime
	UPROPERTY(VisibleAnywhere, Category = Elim)
	UMaterialInstanceDynamic* DynamicDissolveMaterialInstance;

	// Material instance set on the Blueprint, used with the dynamic material instance
	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* DissolveMaterialInstance;

	/**
	* Team color
	*/

	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* RedDissolveMatInst;

	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* RedMaterial;

	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* BlueDissolveMatInst;

	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* BlueMaterial;

	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* OriginalMaterial;

	/**
	* Elim effects
	*/

	UPROPERTY(EditAnywhere)
	UParticleSystem* ElimBotEffect;

	UPROPERTY(VisibleAnywhere)
	UParticleSystemComponent* ElimBotComponent;

	UPROPERTY(EditAnywhere)
	class USoundCue* ElimBotSound;

	UPROPERTY()
	class ABlasterPlayerState* BlasterPlayerState;

	UPROPERTY(EditAnywhere)
	class UNiagaraSystem* CrownSystem;

	UPROPERTY()
	class UNiagaraComponent* CrownSystemComponent;

	/**
	*  Grenade
	*/

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* AttachedGrenade;

	/**
	* Default Weapon
	*/
	UPROPERTY(EditAnywhere)
	TSubclassOf<class AWeapon> DefaultWeaponClass;

	UPROPERTY()
	class ABlasterGameMode* BlasterGameMode;
public:
	void SetOverlappingWeapon(AWeapon* Weapon);
	bool IsWeaponEquipped();
	bool IsAiming();
	FORCEINLINE float GetAO_Yaw() const { return AO_Yaw; }
	FORCEINLINE float GetAO_Pitch() const { return AO_Pitch; }
	AWeapon* GetEquippedWeapon();
	FORCEINLINE ETurningInPlace GetTurningInPlace() const { return TurningInPlace; }
	FVector GetHitTarget() const;
	FORCEINLINE class UInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }
	FORCEINLINE class UCarryIKComponent* GetCarryIKComponent() const { return CarryIKComponent; }
	FORCEINLINE class UPlayerInventoryComponent* GetPlayerInventoryComponent() const { return PlayerInventoryComponent; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	FORCEINLINE bool ShouldRotateRootBone() const { return bRotateRootBone; }
	FORCEINLINE bool IsElimmed() const { return bElimmed; }
	FORCEINLINE float GetHealth() const { return Health; }
	FORCEINLINE float SetHealth(float Amount) { return Health = Amount; }
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }
	FORCEINLINE float GetShield() const { return Shield; }
	FORCEINLINE float SetShield(float Amount) { return Shield = Amount; }
	FORCEINLINE float GetMaxShield() const { return MaxShield; }
	FORCEINLINE FName GetCarrySocketName() const { return CarrySocketName; }
	ECombatState GetCombatState() const;
	FORCEINLINE UCombatComponent* GetCombat() const { return Combat; }
	FORCEINLINE bool GetDisableGameplay() const { return bDisableGameplay; }
	FORCEINLINE UAnimMontage* GetReloadMontage() const { return ReloadMontage; }
	FORCEINLINE UStaticMeshComponent* GetAttachedGrenade() const { return AttachedGrenade; }
	FORCEINLINE UBuffComponent* GetBuff() const { return Buff; }

	bool IsLocallyReloading();
	FORCEINLINE ULagCompensationComponent* GetLagCompensation() const { return LagCompensation; }
	FORCEINLINE bool IsHoldingTheFlag() const { return Combat && Combat->bHoldingTheFlag; }
	ETeam GetTeam();
	void SetHoldingTheFlag(bool bHoldingFlag);

private:
	void ApplyDebugCollisionVisibility();
	UPrimitiveComponent* FindDebugPrimitiveByName(const FName ComponentName) const;
	bool IsPerspectiveFirstPerson() const;
	bool IsPerspectiveThirdPersonLike() const;
};
