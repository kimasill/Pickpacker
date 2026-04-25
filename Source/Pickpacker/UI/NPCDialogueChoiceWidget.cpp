#include "UI/NPCDialogueChoiceWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"

void UNPCDialogueChoiceWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		BuildFallbackWidgetTree();
	}

	if (ChoiceButton)
	{
		ChoiceButton->SetClickMethod(EButtonClickMethod::MouseDown);
		ChoiceButton->OnClicked.RemoveDynamic(this, &UNPCDialogueChoiceWidget::HandleChoiceButtonClicked);
		ChoiceButton->OnClicked.AddDynamic(this, &UNPCDialogueChoiceWidget::HandleChoiceButtonClicked);
	}
}

FReply UNPCDialogueChoiceWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && ChoiceData.ChoiceIndex != INDEX_NONE)
	{
		FocusChoiceButton();
		OnChoiceClicked.Broadcast(ChoiceData.ChoiceIndex);
		return FReply::Handled();
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

void UNPCDialogueChoiceWidget::ApplyChoiceData(const FDialogueChoiceUIData& InChoiceData)
{
	ChoiceData = InChoiceData;

	if (ChoiceTextBlock)
	{
		ChoiceTextBlock->SetText(ChoiceData.ChoiceText);
	}

	BP_OnChoiceDataApplied(ChoiceData);
}

void UNPCDialogueChoiceWidget::FocusChoiceButton()
{
	if (!ChoiceButton)
	{
		return;
	}

	if (APlayerController* OwningPlayer = GetOwningPlayer())
	{
		ChoiceButton->SetUserFocus(OwningPlayer);
	}

	ChoiceButton->SetKeyboardFocus();
}

bool UNPCDialogueChoiceWidget::HasFocusableChoiceButton() const
{
	return ChoiceButton != nullptr;
}

void UNPCDialogueChoiceWidget::HandleChoiceButtonClicked()
{
	OnChoiceClicked.Broadcast(ChoiceData.ChoiceIndex);
}

void UNPCDialogueChoiceWidget::BuildFallbackWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	ChoiceButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ChoiceButton"));
	ChoiceTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ChoiceText"));

	if (ChoiceButton)
	{
		ChoiceButton->SetBackgroundColor(FLinearColor(0.14f, 0.17f, 0.22f, 1.0f));
		ChoiceButton->SetColorAndOpacity(FLinearColor::White);
		ChoiceButton->SetClickMethod(EButtonClickMethod::MouseDown);
	}

	if (ChoiceTextBlock)
	{
		ChoiceTextBlock->SetAutoWrapText(true);
		ChoiceTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		ChoiceTextBlock->SetText(ChoiceData.ChoiceText);
	}

	if (ChoiceButton && ChoiceTextBlock)
	{
		ChoiceButton->AddChild(ChoiceTextBlock);
		WidgetTree->RootWidget = ChoiceButton;
	}
}
