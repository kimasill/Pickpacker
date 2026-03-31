// Fill out your copyright notice in the Description page of Project Settings.

#include "SuspicionHUDWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

USuspicionHUDWidget::USuspicionHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WarningThreshold = 0.7f;
	CriticalThreshold = 0.9f;
}

void USuspicionHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SuspicionBar)
	{
		SuspicionBar->SetPercent(0.0f);
	}

	if (SuspicionText)
	{
		SuspicionText->SetText(FText::FromString(TEXT("Suspicion: 0%")));
	}

	if (WarningIndicator)
	{
		WarningIndicator->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (WarningText)
	{
		WarningText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USuspicionHUDWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void USuspicionHUDWidget::UpdateSuspicionLevel(float SuspicionLevel)
{
	if (SuspicionBar)
	{
		SuspicionBar->SetPercent(SuspicionLevel);
		FLinearColor Color = GetSuspicionColor(SuspicionLevel);
		SuspicionBar->SetFillColorAndOpacity(Color);
	}

	if (SuspicionText)
	{
		int32 Percentage = FMath::RoundToInt(SuspicionLevel * 100.0f);
		SuspicionText->SetText(FText::FromString(FString::Printf(TEXT("Suspicion: %d%%"), Percentage)));
	}

	// Update warning state
	SetWarningState(SuspicionLevel >= WarningThreshold);
}

void USuspicionHUDWidget::UpdateSuspicionValue(float Suspicion, float MaxSuspicion)
{
	float SuspicionLevel = MaxSuspicion > 0.0f ? FMath::Clamp(Suspicion / MaxSuspicion, 0.0f, 1.0f) : 0.0f;
	UpdateSuspicionLevel(SuspicionLevel);
}

void USuspicionHUDWidget::SetWarningState(bool bWarning)
{
	if (WarningIndicator)
	{
		WarningIndicator->SetVisibility(bWarning ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (WarningText)
	{
		WarningText->SetVisibility(bWarning ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bWarning)
		{
			WarningText->SetText(FText::FromString(TEXT("WARNING: High Suspicion!")));
		}
	}
}

FLinearColor USuspicionHUDWidget::GetSuspicionColor(float SuspicionLevel) const
{
	if (SuspicionLevel >= CriticalThreshold)
	{
		return FLinearColor::Red;
	}
	else if (SuspicionLevel >= WarningThreshold)
	{
		return FLinearColor::Yellow;
	}
	else
	{
		return FLinearColor::Green;
	}
}

