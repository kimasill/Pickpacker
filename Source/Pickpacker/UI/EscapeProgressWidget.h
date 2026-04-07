// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DataAssets/DA_ItemData.h"
#include "Escape/EscapeZoneActor.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "EscapeProgressWidget.generated.h"

/**
 * Escape Progress Widget - Displays escape progress and requirements
 */
UCLASS()
class PICKPACKER_API UEscapeProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UEscapeProgressWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;

	/**
	 * Update escape progress
	 */
	UFUNCTION(BlueprintCallable, Category = "Escape Progress")
	void UpdateEscapeProgress(float Progress);

	/**
	 * Update escape requirements
	 */
	UFUNCTION(BlueprintCallable, Category = "Escape Progress")
	void UpdateEscapeRequirements(const TArray<FEscapeRequirement>& Requirements, class ACharacter* Player);

	/**
	 * Show/hide widget
	 */
	UFUNCTION(BlueprintCallable, Category = "Escape Progress")
	void SetWidgetVisibility(bool bVisible);

public:
	/** Overall progress bar */
	UPROPERTY(meta = (BindWidget))
	UProgressBar* OverallProgressBar;

	/** Progress text */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* ProgressText;

	/** Requirements scroll box */
	UPROPERTY(meta = (BindWidget))
	UScrollBox* RequirementsScrollBox;

	/** Requirements text template */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Escape Progress")
	TSubclassOf<UTextBlock> RequirementTextTemplate;
};

