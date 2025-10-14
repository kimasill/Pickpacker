// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blaster/PickpackerTypes/PickpackerTypes.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "ParcelHUDWidget.generated.h"

/**
 * Parcel HUD Widget - Displays parcel state information
 */
UCLASS()
class BLASTER_API UParcelHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UParcelHUDWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * Update parcel state display
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel HUD")
	void UpdateParcelState(const FParcelState& ParcelState, EParcelType ParcelType);

	/**
	 * Set parcel type display
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel HUD")
	void SetParcelType(EParcelType ParcelType);

	/**
	 * Update durability bar
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel HUD")
	void UpdateDurabilityBar(float Durability, float MaxDurability = 100.0f);

	/**
	 * Update weight display
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel HUD")
	void UpdateWeightDisplay(float Weight);

	/**
	 * Update instability bar
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel HUD")
	void UpdateInstabilityBar(float Instability, float MaxInstability = 100.0f);

	/**
	 * Update carrier count display
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel HUD")
	void UpdateCarrierCount(int32 CarrierCount);

	/**
	 * Show/hide HUD
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel HUD")
	void SetHUDVisibility(bool bVisible);

	/**
	 * Set HUD color based on parcel state
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel HUD")
	void UpdateHUDColor(const FParcelState& ParcelState);

protected:
	/**
	 * Get color for durability bar
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel HUD")
	FLinearColor GetDurabilityColor(float Durability, float MaxDurability) const;

	/**
	 * Get color for instability bar
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel HUD")
	FLinearColor GetInstabilityColor(float Instability, float MaxInstability) const;

	/**
	 * Get parcel type icon
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel HUD")
	UTexture2D* GetParcelTypeIcon(EParcelType ParcelType) const;

	/**
	 * Get parcel type text
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel HUD")
	FText GetParcelTypeText(EParcelType ParcelType) const;

public:
	// UI Components
	UPROPERTY(meta = (BindWidget))
	UProgressBar* DurabilityBar;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* InstabilityBar;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* WeightText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* CarrierCountText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ParcelTypeText;

	UPROPERTY(meta = (BindWidget))
	UImage* ParcelTypeIcon;

	UPROPERTY(meta = (BindWidget))
	UImage* BackgroundImage;

protected:
	// Current parcel state
	UPROPERTY()
	FParcelState CurrentParcelState;

	UPROPERTY()
	EParcelType CurrentParcelType = EParcelType::None;

	// Color settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD Colors")
	FLinearColor HealthyColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD Colors")
	FLinearColor WarningColor = FLinearColor::Yellow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD Colors")
	FLinearColor CriticalColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD Colors")
	FLinearColor StableColor = FLinearColor::Blue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD Colors")
	FLinearColor UnstableColor = FLinearColor::Red;

	// Parcel type icons
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Icons")
	UTexture2D* FragileIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Icons")
	UTexture2D* HeavyIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Icons")
	UTexture2D* UnstableIcon;

	// Animation settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float ColorTransitionSpeed = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bEnableColorTransitions = true;

	// Debug settings
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bEnableDebugLogging = true;
};
