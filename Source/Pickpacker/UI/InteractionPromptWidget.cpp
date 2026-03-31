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
	// Position can be set via slot or anchor
	// This is typically handled in the widget designer
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

	UpdateInteractionText(Data.ActionText);

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
	SetPromptVisibility(false);
	OnInteractionWidgetCleared.Broadcast();
}

