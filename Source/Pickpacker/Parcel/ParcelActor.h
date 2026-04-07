// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DataAssets/DA_ParcelData.h"
#include "DataAssets/ParcelRowNamePicker.h"
#include "DataAssets/DA_ItemData.h"
#include "Components/ParcelStateComponent.h"
#include "Components/CarryPointsComponent.h"
#include "UI/ParcelHUDWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameplayTagContainer.h"
#include "Interfaces/InteractableInterface.h"
#include "ParcelActor.generated.h"

class UCarryPointsComponent;
class AShelfActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnParcelAttached, class ACharacter*, Carrier, FName, SocketId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnParcelDropped, FVector, DropLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnParcelBroken, class AParcelActor*, Parcel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnParcelItemUsed, class ACharacter*, User, EItemType, ItemType);

/**
 * Parcel Actor - Represents a package that can be carried by players
 */
UCLASS(BlueprintType, Blueprintable)
class PICKPACKER_API AParcelActor : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AParcelActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Destroyed() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Initialize parcel with configuration
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel")
	void InitializeParcel(const FParcelConfig& Config);

	/**
	 * Apply ParcelData asset configuration using the configured tag
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel")
	bool ApplyParcelConfigFromDataAsset(bool bInitializeRuntime = true);

	/** DataAsset/RowName 수동 설정 (스폰 시 주입용) */
	UFUNCTION(BlueprintCallable, Category = "Parcel")
	void SetParcelDataAsset(UDA_ParcelData* InAsset) { ParcelDataAsset = InAsset; }

	UFUNCTION(BlueprintCallable, Category = "Parcel")
	void SetParcelDefinitionRowName(const FName& InRow) { ParcelDefinitionRowName = InRow; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel")
	FName GetParcelDefinitionRowName() const { return ParcelDefinitionRowName; }
	void SetMeshPhysics(bool bEnablePhysics);
	/** 포장 레시피/콘텐츠 적용 */
	void SetPackageRecipe(const FParcelPackageRecipe* InRecipe, UDA_ParcelData* InParcelDataAsset = nullptr);
	void SetPackageContents(const TArray<FParcelPackageContent>& InContents);
	/** 포장 해제 (정상) / 파괴 시 언팩 */
	void UnpackAtTransform(const FTransform& OutTransform, bool bScatterAroundLocation);
	void SpawnPackageContents(const FTransform& SpawnTransform, bool bScatterAroundLocation);

	/**
	 * Request to attach parcel to character
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel")
	void RequestAttach(class ACharacter* Carrier, const FName& SocketId);

	/**
	 * Request to drop parcel
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel")
	void RequestDrop(const FVector& Impulse = FVector::ZeroVector);

	/**
	 * Check if parcel can be attached
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel")
	bool CanBeAttached() const;

	/**
	 * Check if parcel is attached
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel")
	bool IsAttached() const;

	/**
	 * Get parcel state
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel")
	const FParcelState& GetParcelState() const;

	/**
	 * Get carry points component
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel")
	UCarryPointsComponent* GetCarryPointsComponent() const { return CarryPointsComponent; }

	/**
	 * Get parcel state component
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel")
	UParcelStateComponent* GetParcelStateComponent() const { return ParcelStateComponent; }

	/**
	 * Get parcel tags
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel")
	FGameplayTagContainer GetParcelTags() const { return ParcelTags; }

	/** Classification gameplay tag */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel")
	FGameplayTag GetClassificationTag() const { return ParcelClassificationTag; }

	/** Parcel tag from ParcelConfig */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel|Config")
	FGameplayTag GetParcelTag() const { return ParcelConfig.ParcelTag; }

	/** Parcel display name (from ParcelConfig) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel|Config")
	FString GetParcelDisplayName() const { return ParcelConfig.ParcelName; }

	/** Price earned when submitted */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel|Economy")
	int32 GetParcelPrice() const { return ParcelPrice; }

	/**
	 * Set parcel tags
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel")
	void SetParcelTags(const FGameplayTagContainer& NewTags) { ParcelTags = NewTags; }

	/**
	 * Add parcel tag
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel")
	void AddParcelTag(const FGameplayTag& Tag) { ParcelTags.AddTag(Tag); }

	/**
	 * Remove parcel tag
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel")
	void RemoveParcelTag(const FGameplayTag& Tag) { ParcelTags.RemoveTag(Tag); }

	/**
	 * Check if parcel is packaged
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel")
	bool IsPackaged() const { return bIsPackaged; }

	/** 패키지 번들 여부 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel")
	bool IsPackageBundle() const { return bIsPackageBundle; }

	/**
	 * Set packaged state and update mesh
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel")
	void SetPackaged(bool bPackaged);

	/**
	 * Get original mesh (before packaging)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel")
	UStaticMesh* GetOriginalMesh() const { return OriginalMesh; }

	/**
	 * Check if this parcel is an item (unpackaged)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Item")
	bool IsItem() const { return bIsItem; }

	/** Whether this parcel should be carried two-handed (based on carry socket count) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel")
	bool RequiresTwoHandCarry() const { return bRequiresTwoHandCarry; }

	/**
	 * Check if this item is usable
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Item")
	bool IsUsable() const { return bIsUsable; }

	/**
	 * Get item type
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Item")
	EItemType GetItemType() const { return ItemType; }

	/**
	 * Get item data
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Item")
	const FItemData& GetItemData() const { return ItemData; }

	/** 포장 수량 계산용 공간 차지 단위 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel|Packaging")
	int32 GetPackagingSpaceUnits() const;

	/** 주문 수량 계산: 박스 내용물 unit 합산 (박스 1개=1이 아님) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel|Economy")
	int32 GetContentUnitTotal() const;

	/** RequiredParcelTag와 일치하는 콘텐츠의 unit 수 (주문 매칭용, 포장 시 contents 기준) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel|Economy")
	int32 GetContentUnitsForTag(FGameplayTag RequiredTag) const;

	/** RequiredParcelTag와 일치하는 콘텐츠의 가치 합산 (크레딧 비례 계산용) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel|Economy")
	int32 GetContentValueForTag(FGameplayTag RequiredTag) const;

	/** 주문 제출 시 크레딧: 박스 내용물 가치 합산 (BasePrice*Count) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel|Economy")
	int32 GetContentValueTotal() const;

	/**
	 * Set item data
	 */
	UFUNCTION(BlueprintCallable, Category = "Item")
	void SetItemData(const FItemData& NewItemData);

	/**
	 * Use this item (if usable)
	 */
	UFUNCTION(BlueprintCallable, Category = "Item")
	bool UseItem(class ACharacter* User);

	/**
	 * Server RPC for item use
	 */
	UFUNCTION(Server, Reliable, Category = "Item")
	void Server_UseItem(ACharacter* User);

	/**
	* 	HUD 제어
	*/
	void SetTargetedByLocalPlayer(bool bTargeted);

	/**
	 * Overlap notification for interaction
	 */

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void NotifyBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void NotifyEndOverlap(AActor* OverlappedActor, AActor* OtherActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
	void BP_OnBeginFocus(AActor* OtherActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
	void BP_OnEndFocus(AActor* OtherActor);

protected:
	/**
	 * Server RPC for attach request
	 */
	UFUNCTION(Server, Reliable, Category = "Parcel")
	void Server_RequestAttach(ACharacter* Carrier, FName SocketId);

	/**
	 * Server RPC for drop request
	 */
	UFUNCTION(Server, Reliable, Category = "Parcel")
	void Server_RequestDrop(FVector Impulse);

	/**
	 * Multicast RPC for attach event (Reliable - 클라이언트에서 AttachActor 실행 필수, 패키징 빌드에서 Unreliable 시 누락되면 땅에 떨어짐)
	 */
	UFUNCTION(NetMulticast, Reliable, Category = "Parcel")
	void Multicast_ParcelAttached(ACharacter* Carrier, FName SocketId);

	/**
	 * Multicast RPC for drop event
	 */
	UFUNCTION(NetMulticast, Unreliable, Category = "Parcel")
	void Multicast_ParcelDropped(ACharacter* Carrier, FVector DropLocation);

	/**
	 * Multicast RPC for playing parcel audio/VFX effects
	 */
	UFUNCTION(NetMulticast, Reliable, Category = "Parcel|AV")
	void Multicast_PlayParcelEffect(FName EventKey, FVector Location);

	/**
	 * Handle parcel state changes
	 */
	UFUNCTION()
	void OnParcelStateChanged(const FParcelState& NewState);

	/**
	 * Handle parcel broken
	 */
	UFUNCTION()
	void HandleParcelBroken();

	/**
	 * Update HUD widget
	 */
	UFUNCTION()
	void UpdateHUDWidget();

	/**
	 * Set HUD widget class
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel")
	void SetHUDWidgetClass(TSubclassOf<UParcelHUDWidget> WidgetClass);

	/**
	 * Configure physics for drop
	 */
	void ConfigureDropPhysics(const FVector& Impulse);

	/**
	 * Stabilize physics after drop
	 */
	void StabilizePhysics();

	/**
	 * Handle impact events for destruction-on-impact logic
	 */
	UFUNCTION()
	void HandleParcelMeshHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit
	);

public:
	// Delegates
	UPROPERTY(BlueprintAssignable, Category = "Parcel")
	FOnParcelAttached OnParcelAttached;

	UPROPERTY(BlueprintAssignable, Category = "Parcel")
	FOnParcelDropped OnParcelDropped;

	UPROPERTY(BlueprintAssignable, Category = "Parcel")
	FOnParcelBroken OnParcelBroken;

	/** Item used event */
	UPROPERTY(BlueprintAssignable, Category = "Item")
	FOnParcelItemUsed OnItemUsed;

protected:
	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UParcelStateComponent* ParcelStateComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCarryPointsComponent* CarryPointsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UWidgetComponent* HUDWidgetComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UWidgetComponent* PickupWidget;
	// Parcel configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Config", meta = (AllowPrivateAccess = "true"))
	bool bAutoApplyParcelData = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config", meta = (AllowPrivateAccess = "true"))
	UDA_ParcelData* ParcelDataAsset = nullptr;

	// DataAsset 안의 ParcelConfigs 중 하나를 선택 (ParcelName으로 드롭다운)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config", meta = (AllowPrivateAccess = "true", GetOptions = "GetParcelRowOptions"))
	FName ParcelDefinitionRowName;

	// 포장 레시피 참조 (패키징된 번들일 때 사용)
	UPROPERTY(Replicated)
	FGameplayTag PackageTargetTag;

	UPROPERTY(Replicated)
	FName PackageTargetRowName = NAME_None;

	UPROPERTY(Replicated)
	int32 PackageRequiredCount = 0;

	UPROPERTY(Replicated)
	int32 PackageMinRequiredCount = 0;

	UPROPERTY(Replicated)
	int32 PackageMaxRequiredCount = 0;

	UPROPERTY(Replicated)
	TSoftObjectPtr<UStaticMesh> PackageMeshAsset;

	UPROPERTY(VisibleInstanceOnly, Category = "Parcel Packaging", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDA_ParcelData> PackageParcelDataAsset = nullptr;

	/** 포장된 콘텐츠 (런타임 데이터, PackedParcelActor의 InitialContents에서 설정됨) */
	UPROPERTY(VisibleInstanceOnly, Category = "Parcel Packaging", meta = (AllowPrivateAccess = "true"))
	TArray<FParcelPackageContent> PackageContents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Config", meta = (AllowPrivateAccess = "true", EditCondition = "!bAutoApplyParcelData", EditConditionHides))
	FParcelConfig ParcelConfig;

	// Parcel tags
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Config")
	FGameplayTagContainer ParcelTags;

	// Physics settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float DropStabilizeTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float DropImpulseMultiplier = 1.0f;

	// 충격 판정 최소 조건 및 쿨다운
	UPROPERTY(EditAnywhere, Category = "Parcel|Damage")
	float MinImpactSpeedForDamage = 75.0f;

	UPROPERTY(EditAnywhere, Category = "Parcel|Damage")
	float MinImpactImpulseForDamage = 80.0f;

	UPROPERTY(EditAnywhere, Category = "Parcel|Damage")
	float ImpactDamageCooldown = 0.15f;
	// State tracking
	UPROPERTY(Replicated)
	bool bIsAttached = false;

	UPROPERTY(Replicated)
	FName CurrentSocketId = NAME_None;

	UPROPERTY(Replicated)
	class ACharacter* CurrentCarrier = nullptr;

	// Packaging state
	UPROPERTY(Replicated)
	bool bIsPackaged = false;
	UPROPERTY(Replicated)
	bool bIsPackageBundle = false;
	UPROPERTY()
	bool bHasUnpacked = false;
	UPROPERTY()
	bool bSkipContentSpawnOnDestroy = true;


	/** 포장된 상태의 메시 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel|Packaging", meta = (EditCondition = "!bAutoApplyParcelData", EditConditionHides))
	UStaticMesh* PackagedMesh = nullptr;

	/** 언패키지 상태 기본 메시 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel|Packaging", meta = (EditCondition = "!bAutoApplyParcelData", EditConditionHides))
	UStaticMesh* DefaultUnpackagedMesh = nullptr;

	/** 원본 메시 저장 (포장 해제용) */
	UPROPERTY()
	UStaticMesh* OriginalMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel")
	AActor* ConveyorActor = nullptr;

	/** Parcel price awarded on submission */
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Parcel|Economy")
	int32 ParcelPrice = 0;

	/** Classification gameplay tag */
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Parcel|Tags")
	FGameplayTag ParcelClassificationTag;

	// Item properties (포장되지 않은 택배 = 아이템)
	/** Whether this parcel is an item (unpackaged state) */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Item")
	bool bIsItem = false;

	/** Whether this item can be used */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Item")
	bool bIsUsable = false;

	/** Item type */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Item")
	EItemType ItemType = EItemType::Unknown;

	/** Item data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FItemData ItemData;

	/** Story/특수 아이템 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	bool bIsSpecialItem = false;

	/** 특수 아이템 태그 (스토리/효과 구분용) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FGameplayTagContainer SpecialItemTags;

	// Physics stabilization
	UPROPERTY()
	FTimerHandle StabilizeTimerHandle;

	// HUD settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	TSubclassOf<UParcelHUDWidget> HUDWidgetClass;

	UPROPERTY()
	UParcelHUDWidget* HUDWidget = nullptr;

	/** 상호작용 타겟 상태 (로컬 전용) */
	bool bIsTargetedByLocalPlayer = false;

	/** 특수 아이템 사용 시 블루프린트 처리 훅 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Item")
	void BP_OnSpecialItemUsed(ACharacter* User);
	/**
	 * Show/hide pickup widget
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel")
	void ShowPickupWidget(bool bShowWidget);

	// Debug settings
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bEnableDebugLogging = true;

	// InteractableInterface Implementation
	virtual void OnInteract_Implementation(ACharacter* Interactor);
	virtual bool CanInteract_Implementation(ACharacter* Interactor);
	virtual FText GetInteractText_Implementation();
	virtual void StartHighlight_Implementation();
	virtual void EndHighlight_Implementation() override;

private:
	/** 하이라이트를 위한 원본 머티리얼 저장 */
	UPROPERTY()
	TArray<UMaterialInterface*> OriginalMaterials;

	/** 하이라이트 머티리얼 */
	UPROPERTY(EditAnywhere, Category = "Interaction")
	UMaterialInterface* HighlightMaterial = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Weapon Properties")
	USkeletalMeshComponent* ParcelMesh;

	UPROPERTY()
	float LastImpactTime = -100.0f;

	/** 최근 드랍한 캐릭터 (드랍 충격 의심 판정용) */
	UPROPERTY()
	TWeakObjectPtr<class ABlasterCharacter> PendingDropper;

	/** 드랍 직후 충격 의심 판정을 대기 중인지 */
	UPROPERTY()
	bool bPendingDropSuspicion = false;

	/** 플레이어 드랍 이후 1회 충격만 데미지 허용 */
	UPROPERTY()
	bool bImpactDamageEnabled = false;

	/** 컨베이어에 실려있는 동안 충돌 데미지 무시용 */
	UPROPERTY()
	bool bOnConveyor = false;

	UPROPERTY()
	TWeakObjectPtr<AShelfActor> OccupyingShelf;

	UPROPERTY()
	int32 OccupyingShelfSlotIndex = INDEX_NONE;

	/** Active loop audio component for spill/leak effects */
	UPROPERTY()
	class UAudioComponent* ActiveLoopAudioComponent = nullptr;

	/** Active loop Niagara component for spill/leak effects */
	UPROPERTY()
	class UNiagaraComponent* ActiveLoopVfxComponent = nullptr;

public:
	/**
	 * Begin leak/spill loop effects
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel|AV")
	void BeginLeakLoop();

	/**
	 * End leak/spill loop effects
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel|AV")
	void EndLeakLoop();

public:
	FORCEINLINE UStaticMeshComponent* GetParcelMesh() const { return MeshComponent; }
	AShelfActor* GetOccupyingShelf() const { return OccupyingShelf.Get(); }
	int32 GetOccupyingShelfSlot() const { return OccupyingShelfSlotIndex; }
	void AssignToShelf(AShelfActor* Shelf, int32 SlotIndex);
	void ClearShelfAssignment(AShelfActor* Shelf);
	const TArray<FParcelPackageContent>& GetPackageContents() const { return PackageContents; }
	void SetOnConveyor(bool bInOnConveyor) { bOnConveyor = bInOnConveyor; }
	bool IsOnConveyor() const { return bOnConveyor; }
	void SetImpactDamageEnabled(bool bEnabled) { bImpactDamageEnabled = bEnabled; }

protected:
	/** 서브클래스에서 메시 업데이트를 위해 사용 */
	bool UpdateMeshForCurrentPackagingState();

private:
	bool Handle_UseItem(class ACharacter* User);
	bool ApplyParcelConfigFromDataAssetInternal(bool bInitializeRuntime, bool bLogWarnings);
	bool TryResolveParcelConfig(FParcelConfig& OutConfig, bool bLogWarnings) const;
	void ApplyParcelConfigVisuals(const FParcelConfig& Config);
	void UpdatePackagedConfigFromContents();

	// Whether to use two-hand carry animations (derived from carry socket count)
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Parcel|Carry")
	bool bRequiresTwoHandCarry = false;

public:
    /**
     * Provide row options for ParcelDefinitionRowName based on ParcelDataAsset contents
     */
    UFUNCTION(BlueprintCallable, Category = "Parcel|Config")
    TArray<FName> GetParcelRowOptions() const;
};
