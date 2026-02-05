// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blaster/DataAssets/DA_ParcelData.h"
#include "PickpackerTypes/PickpackerTypes.h"
#include "GameplayTagContainer.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Engine/DataTable.h"
#include "Blaster/UI/ParcelTagDisplayData.h"
#include "ParcelHUDWidget.generated.h"

USTRUCT(BlueprintType)
struct BLASTER_API FParcelHUDStyle
{
	GENERATED_BODY()

	// Durability colors
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Colors")
	FLinearColor HealthyColor = FLinearColor(0.486f, 1.0f, 0.486f, 1.0f); // #7CFF7C

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Colors")
	FLinearColor WarningColor = FLinearColor(1.0f, 0.75f, 0.25f, 1.0f);   // 노랑-주황

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Colors")
	FLinearColor CriticalColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);     // 빨강

	// Instability colors (안정 → 파랑, 불안정 → 보라/빨강)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Colors")
	FLinearColor StableColor = FLinearColor(0.3f, 0.6f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Colors")
	FLinearColor UnstableColor = FLinearColor(0.7f, 0.0f, 0.8f, 1.0f);

	// Background tints (Border Brush Tint)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Colors")
	FLinearColor BackgroundHealthyTint = FLinearColor(0.3f, 1.0f, 0.3f, 0.12f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Colors")
	FLinearColor BackgroundWarningTint = FLinearColor(1.0f, 0.6f, 0.2f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Colors")
	FLinearColor BackgroundCriticalTint = FLinearColor(1.0f, 0.1f, 0.1f, 0.28f);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnParcelContentListUpdated, bool, bListed);

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
	void UpdateParcelState(const FParcelState& ParcelState, const FGameplayTag& ParcelTag, const FGameplayTag& ClassificationTag, float MaxDurability = 100.0f);

	/**
	 * Set parcel type display
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel HUD")
	void SetClassificationTag(const FGameplayTag& ClassificationTag);

	UFUNCTION(BlueprintCallable, Category = "Parcel HUD")
	void SetParcelTag(const FGameplayTag& ParcelTag);

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
	* Update unit display
	*/
	UFUNCTION(BlueprintCallable, Category = "Parcel HUD")
	void UpdateUnitDisplay(int32 Count);

	/**
	 * Update contents list display (packed parcel)
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel HUD")
	void UpdateContentsList(const TArray<FText>& Contents);

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
	void UpdateHUDColor(const FParcelState& ParcelState, float MaxDurability = 100.0f);

	/** 최소 표시 모드 토글 (집은 상태 등) */
	UFUNCTION(BlueprintCallable, Category = "Parcel HUD")
	void SetMinimalDisplay(bool bMinimal);

	UFUNCTION(BlueprintCallable, Category = "Parcel HUD")
	void SetParcelName(const FText& InName);


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
	UTexture2D* GetClassificationIcon(const FGameplayTag& ClassificationTag) const;

	/**
	 * Get parcel type text
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel HUD")
	FText GetClassificationText(const FGameplayTag& ClassificationTag) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parcel HUD")
	FText GetParcelTagText(const FGameplayTag& ParcelTag) const;

	/**
	 * Extract the last part of a gameplay tag (e.g., "Pickpacker.Parcel.Classification.Standard" -> "Standard")
	 */
	FString GetTagLastPart(const FGameplayTag& Tag) const;


public:
	// UI Components
	UPROPERTY(meta = (BindWidget))
	UProgressBar* DurabilityBar;

	// Optional instability/weight/carrier displays
	UPROPERTY(meta = (BindWidget))
	UProgressBar* InstabilityBar;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* WeightText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* UnitText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* CarrierCountText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ParcelTypeText;

	UPROPERTY(meta = (BindWidget))
	UImage* ParcelTypeIcon;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ParcelTagText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ParcelNameText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ContentsListText;

	UPROPERTY(meta = (BindWidget))
	UBorder* BackgroundBorder;

	// 태그 표시용 데이터 테이블 (Row: FParcelHUDTagDisplayRow)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel HUD")
	UDataTable* ClassificationDisplayTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel HUD")
	UDataTable* ParcelTagDisplayTable = nullptr;


	UPROPERTY(BlueprintAssignable, Category = "Parcel HUD")
	FOnParcelContentListUpdated OnParcelContentListUpdated;

protected:
	// Current parcel state
	UPROPERTY()
	FParcelState CurrentParcelState;

	UPROPERTY()
	FGameplayTag CurrentClassificationTag;

	// Style (BP에서 색/아이콘/배경 Tint 조정)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD Style")
	FParcelHUDStyle HUDStyle;

	// Parcel type icons
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Icons")
	UTexture2D* StandardIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Icons")
	UTexture2D* FragileIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Icons")
	UTexture2D* ContrabandIcon;

	// Animation settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float ColorTransitionSpeed = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bEnableColorTransitions = true;

	// Debug settings
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bEnableDebugLogging = true;

	// Simplification toggles
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Visibility")
	bool bShowInstability = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Visibility")
	bool bShowCarrierCount = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Visibility")
	bool bShowWeight = true;

	// Optional movement multiplier display (derived from weight)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Visibility")
	bool bShowMovementMultiplier = false;

	// Minimal 모드에서 남길 요소
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Visibility")
	bool bMinimalShowWeight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Visibility")
	bool bMinimalShowClassification = true;

private:
	bool bMinimalMode = false;
};


