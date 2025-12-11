// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Styling/SlateBrush.h"
#include "Blaster/Interaction/InteractionUIData.h"
#include "InteractionPromptWidget.generated.h"

/**
 * Interaction Prompt Widget - Displays interaction prompt when player looks at interactable objects
 */
UCLASS()
class BLASTER_API UInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UInteractionPromptWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionWidgetUpdated, FInteractionUIData, Data);
	UPROPERTY(BlueprintAssignable, Category = "Interaction Prompt")
	FOnInteractionWidgetUpdated OnInteractionWidgetUpdated;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInteractionWidgetCleared);
	UPROPERTY(BlueprintAssignable, Category = "Interaction Prompt")
	FOnInteractionWidgetCleared OnInteractionWidgetCleared;

	/**
	 * Update interaction text
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction Prompt")
	void UpdateInteractionText(const FText& InteractText);

	/**
	 * Show/hide prompt
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction Prompt")
	void SetPromptVisibility(bool bVisible);

	/**
	 * Set prompt position (for screen space)
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction Prompt")
	void SetPromptPosition(const FVector2D& Position);

	/**
	 * Update credit unlock information
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction Prompt")
	void UpdateCreditInfo(bool bRequiresUnlock, int32 UnlockCost, const FText& LockedMessage, const FText& UnlockedMessage, int32 CurrentCredits);

	/** 새 UI 데이터로 전체 갱신 */
	UFUNCTION(BlueprintCallable, Category = "Interaction Prompt")
	void UpdateFromInteractionData(const FInteractionUIData& Data, const FText& InputKeyText);

	/** UI 초기화 */
	UFUNCTION(BlueprintCallable, Category = "Interaction Prompt")
	void ClearInteractionData();

	/**
	 * Clear credit information
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction Prompt")
	void ClearCreditInfo();

public:
	/** Interaction text */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* InteractionText;

	/** Prompt background */
	UPROPERTY(meta = (BindWidget))
	UImage* PromptBackground;

	// Key icon brush (locked state)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Prompt|Icons")
	FSlateBrush LockedKeyBrush;

	/** Credit cost text (optional) */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* CreditCostText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Prompt|Icons")
	UTextBlock* InformationText;
};

