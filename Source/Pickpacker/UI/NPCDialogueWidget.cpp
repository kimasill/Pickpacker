#include "UI/NPCDialogueWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Input/Reply.h"
#include "PickpackerAssetPaths.h"
#include "UI/NPCDialogueChoiceWidget.h"

void UNPCDialogueWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		BuildFallbackWidgetTree();
	}

	if (ContinueButton)
	{
		ContinueButton->SetClickMethod(EButtonClickMethod::MouseDown);
		ContinueButton->OnClicked.RemoveDynamic(this, &UNPCDialogueWidget::HandleContinueClicked);
		ContinueButton->OnClicked.AddDynamic(this, &UNPCDialogueWidget::HandleContinueClicked);
	}

	if (CloseButton)
	{
		CloseButton->SetClickMethod(EButtonClickMethod::MouseDown);
		CloseButton->OnClicked.RemoveDynamic(this, &UNPCDialogueWidget::HandleCloseClicked);
		CloseButton->OnClicked.AddDynamic(this, &UNPCDialogueWidget::HandleCloseClicked);
	}

	UpdateAuxiliaryButtons();
	BP_OnDialogueInitialized();
}

void UNPCDialogueWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	FocusPreferredWidget();
	BP_OnDialogueConstructed();
}

void UNPCDialogueWidget::NativeDestruct()
{
	BP_OnDialogueDestructed();

	Super::NativeDestruct();
}

FReply UNPCDialogueWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FocusPreferredWidget();
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UNPCDialogueWidget::ApplyDialogueState(const FDialogueUIState& InDialogueState)
{
	DialogueState = InDialogueState;

	if (SpeakerTextBlock)
	{
		SpeakerTextBlock->SetText(DialogueState.SpeakerName);
	}

	if (DialogueTextBlock)
	{
		DialogueTextBlock->SetText(DialogueState.DialogueText);
	}

	RebuildChoices();
	UpdateAuxiliaryButtons();
	FocusPreferredWidget();
	BP_OnDialogueStateApplied(DialogueState);
}

void UNPCDialogueWidget::FocusPreferredWidget()
{
	for (UNPCDialogueChoiceWidget* ChoiceWidget : SpawnedChoiceWidgets)
	{
		if (ChoiceWidget && ChoiceWidget->HasFocusableChoiceButton())
		{
			ChoiceWidget->FocusChoiceButton();
			return;
		}
	}

	if (ContinueButton && ContinueButton->GetVisibility() == ESlateVisibility::Visible)
	{
		if (APlayerController* OwningPlayer = GetOwningPlayer())
		{
			ContinueButton->SetUserFocus(OwningPlayer);
		}

		ContinueButton->SetKeyboardFocus();
		return;
	}

	if (CloseButton && CloseButton->GetVisibility() == ESlateVisibility::Visible)
	{
		if (APlayerController* OwningPlayer = GetOwningPlayer())
		{
			CloseButton->SetUserFocus(OwningPlayer);
		}

		CloseButton->SetKeyboardFocus();
	}
}

void UNPCDialogueWidget::HandleChoiceSelected(int32 ChoiceIndex)
{
	OnChoiceSelected.Broadcast(ChoiceIndex);
}

void UNPCDialogueWidget::HandleContinueClicked()
{
	OnAdvanceRequested.Broadcast();
}

void UNPCDialogueWidget::HandleCloseClicked()
{
	OnClosedRequested.Broadcast();
}

void UNPCDialogueWidget::BuildFallbackWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DialogueCanvas"));
	USizeBox* DialogueSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DialogueSizeBox"));
	UBorder* RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialogueRoot"));
	UVerticalBox* RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogueVBox"));
	SpeakerTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpeakerText"));
	DialogueTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DialogueText"));
	ChoicesPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ChoicesPanel"));
	ContinueButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ContinueButton"));
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	ContinueButtonTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ContinueButtonText"));
	CloseButtonTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseButtonText"));

	if (SpeakerTextBlock)
	{
		SpeakerTextBlock->SetAutoWrapText(true);
		SpeakerTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.78f, 0.36f, 1.0f)));
		SpeakerTextBlock->SetText(DialogueState.SpeakerName);
	}

	if (DialogueTextBlock)
	{
		DialogueTextBlock->SetAutoWrapText(true);
		DialogueTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		DialogueTextBlock->SetText(DialogueState.DialogueText);
	}

	if (ContinueButtonTextBlock)
	{
		ContinueButton->SetBackgroundColor(FLinearColor(0.16f, 0.20f, 0.28f, 1.0f));
		ContinueButton->SetClickMethod(EButtonClickMethod::MouseDown);
		ContinueButtonTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		ContinueButtonTextBlock->SetText(NSLOCTEXT("NPCDialogue", "Continue", "Continue"));
		ContinueButton->AddChild(ContinueButtonTextBlock);
	}

	if (CloseButton && CloseButtonTextBlock)
	{
		CloseButton->SetBackgroundColor(FLinearColor(0.32f, 0.16f, 0.16f, 1.0f));
		CloseButton->SetClickMethod(EButtonClickMethod::MouseDown);
		CloseButtonTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		CloseButtonTextBlock->SetText(NSLOCTEXT("NPCDialogue", "Close", "Close"));
		CloseButton->AddChild(CloseButtonTextBlock);
	}

	if (RootCanvas && DialogueSizeBox && RootBorder && RootBox && SpeakerTextBlock && DialogueTextBlock && ChoicesPanel && ContinueButton && CloseButton)
	{
		RootBorder->SetBrushColor(FLinearColor(0.05f, 0.06f, 0.08f, 0.94f));
		RootBorder->SetPadding(FMargin(24.f, 20.f));

		if (UVerticalBoxSlot* SpeakerSlot = RootBox->AddChildToVerticalBox(SpeakerTextBlock))
		{
			SpeakerSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		}
		if (UVerticalBoxSlot* DialogueSlot = RootBox->AddChildToVerticalBox(DialogueTextBlock))
		{
			DialogueSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
		}
		if (UVerticalBoxSlot* ChoicesSlot = RootBox->AddChildToVerticalBox(ChoicesPanel))
		{
			ChoicesSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
		}
		if (UVerticalBoxSlot* ContinueSlot = RootBox->AddChildToVerticalBox(ContinueButton))
		{
			ContinueSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		}
		RootBox->AddChildToVerticalBox(CloseButton);
		RootBorder->SetContent(RootBox);
		DialogueSizeBox->SetWidthOverride(760.f);
		DialogueSizeBox->SetMaxDesiredHeight(420.f);
		DialogueSizeBox->SetContent(RootBorder);
		if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(DialogueSizeBox))
		{
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			CanvasSlot->SetPosition(FVector2D(0.f, -48.f));
		}
		WidgetTree->RootWidget = RootCanvas;
	}
}

void UNPCDialogueWidget::RebuildChoices()
{
	if (!ChoicesPanel)
	{
		return;
	}

	ChoicesPanel->ClearChildren();
	SpawnedChoiceWidgets.Reset();

	TSubclassOf<UNPCDialogueChoiceWidget> EntryWidgetClass = ChoiceWidgetClass;
	if (!EntryWidgetClass)
	{
		EntryWidgetClass = LoadClass<UNPCDialogueChoiceWidget>(nullptr, PickpackerAssetPaths::Blueprints::WidgetNPCDialogueChoiceClass);
	}
	if (!EntryWidgetClass)
	{
		EntryWidgetClass = UNPCDialogueChoiceWidget::StaticClass();
	}

	for (const FDialogueChoiceUIData& Choice : DialogueState.Choices)
	{
		UNPCDialogueChoiceWidget* ChoiceWidget = CreateWidget<UNPCDialogueChoiceWidget>(this, EntryWidgetClass);
		if (!ChoiceWidget)
		{
			continue;
		}

		ChoiceWidget->ApplyChoiceData(Choice);
		ChoiceWidget->OnChoiceClicked.AddDynamic(this, &UNPCDialogueWidget::HandleChoiceSelected);
		ChoicesPanel->AddChild(ChoiceWidget);
		SpawnedChoiceWidgets.Add(ChoiceWidget);
	}

	BP_OnChoicesRebuilt();
}

void UNPCDialogueWidget::UpdateAuxiliaryButtons()
{
	if (ContinueButton)
	{
		ContinueButton->SetVisibility(DialogueState.bCanAdvance ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (ContinueButtonTextBlock)
	{
		ContinueButtonTextBlock->SetText(DialogueState.Choices.Num() == 0
			? NSLOCTEXT("NPCDialogue", "ContinueFallback", "Continue")
			: NSLOCTEXT("NPCDialogue", "ContinueHidden", "Continue"));
	}

	if (CloseButton)
	{
		CloseButton->SetVisibility(DialogueState.bCanExit ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (CloseButtonTextBlock)
	{
		CloseButtonTextBlock->SetText(NSLOCTEXT("NPCDialogue", "CloseFallback", "Close"));
	}
}
