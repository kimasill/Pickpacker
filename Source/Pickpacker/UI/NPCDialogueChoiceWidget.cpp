#include "UI/NPCDialogueChoiceWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ContentWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "InputCoreTypes.h"
#include "Input/Reply.h"
#include "Styling/SlateTypes.h"

namespace
{
	const FVector2D ChoiceIndicatorIconSize(18.0f, 18.0f);

	void SanitizeRuntimeChoiceWidgetObject(UObject* Object)
	{
		if (!Object)
		{
			return;
		}

		Object->ClearFlags(RF_Transactional);
		Object->SetFlags(RF_Transient);
	}

	void SanitizeRuntimeChoiceWidgetTree(UUserWidget* UserWidget)
	{
		if (!UserWidget)
		{
			return;
		}

		SanitizeRuntimeChoiceWidgetObject(UserWidget);

		if (UWidgetTree* LocalWidgetTree = UserWidget->WidgetTree)
		{
			SanitizeRuntimeChoiceWidgetObject(LocalWidgetTree);
			LocalWidgetTree->ForEachWidget([](UWidget* Widget)
			{
				SanitizeRuntimeChoiceWidgetObject(Widget);
			});
		}
	}

	void ConfigureInvisibleButtonChrome(UButton* Button)
	{
		if (!Button)
		{
			return;
		}

		FButtonStyle ButtonStyle = Button->GetStyle();

		auto MakeNoDrawBrush = [](FSlateBrush& Brush)
		{
			Brush = FSlateBrush();
			Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
			Brush.TintColor = FSlateColor(FLinearColor::Transparent);
		};

		MakeNoDrawBrush(ButtonStyle.Normal);
		MakeNoDrawBrush(ButtonStyle.Hovered);
		MakeNoDrawBrush(ButtonStyle.Pressed);
		MakeNoDrawBrush(ButtonStyle.Disabled);

		Button->SetStyle(ButtonStyle);
		Button->SetBackgroundColor(FLinearColor::White);
		Button->SetColorAndOpacity(FLinearColor::White);
	}

	FLinearColor GetChoiceTextColor(bool bIsEnabled)
	{
		return bIsEnabled
			? FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)
			: FLinearColor(0.62f, 0.66f, 0.72f, 0.68f);
	}

	FLinearColor GetIndicatorTint(bool bIsSatisfied)
	{
		return bIsSatisfied
			? FLinearColor(1.0f, 1.0f, 1.0f, 0.92f)
			: FLinearColor(0.58f, 0.62f, 0.68f, 0.35f);
	}

	void ApplyChoiceIndicatorIconBrush(UImage* Image, UTexture2D* IconTexture)
	{
		if (!Image)
		{
			return;
		}
		Image->SetBrushFromTexture(IconTexture, false);
	}
}

void UNPCDialogueChoiceWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		BuildFallbackWidgetTree();
	}

	EnsureRequirementWidgets();

	SanitizeRuntimeChoiceWidgetTree(this);

	if (ChoiceButton)
	{
		ConfigureInvisibleButtonChrome(ChoiceButton);
		ChoiceButton->SetClickMethod(EButtonClickMethod::MouseDown);
		ChoiceButton->OnClicked.RemoveDynamic(this, &UNPCDialogueChoiceWidget::HandleChoiceButtonClicked);
		ChoiceButton->OnClicked.AddDynamic(this, &UNPCDialogueChoiceWidget::HandleChoiceButtonClicked);
	}

	UpdateChoiceVisualState();
}

void UNPCDialogueChoiceWidget::NativeDestruct()
{
	if (ChoiceButton)
	{
		ChoiceButton->OnClicked.RemoveDynamic(this, &UNPCDialogueChoiceWidget::HandleChoiceButtonClicked);
	}

	Super::NativeDestruct();
}

void UNPCDialogueChoiceWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	FocusChoiceButton();
}

FReply UNPCDialogueChoiceWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && ChoiceData.ChoiceIndex != INDEX_NONE)
	{
		if (!ChoiceData.bIsEnabled)
		{
			return FReply::Handled();
		}

		FocusChoiceButton();
		OnChoiceClicked.Broadcast(ChoiceData.ChoiceIndex);
		return FReply::Handled();
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

void UNPCDialogueChoiceWidget::ApplyChoiceData(const FDialogueChoiceUIData& InChoiceData)
{
	ChoiceData = InChoiceData;
	EnsureRequirementWidgets();

	if (ChoiceTextBlock)
	{
		ChoiceTextBlock->SetText(ChoiceData.ChoiceText);
	}

	BP_OnChoiceDataApplied(ChoiceData);
	UpdateChoiceIndicators();
	UpdateChoiceVisualState();
}

void UNPCDialogueChoiceWidget::FocusChoiceButton()
{
	if (!ChoiceButton || !ChoiceData.bIsEnabled)
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
	return ChoiceButton != nullptr && ChoiceData.bIsEnabled;
}

bool UNPCDialogueChoiceWidget::IsChoiceEnabled() const
{
	return ChoiceData.bIsEnabled;
}

void UNPCDialogueChoiceWidget::HandleChoiceButtonClicked()
{
	if (!ChoiceData.bIsEnabled)
	{
		return;
	}

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
		ConfigureInvisibleButtonChrome(ChoiceButton);
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

void UNPCDialogueChoiceWidget::EnsureRequirementWidgets()
{
	if (!WidgetTree)
	{
		return;
	}

	if (ChoiceRequirementContainer && ChoiceRequirementTextBlock && ChoiceRequirementIconImage)
	{
		return;
	}

	UHorizontalBox* IndicatorRow = Cast<UHorizontalBox>(ChoiceRequirementContainer);
	if (!ChoiceRequirementContainer)
	{
		UWidget* ExistingRoot = WidgetTree->RootWidget;
		if (!ExistingRoot)
		{
			return;
		}

		UVerticalBox* RootWrapper = Cast<UVerticalBox>(ExistingRoot);
		if (!RootWrapper)
		{
			RootWrapper = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ChoiceRootWrapper"));
			if (!RootWrapper)
			{
				return;
			}

			WidgetTree->RootWidget = RootWrapper;
			if (UVerticalBoxSlot* ExistingRootSlot = RootWrapper->AddChildToVerticalBox(ExistingRoot))
			{
				ExistingRootSlot->SetPadding(FMargin(0.0f));
			}
		}

		IndicatorRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ChoiceRequirementRow"));
		if (!IndicatorRow)
		{
			return;
		}

		if (UVerticalBoxSlot* IndicatorRowSlot = RootWrapper->AddChildToVerticalBox(IndicatorRow))
		{
			IndicatorRowSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		}

		ChoiceRequirementContainer = IndicatorRow;
	}

	IndicatorRow = Cast<UHorizontalBox>(ChoiceRequirementContainer);
	if (!ChoiceRequirementContainer)
	{
		return;
	}

	if (!ChoiceRequirementIconImage)
	{
		ChoiceRequirementIconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ChoiceRequirementIcon"));
		if (ChoiceRequirementIconImage)
		{
			ChoiceRequirementIconImage->SetDesiredSizeOverride(ChoiceIndicatorIconSize);
			if (IndicatorRow)
			{
				if (UHorizontalBoxSlot* IconSlot = IndicatorRow->AddChildToHorizontalBox(ChoiceRequirementIconImage))
				{
					IconSlot->SetVerticalAlignment(VAlign_Center);
					IconSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
				}
			}
			else if (ChoiceRequirementContainer)
			{
				ChoiceRequirementContainer->AddChild(ChoiceRequirementIconImage);
			}
		}
	}

	if (!ChoiceRequirementTextBlock)
	{
		ChoiceRequirementTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ChoiceRequirementText"));
		if (ChoiceRequirementTextBlock)
		{
			ChoiceRequirementTextBlock->SetAutoWrapText(true);
			ChoiceRequirementTextBlock->SetColorAndOpacity(FSlateColor(GetIndicatorTint(true)));
			if (IndicatorRow)
			{
				if (UHorizontalBoxSlot* TextSlot = IndicatorRow->AddChildToHorizontalBox(ChoiceRequirementTextBlock))
				{
					TextSlot->SetVerticalAlignment(VAlign_Center);
				}
			}
			else if (ChoiceRequirementContainer)
			{
				ChoiceRequirementContainer->AddChild(ChoiceRequirementTextBlock);
			}
		}
	}
}

void UNPCDialogueChoiceWidget::UpdateChoiceIndicators()
{
	EnsureRequirementWidgets();

	FText CombinedIndicatorText = FText::GetEmpty();
	const FDialogueChoiceIndicatorUIData* PrimaryIndicatorWithIcon = nullptr;
	for (const FDialogueChoiceIndicatorUIData& Indicator : ChoiceData.UnlockIndicators)
	{
		if (Indicator.IndicatorIcon && !Indicator.bIsSatisfied)
		{
			PrimaryIndicatorWithIcon = &Indicator;
			break;
		}
	}

	if (!PrimaryIndicatorWithIcon)
	{
		for (const FDialogueChoiceIndicatorUIData& Indicator : ChoiceData.UnlockIndicators)
		{
			if (Indicator.IndicatorIcon)
			{
				PrimaryIndicatorWithIcon = &Indicator;
				break;
			}
		}
	}

	for (const FDialogueChoiceIndicatorUIData& Indicator : ChoiceData.UnlockIndicators)
	{
		if (Indicator.IndicatorText.IsEmpty())
		{
			continue;
		}

		if (CombinedIndicatorText.IsEmpty())
		{
			CombinedIndicatorText = Indicator.IndicatorText;
		}
		else
		{
			CombinedIndicatorText = FText::Format(
				NSLOCTEXT("NPCDialogue", "ChoiceIndicatorJoin", "{0}  {1}"),
				CombinedIndicatorText,
				Indicator.IndicatorText);
		}
	}

	if (ChoiceRequirementTextBlock)
	{
		ChoiceRequirementTextBlock->SetText(CombinedIndicatorText);
		ChoiceRequirementTextBlock->SetVisibility(CombinedIndicatorText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		ChoiceRequirementTextBlock->SetColorAndOpacity(FSlateColor(
			GetIndicatorTint(PrimaryIndicatorWithIcon ? PrimaryIndicatorWithIcon->bIsSatisfied : ChoiceData.bIsEnabled)));
	}

	if (ChoiceRequirementIconImage)
	{
		if (PrimaryIndicatorWithIcon && PrimaryIndicatorWithIcon->IndicatorIcon)
		{
			ApplyChoiceIndicatorIconBrush(ChoiceRequirementIconImage, PrimaryIndicatorWithIcon->IndicatorIcon);
			ChoiceRequirementIconImage->SetColorAndOpacity(GetIndicatorTint(PrimaryIndicatorWithIcon->bIsSatisfied));
			ChoiceRequirementIconImage->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			ApplyChoiceIndicatorIconBrush(ChoiceRequirementIconImage, nullptr);
			ChoiceRequirementIconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (ChoiceRequirementContainer)
	{
		bool bHasAnyIndicatorIcon = false;
		for (const FDialogueChoiceIndicatorUIData& Indicator : ChoiceData.UnlockIndicators)
		{
			if (Indicator.IndicatorIcon)
			{
				bHasAnyIndicatorIcon = true;
				break;
			}
		}

		const bool bHasIndicator = !CombinedIndicatorText.IsEmpty() || bHasAnyIndicatorIcon;
		ChoiceRequirementContainer->SetVisibility(bHasIndicator ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (ChoiceTextBlock && !ChoiceRequirementTextBlock && !CombinedIndicatorText.IsEmpty())
	{
		ChoiceTextBlock->SetText(FText::Format(
			NSLOCTEXT("NPCDialogue", "ChoiceTextWithIndicator", "{0}    [{1}]"),
			ChoiceData.ChoiceText,
			CombinedIndicatorText));
	}
}

void UNPCDialogueChoiceWidget::UpdateChoiceVisualState()
{
	if (ChoiceButton)
	{
		ChoiceButton->SetIsEnabled(ChoiceData.bIsEnabled);
		ChoiceButton->SetRenderOpacity(ChoiceData.bIsEnabled ? 1.0f : 0.78f);
	}

	if (ChoiceTextBlock)
	{
		ChoiceTextBlock->SetColorAndOpacity(FSlateColor(GetChoiceTextColor(ChoiceData.bIsEnabled)));
	}
}
