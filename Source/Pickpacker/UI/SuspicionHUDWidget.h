// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "SuspicionHUDWidget.generated.h"

/**
 * Suspicion HUD Widget - Displays team suspicion level
 */
UCLASS()
class PICKPACKER_API USuspicionHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspicionHUDWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * Update suspicion level display
	 */
	UFUNCTION(BlueprintCallable, Category = "Suspicion HUD")
	void UpdateSuspicionLevel(float SuspicionLevel);

	/**
	 * Update suspicion value display
	 */
	UFUNCTION(BlueprintCallable, Category = "Suspicion HUD")
	void UpdateSuspicionValue(float Suspicion, float MaxSuspicion);

	/**
	 * Set warning state
	 */
	UFUNCTION(BlueprintCallable, Category = "Suspicion HUD")
	void SetWarningState(bool bWarning);

public:
	/** Suspicion progress bar */
	UPROPERTY(meta = (BindWidget))
	UProgressBar* SuspicionBar;

	/** Suspicion text */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* SuspicionText;

	/** Warning indicator */
	UPROPERTY(meta = (BindWidget))
	UImage* WarningIndicator;

	/** Warning text */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* WarningText;

protected:
	/**
	 * Get color for suspicion level
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Suspicion HUD")
	FLinearColor GetSuspicionColor(float SuspicionLevel) const;

private:
	/** Warning threshold */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspicion HUD", meta = (AllowPrivateAccess = "true"))
	float WarningThreshold = 0.7f;

	/** Critical threshold */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suspicion HUD", meta = (AllowPrivateAccess = "true"))
	float CriticalThreshold = 0.9f;
};

