// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blaster/DataAssets/DA_ParcelData.h"
#include "Blaster/PickpackerTypes/PickpackerTypes.h"
#include "ParcelStateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnParcelStateChanged, const FParcelState&, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnParcelDurabilityChanged, float, OldDurability, float, NewDurability);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnParcelInstabilityChanged, float, OldInstability, float, NewInstability);

/**
 * Parcel State Component - Manages parcel durability, weight, and instability
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BLASTER_API UParcelStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UParcelStateComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Initialize parcel with configuration
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel State")
	void InitializeParcel(const FParcelConfig& Config);

	/**
	 * Apply damage to parcel
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel State")
	void ApplyDamage(float DamageAmount, const FString& DamageSource = TEXT("Unknown"));

	/**
	 * Apply impact damage (for fragile parcels)
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel State")
	void ApplyImpactDamage(float ImpactForce, const FString& ImpactSource = TEXT("Impact"));

	/**
	 * Update instability (for contraband parcels)
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel State")
	void UpdateInstability(float DeltaTime);

	/**
	 * Apply heavy parcel movement penalty
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel State")
	float GetMovementSpeedMultiplier() const;

	/**
	 * Check if jumping is allowed (for heavy parcels)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel State")
	bool CanJump() const;

	/**
	 * Get instability decay rate
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel State")
	float GetInstabilityDecayRate() const;

	/**
	 * Apply two-person carry bonuses
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel State")
	void ApplyTwoPersonCarryBonuses(bool bTwoPersonCarry);

	/**
	 * Get effective movement speed multiplier (including two-person bonuses)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel State")
	float GetEffectiveMovementSpeedMultiplier() const;

	/**
	 * Check if parcel is broken
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel State")
	bool IsBroken() const { return CurrentState.Durability <= 0.0f; }

	/**
	 * Check if parcel is critical
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel State")
	bool IsCritical() const { return CurrentState.Durability <= 25.0f; }

	/**
	 * Get current parcel state
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel State")
	const FParcelState& GetParcelState() const { return CurrentState; }

	/**
	 * Get parcel type
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel State")
	EParcelType GetParcelType() const { return ParcelConfig.ParcelType; }

	/**
	 * Get parcel configuration
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel State")
	const FParcelConfig& GetParcelConfig() const { return ParcelConfig; }

	/**
	 * Set parcel attached state
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel State")
	void SetAttachedState(bool bAttached, const FName& SocketId = NAME_None);

	/**
	 * Set parcel state (for restoring from packaged box)
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel State")
	void SetParcelState(const FParcelState& NewState);

	/**
	 * Set the internal item data stored within this parcel
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel State")
	void SetInternalItemData(const FItemData& ItemData);

	/**
	 * Get stored item data
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel State")
	const FItemData& GetInternalItemData() const { return InternalItemData; }

protected:
	/**
	 * Called when parcel state is replicated
	 */
	UFUNCTION()
	void OnRep_ParcelState();

	/**
	 * Called when durability changes
	 */
	UFUNCTION()
	void OnRep_Durability();

	/**
	 * Called when instability changes
	 */
	UFUNCTION()
	void OnRep_Instability();

	/**
	 * Called when internal item data changes
	 */
	UFUNCTION()
	void OnRep_InternalItemData();

	/**
	 * Calculate damage based on parcel type
	 */
	float CalculateDamage(float BaseDamage, const FString& DamageSource);

public:
	// Delegates
	UPROPERTY(BlueprintAssignable, Category = "Parcel State")
	FOnParcelStateChanged OnParcelStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Parcel State")
	FOnParcelDurabilityChanged OnDurabilityChanged;

	UPROPERTY(BlueprintAssignable, Category = "Parcel State")
	FOnParcelInstabilityChanged OnInstabilityChanged;

protected:
	// Replicated parcel state
	UPROPERTY(ReplicatedUsing = OnRep_ParcelState)
	FParcelState CurrentState;

	// Internal item data stored with the parcel
	UPROPERTY(ReplicatedUsing = OnRep_InternalItemData, BlueprintReadOnly, Category = "Parcel State")
	FItemData InternalItemData;

	// Parcel configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Config")
	FParcelConfig ParcelConfig;

	// Damage tracking
	UPROPERTY()
	float LastDamageTime = 0.0f;

	UPROPERTY()
	FString LastDamageSource = TEXT("");

	// Instability timer
	UPROPERTY()
	float InstabilityTimer = 0.0f;

	// Movement penalty settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float HeavyMovementPenalty = 0.75f; // 25% speed reduction

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool bHeavyBlocksJump = true;

	// Impact damage settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	float FragileImpactMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	float ImpactDamageThreshold = 50.0f;

	// Instability settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instability")
	float UnstableDecayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instability")
	float InstabilityFailureThreshold = 100.0f;

	// Two-person carry bonuses
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Two-Person Carry")
	float TwoPersonStabilityBonus = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Two-Person Carry")
	float TwoPersonMovementBonus = 0.15f;

	// Current two-person carry state
	UPROPERTY()
	bool bIsTwoPersonCarry = false;

	// Debug settings
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bEnableDebugLogging = true;
};
