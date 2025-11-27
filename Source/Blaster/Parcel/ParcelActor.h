// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Blaster/DataAssets/DA_ParcelData.h"
#include "Blaster/DataAssets/DA_ItemData.h"
#include "Blaster/Components/ParcelStateComponent.h"
#include "Blaster/Components/CarryPointsComponent.h"
#include "Blaster/UI/ParcelHUDWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameplayTagContainer.h"
#include "Blaster/Components/InteractionComponent.h"
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
class BLASTER_API AParcelActor : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AParcelActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

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

	/** Item gameplay tag */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel")
	FGameplayTag GetItemTag() const { return ParcelItemTag; }

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
	 * Multicast RPC for attach event
	 */
	UFUNCTION(NetMulticast, Unreliable, Category = "Parcel")
	void Multicast_ParcelAttached(ACharacter* Carrier, FName SocketId);

	/**
	 * Multicast RPC for drop event
	 */
	UFUNCTION(NetMulticast, Unreliable, Category = "Parcel")
	void Multicast_ParcelDropped(ACharacter* Carrier, FVector DropLocation);

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config", meta = (AllowPrivateAccess = "true", Categories = "Parcel"))
	FGameplayTag ParcelDefinitionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Config", meta = (EditCondition = "!bAutoApplyParcelData", EditConditionHides))
	FParcelConfig ParcelConfig;

	// Parcel tags
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Config")
	FGameplayTagContainer ParcelTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Config")
	bool PackageOnSpawn = true;

	// Physics settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float DropStabilizeTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float DropImpulseMultiplier = 1.0f;

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

	/** Item gameplay tag */
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Parcel|Tags")
	FGameplayTag ParcelItemTag;

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

	// Physics stabilization
	UPROPERTY()
	FTimerHandle StabilizeTimerHandle;

	// HUD settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	TSubclassOf<UParcelHUDWidget> HUDWidgetClass;

	UPROPERTY()
	UParcelHUDWidget* HUDWidget = nullptr;

	/**
	 * Show/hide pickup widget
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel")
	void ShowPickupWidget(bool bShowWidget);

	// Debug settings
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bEnableDebugLogging = true;

	// InteractableInterface Implementation
	virtual bool OnInteract_Implementation(ACharacter* Interactor) override;
	virtual bool CanInteract_Implementation(ACharacter* Interactor) const override;
	virtual FText GetInteractText_Implementation() const override;
	virtual void StartHighlight_Implementation() override;
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
	TWeakObjectPtr<AShelfActor> OccupyingShelf;

	UPROPERTY()
	int32 OccupyingShelfSlotIndex = INDEX_NONE;	

public:
	FORCEINLINE UStaticMeshComponent* GetParcelMesh() const { return MeshComponent; }
	AShelfActor* GetOccupyingShelf() const { return OccupyingShelf.Get(); }
	int32 GetOccupyingShelfSlot() const { return OccupyingShelfSlotIndex; }
	void AssignToShelf(AShelfActor* Shelf, int32 SlotIndex);
	void ClearShelfAssignment(AShelfActor* Shelf);

private:
	bool Handle_UseItem(class ACharacter* User);
	bool ApplyParcelConfigFromDataAssetInternal(bool bInitializeRuntime, bool bLogWarnings);
	bool TryResolveParcelConfig(FParcelConfig& OutConfig, bool bLogWarnings) const;
	void ApplyParcelConfigVisuals(const FParcelConfig& Config);
	bool UpdateMeshForCurrentPackagingState();
};
