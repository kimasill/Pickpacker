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

