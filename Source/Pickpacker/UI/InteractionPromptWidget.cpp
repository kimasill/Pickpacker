// Fill out your copyright notice in the Description page of Project Settings.

#include "InteractionPromptWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

UInteractionPromptWidget::UInteractionPromptWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInteractionPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Initially hidden
	SetPromptVisibility(false);
}

void UInteractionPromptWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void UInteractionPromptWidget::UpdateInteractionText(const FText& InteractText)
{
	if (InteractionText)
	{
		InteractionText->SetText(InteractText);
	}
}

void UInteractionPromptWidget::SetPromptVisibility(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UInteractionPromptWidget::SetPromptPosition(const FVector2D& Position)
{
	SetAlignmentInViewport(FVector2D(0.5f, 1.0f));
	SetPositionInViewport(Position, false);
}

void UInteractionPromptWidget::UpdateCreditInfo(bool bRequiresUnlock, int32 UnlockCost, const FText& LockedMessage, const FText& UnlockedMessage, int32 CurrentCredits)
{
	SetPromptVisibility(true);
	if (CreditCostText)
	{
		if (bRequiresUnlock && UnlockCost > 0)
		{
			const bool bCanAfford = CurrentCredits >= UnlockCost;
			const FText CreditText = FText::Format(NSLOCTEXT("InteractionPrompt", "CreditCost", "{0} CR"), UnlockCost);
			CreditCostText->SetText(CreditText);
			
			// Set color based on affordability
			if (bCanAfford)
			{
				CreditCostText->SetColorAndOpacity(FLinearColor::Green);
			}
			else
			{
				CreditCostText->SetColorAndOpacity(FLinearColor::Red);
			}
			
			CreditCostText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			CreditCostText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (bRequiresUnlock)
	{
		if (!LockedMessage.IsEmpty())
		{
			UpdateInteractionText(LockedMessage);
		}
		else if (UnlockCost > 0)
		{
			FText PromptText = NSLOCTEXT("InteractionPrompt", "LockedDefault", "요구 크레딧:");
			UpdateInteractionText(PromptText);
		}
		else
		{
			UpdateInteractionText(NSLOCTEXT("InteractionPrompt", "LockedNoCost", "Locked"));			
		}		
	}
	else if (!UnlockedMessage.IsEmpty())
	{
		UpdateInteractionText(UnlockedMessage);		
	}	
}

void UInteractionPromptWidget::ClearCreditInfo()
{
	SetPromptVisibility(true);
	if (CreditCostText)
	{
		CreditCostText->SetVisibility(ESlateVisibility::Collapsed);
	}
}


void UInteractionPromptWidget::UpdateFromInteractionData(const FInteractionUIData& Data, const FText& InputKeyText)
{
	// 프롬프트는 데이터가 오면 표시
	SetPromptVisibility(true);

	FText DisplayActionText = Data.ActionText;
	if (InputKeyText.IsEmpty() == false && DataKey == nullptr && KeyName == nullptr && InformationText == nullptr)
	{
		DisplayActionText = FText::Format(NSLOCTEXT("InteractionPrompt", "InlinePrompt", "[{0}] {1}"), InputKeyText, Data.ActionText);
	}

	UpdateInteractionText(DisplayActionText);

	if (DataKey)
	{
		DataKey->SetText(InputKeyText);
		DataKey->SetVisibility(InputKeyText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (KeyName)
	{
		KeyName->SetText(InputKeyText);
		KeyName->SetVisibility(InputKeyText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (InformationText)
	{
		InformationText->SetText(InputKeyText);
		InformationText->SetVisibility(InputKeyText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	// 크레딧/잠금 표시
	if (Data.bRequiresUnlock && Data.UnlockCost > 0)
	{
		UpdateCreditInfo(Data.bRequiresUnlock, Data.UnlockCost, Data.LockedMessage, Data.UnlockedMessage, Data.CurrentCredits);
	}
	else
	{
		ClearCreditInfo();
	}

	// 컬러 틴트 (선택)
	if (PromptBackground)
	{
		PromptBackground->SetColorAndOpacity(Data.Tint);
	}

	// 델리게이트 브로드캐스트
	OnInteractionWidgetUpdated.Broadcast(Data);
}

void UInteractionPromptWidget::ClearInteractionData()
{
	ClearCreditInfo();
	UpdateInteractionText(FText());
	if (DataKey)
	{
		DataKey->SetText(FText());
		DataKey->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (KeyName)
	{
		KeyName->SetText(FText());
		KeyName->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (InformationText)
	{
		InformationText->SetText(FText());
		InformationText->SetVisibility(ESlateVisibility::Collapsed);
	}

	SetPromptVisibility(false);
	OnInteractionWidgetCleared.Broadcast();
}
