// Fill out your copyright notice in the Description page of Project Settings.

#include "EscapeProgressWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Character/BlasterCharacter.h"
#include "Components/PlayerInventoryComponent.h"

UEscapeProgressWidget::UEscapeProgressWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UEscapeProgressWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (OverallProgressBar)
	{
		OverallProgressBar->SetPercent(0.0f);
	}

	if (ProgressText)
	{
		ProgressText->SetText(FText::FromString(TEXT("Escape Progress: 0%")));
	}
}

void UEscapeProgressWidget::UpdateEscapeProgress(float Progress)
{
	if (OverallProgressBar)
	{
		OverallProgressBar->SetPercent(Progress);
	}

	if (ProgressText)
	{
		int32 Percentage = FMath::RoundToInt(Progress * 100.0f);
		ProgressText->SetText(FText::FromString(FString::Printf(TEXT("Escape Progress: %d%%"), Percentage)));
	}
}

void UEscapeProgressWidget::UpdateEscapeRequirements(const TArray<FEscapeRequirement>& Requirements, ACharacter* Player)
{
	if (!RequirementsScrollBox || !Player)
	{
		return;
	}

	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Player);
	if (!BlasterCharacter)
	{
		return;
	}

	UPlayerInventoryComponent* InventoryComponent = BlasterCharacter->GetPlayerInventoryComponent();
	if (!InventoryComponent)
	{
		return;
	}

	// Clear existing requirements
	RequirementsScrollBox->ClearChildren();

	// Add requirements
	for (const FEscapeRequirement& Requirement : Requirements)
	{
		TArray<AParcelActor*> Items = InventoryComponent->GetItemsByType(Requirement.RequiredItemType);
		int32 CurrentCount = Items.Num();
		bool bCompleted = CurrentCount >= Requirement.RequiredCount;

		FString RequirementText = FString::Printf(
			TEXT("%s: %d/%d %s"),
			*UEnum::GetValueAsString(Requirement.RequiredItemType),
			CurrentCount,
			Requirement.RequiredCount,
			bCompleted ? TEXT("✓") : TEXT("")
		);

		UTextBlock* RequirementTextBlock = NewObject<UTextBlock>(this);
		if (RequirementTextBlock)
		{
			RequirementTextBlock->SetText(FText::FromString(RequirementText));
			RequirementsScrollBox->AddChild(RequirementTextBlock);
		}
	}
}

void UEscapeProgressWidget::SetWidgetVisibility(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

