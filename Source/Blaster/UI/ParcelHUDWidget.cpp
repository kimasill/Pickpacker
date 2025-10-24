// Fill out your copyright notice in the Description page of Project Settings.

#include "ParcelHUDWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

UParcelHUDWidget::UParcelHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Initialize default colors
	HealthyColor = FLinearColor::Green;
	WarningColor = FLinearColor::Yellow;
	CriticalColor = FLinearColor::Red;
	StableColor = FLinearColor::Blue;
	UnstableColor = FLinearColor::Red;

	// Initialize animation settings
	ColorTransitionSpeed = 2.0f;
	bEnableColorTransitions = true;
	bEnableDebugLogging = true;

	// Initialize current state
	CurrentParcelType = EParcelType::None;
	CurrentParcelState = FParcelState();
}

void UParcelHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize UI elements
	if (DurabilityBar)
	{
		DurabilityBar->SetPercent(1.0f);
		DurabilityBar->SetFillColorAndOpacity(HealthyColor);
	}

	if (InstabilityBar)
	{
		InstabilityBar->SetPercent(0.0f);
		InstabilityBar->SetFillColorAndOpacity(StableColor);
	}

	if (WeightText)
	{
		WeightText->SetText(FText::FromString(TEXT("Weight: 1.0")));
	}

	if (CarrierCountText)
	{
		CarrierCountText->SetText(FText::FromString(TEXT("Carriers: 0")));
	}

	if (ParcelTypeText)
	{
		ParcelTypeText->SetText(FText::FromString(TEXT("Unknown")));
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelHUDWidget] Widget constructed"));
	}
}

void UParcelHUDWidget::NativeDestruct()
{
	Super::NativeDestruct();

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelHUDWidget] Widget destructed"));
	}
}

void UParcelHUDWidget::UpdateParcelState(const FParcelState& ParcelState, EParcelType ParcelType)
{
	CurrentParcelState = ParcelState;
	CurrentParcelType = ParcelType;

	// Update all UI elements
	UpdateDurabilityBar(ParcelState.Durability, 100.0f);
	UpdateWeightDisplay(ParcelState.Weight);
	UpdateInstabilityBar(ParcelState.Instability, 100.0f);
	UpdateCarrierCount(ParcelState.Carriers.Num());
	SetParcelType(ParcelType);
	UpdateHUDColor(ParcelState);

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelHUDWidget] Parcel state updated - Durability: %.2f, Weight: %.2f, Instability: %.2f, Carriers: %d"),
			ParcelState.Durability, ParcelState.Weight, ParcelState.Instability, ParcelState.Carriers.Num());
	}
}

void UParcelHUDWidget::SetParcelType(EParcelType ParcelType)
{
	CurrentParcelType = ParcelType;

	if (ParcelTypeText)
	{
		ParcelTypeText->SetText(GetParcelTypeText(ParcelType));
	}

	if (ParcelTypeIcon)
	{
		UTexture2D* IconTexture = GetParcelTypeIcon(ParcelType);
		if (IconTexture)
		{
			ParcelTypeIcon->SetBrushFromTexture(IconTexture);
		}
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelHUDWidget] Parcel type set to: %s"), *UEnum::GetValueAsString(ParcelType));
	}
}

void UParcelHUDWidget::UpdateDurabilityBar(float Durability, float MaxDurability)
{
	if (!DurabilityBar)
	{
		return;
	}

	float DurabilityPercent = FMath::Clamp(Durability / MaxDurability, 0.0f, 1.0f);
	DurabilityBar->SetPercent(DurabilityPercent);

	// Update color based on durability
	FLinearColor DurabilityColor = GetDurabilityColor(Durability, MaxDurability);
	DurabilityBar->SetFillColorAndOpacity(DurabilityColor);

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("[ParcelHUDWidget] Durability bar updated - Value: %.2f, Percent: %.2f"),
			Durability, DurabilityPercent);
	}
}

void UParcelHUDWidget::UpdateWeightDisplay(float Weight)
{
	if (!WeightText)
	{
		return;
	}

	FText WeightTextContent = FText::Format(FText::FromString(TEXT("Weight: {0}")), FText::AsNumber(Weight));
	WeightText->SetText(WeightTextContent);

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("[ParcelHUDWidget] Weight display updated - Value: %.2f"), Weight);
	}
}

void UParcelHUDWidget::UpdateInstabilityBar(float Instability, float MaxInstability)
{
	if (!InstabilityBar)
	{
		return;
	}

	float InstabilityPercent = FMath::Clamp(Instability / MaxInstability, 0.0f, 1.0f);
	InstabilityBar->SetPercent(InstabilityPercent);

	// Update color based on instability
	FLinearColor InstabilityColor = GetInstabilityColor(Instability, MaxInstability);
	InstabilityBar->SetFillColorAndOpacity(InstabilityColor);

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("[ParcelHUDWidget] Instability bar updated - Value: %.2f, Percent: %.2f"),
			Instability, InstabilityPercent);
	}
}

void UParcelHUDWidget::UpdateCarrierCount(int32 CarrierCount)
{
	if (!CarrierCountText)
	{
		return;
	}

	FText CarrierTextContent = FText::Format(FText::FromString(TEXT("Carriers: {0}")), FText::AsNumber(CarrierCount));
	CarrierCountText->SetText(CarrierTextContent);

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("[ParcelHUDWidget] Carrier count updated - Count: %d"), CarrierCount);
	}
}

void UParcelHUDWidget::SetHUDVisibility(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelHUDWidget] HUD visibility set to: %s"), bVisible ? TEXT("Visible") : TEXT("Hidden"));
	}
}

void UParcelHUDWidget::UpdateHUDColor(const FParcelState& ParcelState)
{
	if (!BackgroundImage)
	{
		return;
	}

	// Determine overall HUD color based on parcel state
	FLinearColor HUDColor = FLinearColor::White;

	// Check if parcel is critical
	if (ParcelState.Durability <= 25.0f)
	{
		HUDColor = CriticalColor;
	}
	else if (ParcelState.Durability <= 50.0f)
	{
		HUDColor = WarningColor;
	}
	else if (ParcelState.Instability > 50.0f)
	{
		HUDColor = UnstableColor;
	}
	else
	{
		HUDColor = HealthyColor;
	}

	BackgroundImage->SetColorAndOpacity(HUDColor);

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("[ParcelHUDWidget] HUD color updated"));
	}
}

FLinearColor UParcelHUDWidget::GetDurabilityColor(float Durability, float MaxDurability) const
{
	float DurabilityPercent = Durability / MaxDurability;

	if (DurabilityPercent <= 0.25f)
	{
		return CriticalColor;
	}
	else if (DurabilityPercent <= 0.5f)
	{
		return WarningColor;
	}
	else
	{
		return HealthyColor;
	}
}

FLinearColor UParcelHUDWidget::GetInstabilityColor(float Instability, float MaxInstability) const
{
	float InstabilityPercent = Instability / MaxInstability;

	if (InstabilityPercent >= 0.75f)
	{
		return UnstableColor;
	}
	else if (InstabilityPercent >= 0.5f)
	{
		return WarningColor;
	}
	else
	{
		return StableColor;
	}
}

UTexture2D* UParcelHUDWidget::GetParcelTypeIcon(EParcelType ParcelType) const
{
	switch (ParcelType)
	{
	case EParcelType::Fragile:
		return FragileIcon;
	case EParcelType::Heavy:
		return HeavyIcon;
	case EParcelType::Unstable:
		return UnstableIcon;
	default:
		return nullptr;
	}
}

FText UParcelHUDWidget::GetParcelTypeText(EParcelType ParcelType) const
{
	switch (ParcelType)
	{
	case EParcelType::Fragile:
		return FText::FromString(TEXT("Fragile"));
	case EParcelType::Heavy:
		return FText::FromString(TEXT("Heavy"));
	case EParcelType::Unstable:
		return FText::FromString(TEXT("Unstable"));
	default:
		return FText::FromString(TEXT("Unknown"));
	}
}


