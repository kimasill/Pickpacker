// Fill out your copyright notice in the Description page of Project Settings.

#include "ParcelHUDWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"
#include "Blaster/DataAssets/DA_ParcelData.h"
#include "GameplayTagsManager.h"
#include "Blaster/UI/ParcelTagDisplayUtils.h"

UParcelHUDWidget::UParcelHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Initialize animation settings
	ColorTransitionSpeed = 2.0f;
	bEnableColorTransitions = true;
	bEnableDebugLogging = true;

	// Initialize current state
	CurrentClassificationTag = FGameplayTag::RequestGameplayTag(TEXT("Parcel-Classification.Standard"), false);
	CurrentParcelState = FParcelState();
}

void UParcelHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize UI elements
	if (DurabilityBar)
	{
		DurabilityBar->SetPercent(1.0f);
		DurabilityBar->SetFillColorAndOpacity(HUDStyle.HealthyColor);
	}

	if (InstabilityBar)
	{
		InstabilityBar->SetPercent(0.0f);
		InstabilityBar->SetFillColorAndOpacity(HUDStyle.StableColor);
		InstabilityBar->SetVisibility(bShowInstability ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (WeightText)
	{
		WeightText->SetText(FText::FromString(TEXT("Weight: 1.0")));
		WeightText->SetVisibility(bShowWeight ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (CarrierCountText)
	{
		CarrierCountText->SetText(FText::FromString(TEXT("Carriers: 0")));
		CarrierCountText->SetVisibility(bShowCarrierCount ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (ParcelTypeText)
	{
		ParcelTypeText->SetText(FText::FromString(TEXT("Unknown")));
	}

	if (ParcelNameText)
	{
		ParcelNameText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ContentsListText)
	{
		ContentsListText->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (BackgroundBorder)
	{
		BackgroundBorder->SetBrushColor(HUDStyle.BackgroundHealthyTint);
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

void UParcelHUDWidget::UpdateParcelState(const FParcelState& ParcelState, const FGameplayTag& ParcelTag, const FGameplayTag& ClassificationTag, float MaxDurability)
{
	CurrentParcelState = ParcelState;
	CurrentClassificationTag = ClassificationTag;

	// 최소 모드에서는 제한된 정보만 갱신
	if (bMinimalMode)
	{
		// 최소 모드: 내구도/불안정도/캐리어 수 숨김, 분류와 무게만 유지
		if (ParcelTypeText && bMinimalShowClassification)
		{
			ParcelTypeText->SetVisibility(ESlateVisibility::Visible);
			ParcelTypeText->SetText(GetClassificationText(ClassificationTag));
		}
		if (ParcelTypeIcon && bMinimalShowClassification)
		{
			if (UTexture2D* IconTexture = GetClassificationIcon(ClassificationTag))
			{
				ParcelTypeIcon->SetBrushFromTexture(IconTexture);
				ParcelTypeIcon->SetVisibility(ESlateVisibility::Visible);
			}
		}
		UpdateWeightDisplay(ParcelState.Weight);
		return;
	}

	// Update all UI elements
	UpdateDurabilityBar(ParcelState.Durability, MaxDurability);
	UpdateWeightDisplay(ParcelState.Weight);
	UpdateUnitDisplay(ParcelState.Unit);
	UpdateInstabilityBar(ParcelState.Instability, 100.0f);
	UpdateCarrierCount(ParcelState.Carriers.Num());
	SetParcelTag(ParcelTag);
	SetClassificationTag(ClassificationTag);
	UpdateHUDColor(ParcelState, MaxDurability);

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelHUDWidget] Parcel state updated - Durability: %.2f, Weight: %.2f, Instability: %.2f, Carriers: %d"),
			ParcelState.Durability, ParcelState.Weight, ParcelState.Instability, ParcelState.Carriers.Num());
	}
}

void UParcelHUDWidget::SetClassificationTag(const FGameplayTag& ClassificationTag)
{
	if (ParcelTypeText)
	{
		ParcelTypeText->SetText(GetClassificationText(ClassificationTag));
	}

	if (ParcelTypeIcon)
	{
		UTexture2D* IconTexture = GetClassificationIcon(ClassificationTag);
		if (IconTexture)
		{
			ParcelTypeIcon->SetBrushFromTexture(IconTexture);
		}
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelHUDWidget] Parcel classification set to: %s"), *ClassificationTag.ToString());
	}
}

void UParcelHUDWidget::SetParcelTag(const FGameplayTag& ParcelTag)
{
	if(ParcelTagText)
	{
		ParcelTagText->SetText(GetParcelTagText(ParcelTag));
	}
	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelHUDWidget] Parcel tag set to: %s"), *ParcelTag.ToString());
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

	WeightText->SetVisibility(bShowWeight ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bShowWeight)
	{
		FText WeightTextContent = FText::Format(FText::FromString(TEXT("무게: {0}kg")), FText::AsNumber(Weight));
		WeightText->SetText(WeightTextContent);
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("[ParcelHUDWidget] Weight display updated - Value: %.2f"), Weight);
	}
}

void UParcelHUDWidget::UpdateUnitDisplay(int32 Count)
{
	if (!UnitText)
	{
		return;
	}
	FText UnitTextContent = FText::Format(FText::FromString(TEXT("크기: {0}")), FText::AsNumber(Count));
	UnitText->SetText(UnitTextContent);
	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("[ParcelHUDWidget] Unit count updated - Count: %d"), Count);
	}
}

void UParcelHUDWidget::UpdateContentsList(const TArray<FText>& Contents)
{
	if (!ContentsListText)
	{
		return;
	}

	if (Contents.Num() == 0 || bMinimalMode)
	{
		ContentsListText->SetVisibility(ESlateVisibility::Collapsed);
		ContentsListText->SetText(FText::GetEmpty());
		return;
	}

	FString Combined;
	for (int32 Index = 0; Index < Contents.Num(); ++Index)
	{
		if (Index > 0)
		{
			Combined.Append(TEXT("\n"));
		}
		Combined.Append(Contents[Index].ToString());
	}

	ContentsListText->SetText(FText::FromString(Combined));
	ContentsListText->SetVisibility(ESlateVisibility::Visible);
}

void UParcelHUDWidget::UpdateInstabilityBar(float Instability, float MaxInstability)
{
	if (!InstabilityBar)
	{
		return;
	}

	InstabilityBar->SetVisibility(bShowInstability ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bShowInstability)
	{
		float InstabilityPercent = FMath::Clamp(Instability / MaxInstability, 0.0f, 1.0f);
		InstabilityBar->SetPercent(InstabilityPercent);

		// Update color based on instability
		FLinearColor InstabilityColor = GetInstabilityColor(Instability, MaxInstability);
		InstabilityBar->SetFillColorAndOpacity(InstabilityColor);
	}

}

void UParcelHUDWidget::SetParcelName(const FText& InName)
{
	if (ParcelNameText)
	{
		const bool bHasName = !InName.IsEmpty();
		ParcelNameText->SetVisibility(bHasName ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		ParcelNameText->SetText(InName);
	}
}

void UParcelHUDWidget::SetMinimalDisplay(bool bMinimal)
{
	bMinimalMode = bMinimal;

	// 내구도/불안정도/캐리어 수는 최소 모드에서 숨김
	if (DurabilityBar)
	{
		DurabilityBar->SetVisibility(bMinimal ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (InstabilityBar)
	{
		InstabilityBar->SetVisibility(bMinimal ? ESlateVisibility::Collapsed : (bShowInstability ? ESlateVisibility::Visible : ESlateVisibility::Collapsed));
	}
	if (CarrierCountText)
	{
		CarrierCountText->SetVisibility(bMinimal ? ESlateVisibility::Collapsed : (bShowCarrierCount ? ESlateVisibility::Visible : ESlateVisibility::Collapsed));
	}

	// 무게/분류는 설정에 따라 유지
	if (WeightText)
	{
		WeightText->SetVisibility(bMinimal ? (bMinimalShowWeight ? ESlateVisibility::Visible : ESlateVisibility::Collapsed)
										   : (bShowWeight ? ESlateVisibility::Visible : ESlateVisibility::Collapsed));
	}
	if (ParcelTypeText)
	{
		ParcelTypeText->SetVisibility(bMinimal ? (bMinimalShowClassification ? ESlateVisibility::Visible : ESlateVisibility::Collapsed)
											   : ESlateVisibility::Visible);
	}
	if (ParcelTypeIcon)
	{
		ParcelTypeIcon->SetVisibility(bMinimal ? (bMinimalShowClassification ? ESlateVisibility::Visible : ESlateVisibility::Collapsed)
											   : ESlateVisibility::Visible);
	}
}

void UParcelHUDWidget::UpdateCarrierCount(int32 CarrierCount)
{
	if (!CarrierCountText)
	{
		return;
	}

	CarrierCountText->SetVisibility(bShowCarrierCount ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bShowCarrierCount)
	{
		FText CarrierTextContent = FText::Format(FText::FromString(TEXT("운반자 수: {0}")), FText::AsNumber(CarrierCount));
		CarrierCountText->SetText(CarrierTextContent);
	}

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

void UParcelHUDWidget::UpdateHUDColor(const FParcelState& ParcelState, float MaxDurability)
{
	if (!BackgroundBorder)
	{
		return;
	}

	// Determine overall HUD color based on parcel state
	FLinearColor HUDColor = FLinearColor::White;

	// 내구도 퍼센트 기준으로 임계/경고 판단
	const float SafeMax = MaxDurability > 0.0f ? MaxDurability : 100.0f;
	const float DurabilityPercent = ParcelState.Durability / SafeMax;

	// Check if parcel is critical
	if (DurabilityPercent <= 0.25f)
	{
		HUDColor = HUDStyle.BackgroundCriticalTint;
	}
	else if (DurabilityPercent <= 0.5f)
	{
		HUDColor = HUDStyle.BackgroundWarningTint;
	}
	else if (ParcelState.Instability > 50.0f)
	{
		HUDColor = HUDStyle.BackgroundWarningTint;
	}
	else
	{
		HUDColor = HUDStyle.BackgroundHealthyTint;
	}

	BackgroundBorder->SetBrushColor(HUDColor);

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("[ParcelHUDWidget] HUD color updated"));
	}
}

FLinearColor UParcelHUDWidget::GetDurabilityColor(float Durability, float MaxDurability) const
{
	const float SafeMax = MaxDurability > 0.0f ? MaxDurability : 100.0f;
	float DurabilityPercent = Durability / SafeMax;

	if (DurabilityPercent <= 0.25f)
	{
		return HUDStyle.CriticalColor;
	}
	else if (DurabilityPercent <= 0.5f)
	{
		return HUDStyle.WarningColor;
	}
	else
	{
		return HUDStyle.HealthyColor;
	}
}

FLinearColor UParcelHUDWidget::GetInstabilityColor(float Instability, float MaxInstability) const
{
	float InstabilityPercent = Instability / MaxInstability;

	if (InstabilityPercent >= 0.75f)
	{
		return HUDStyle.UnstableColor;
	}
	else if (InstabilityPercent >= 0.5f)
	{
		return HUDStyle.WarningColor;
	}
	else
	{
		return HUDStyle.StableColor;
	}
}

FString UParcelHUDWidget::GetTagLastPart(const FGameplayTag& Tag) const
{
	if (!Tag.IsValid())
	{
		return FString();
	}

	// 태그의 마지막 부분 추출 (예: Pickpacker.Parcel.Classification.Standard -> Standard)
	FString TagString = Tag.ToString();
	
	// 점(.) 또는 하이픈(-)으로 분리하여 마지막 부분 추출
	int32 LastDotIndex = TagString.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	int32 LastHyphenIndex = TagString.Find(TEXT("-"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	
	int32 LastSeparatorIndex = FMath::Max(LastDotIndex, LastHyphenIndex);
	if (LastSeparatorIndex != INDEX_NONE && LastSeparatorIndex < TagString.Len() - 1)
	{
		return TagString.Mid(LastSeparatorIndex + 1);
	}
	
	// 구분자가 없으면 전체 태그 이름 사용
	return TagString;
}

UTexture2D* UParcelHUDWidget::GetClassificationIcon(const FGameplayTag& ClassificationTag) const
{
	if (!ClassificationTag.IsValid())
	{
		return StandardIcon;
	}

	FString LastPart = GetTagLastPart(ClassificationTag);

	// 마지막 부분으로 아이콘 결정
	if (LastPart.Equals(TEXT("Fragile"), ESearchCase::IgnoreCase))
	{
		return FragileIcon;
	}
	else if (LastPart.Equals(TEXT("Contraband"), ESearchCase::IgnoreCase))
	{
		return ContrabandIcon;
	}

	return StandardIcon;
}

FText UParcelHUDWidget::GetClassificationText(const FGameplayTag& ClassificationTag) const
{
	if (!ClassificationTag.IsValid())
	{
		return NSLOCTEXT("ParcelHUD", "ClassificationFallback", "일반");
	}

	FText DisplayText;
	if (FParcelTagDisplayUtils::TryGetTagDisplayText(ClassificationDisplayTable, ClassificationTag, DisplayText))
	{
		return DisplayText;
	}

	const FString LastPart = GetTagLastPart(ClassificationTag);
	return FText::FromString(LastPart);
}

FText UParcelHUDWidget::GetParcelTagText(const FGameplayTag& ParcelTag) const
{
	if (!ParcelTag.IsValid())
	{
		return FText::GetEmpty();
	}

	FText DisplayText;
	if (FParcelTagDisplayUtils::TryGetTagDisplayText(ParcelTagDisplayTable, ParcelTag, DisplayText))
	{
		return DisplayText;
	}

	const FString LastPart = GetTagLastPart(ParcelTag);
	return FText::FromString(LastPart);
}


