// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Blaster/DataAssets/DA_ParcelData.h"
#include "Blaster/Components/ParcelStateComponent.h"
#include "Blaster/Components/CarryPointsComponent.h"
#include "Blaster/UI/ParcelHUDWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameplayTagContainer.h"
#include "ParcelActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnParcelAttached, class ACharacter*, Carrier, FName, SocketId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnParcelDropped, FVector, DropLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnParcelBroken, class AParcelActor*, Parcel);

/**
 * Parcel Actor - Represents a package that can be carried by players
 */
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API AParcelActor : public AActor
{
	GENERATED_BODY()

public:
	AParcelActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/**
	 * Initialize parcel with configuration
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel")
	void InitializeParcel(const FParcelConfig& Config);

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
	 * Get parcel type
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel")
	EParcelType GetParcelType() const;

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
	void Multicast_ParcelDropped(FVector DropLocation);

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

public:
	// Delegates
	UPROPERTY(BlueprintAssignable, Category = "Parcel")
	FOnParcelAttached OnParcelAttached;

	UPROPERTY(BlueprintAssignable, Category = "Parcel")
	FOnParcelDropped OnParcelDropped;

	UPROPERTY(BlueprintAssignable, Category = "Parcel")
	FOnParcelBroken OnParcelBroken;

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

	UPROPERTY(VisibleAnywhere, Category = "Weapon Properties")
	class UWidgetComponent* PickupWidget;
	// Parcel configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Config")
	FParcelConfig ParcelConfig;

	// Parcel tags
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Config")
	FGameplayTagContainer ParcelTags;

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

	// Physics stabilization
	UPROPERTY()
	FTimerHandle StabilizeTimerHandle;

	// HUD settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	TSubclassOf<UParcelHUDWidget> HUDWidgetClass;

	UPROPERTY()
	UParcelHUDWidget* HUDWidget = nullptr;

	// Debug settings
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bEnableDebugLogging = true;
};
