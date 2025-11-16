// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
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

public:
	/** Interaction text */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* InteractionText;

	/** Prompt background */
	UPROPERTY(meta = (BindWidget))
	UImage* PromptBackground;

	/** Key icon (E key) */
	UPROPERTY(meta = (BindWidget))
	UImage* KeyIcon;
};

