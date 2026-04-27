#include "UI/TrainDestinationSelectionWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerState.h"
#include "PlayerController/BlasterPlayerController.h"

UTrainDestinationSelectionWidget::UTrainDestinationSelectionWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VisiblePanelCount = 4;
}

void UTrainDestinationSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildWidgetTreeIfNeeded();
	RefreshPanelData();
	RefreshPresentation();
}

void UTrainDestinationSelectionWidget::ApplySelectionContext(const FTrainSelectionContext& InSelectionContext)
{
	SelectionContext = InSelectionContext;
	RefreshPanelData();
	RefreshPresentation();
}

void UTrainDestinationSelectionWidget::ApplyVoteStates(const TArray<FTrainDestinationVoteState>& InVoteStates)
{
	VoteStates = InVoteStates;
	RefreshPanelData();
	RefreshPresentation();
}

void UTrainDestinationSelectionWidget::ApplyRouteSelectionResult(const FRouteSelectionResult& InRouteSelectionResult)
{
	RouteSelectionResult = InRouteSelectionResult;
	RefreshPanelData();
	RefreshPresentation();
}

void UTrainDestinationSelectionWidget::SubmitDestinationVote(FName DestinationId)
{
	if (DestinationId.IsNone())
	{
		return;
	}

	if (ABlasterPlayerController* PlayerController = GetOwningPlayer<ABlasterPlayerController>())
	{
		PlayerController->ServerSelectTrainDestination(DestinationId);
	}
}

void UTrainDestinationSelectionWidget::HandlePanel0Clicked()
{
	HandlePanelClicked(0);
}

void UTrainDestinationSelectionWidget::HandlePanel1Clicked()
{
	HandlePanelClicked(1);
}

void UTrainDestinationSelectionWidget::HandlePanel2Clicked()
{
	HandlePanelClicked(2);
}

void UTrainDestinationSelectionWidget::HandlePanel3Clicked()
{
	HandlePanelClicked(3);
}

void UTrainDestinationSelectionWidget::BuildWidgetTreeIfNeeded()
{
	if (!WidgetTree || RootBox)
	{
		return;
	}

	RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TrainSelectionRoot"));
	WidgetTree->RootWidget = RootBox;

	TitleTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleTextBlock->SetText(FText::FromString(TEXT("Train Route Selection")));
	if (UVerticalBoxSlot* TitleSlot = RootBox->AddChildToVerticalBox(TitleTextBlock))
	{
		TitleSlot->SetPadding(FMargin(16.0f, 16.0f, 16.0f, 6.0f));
	}

	StatusTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusTextBlock->SetAutoWrapText(true);
	if (UVerticalBoxSlot* StatusSlot = RootBox->AddChildToVerticalBox(StatusTextBlock))
	{
		StatusSlot->SetPadding(FMargin(16.0f, 0.0f, 16.0f, 6.0f));
	}

	MissionTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MissionText"));
	MissionTextBlock->SetAutoWrapText(true);
	if (UVerticalBoxSlot* MissionSlot = RootBox->AddChildToVerticalBox(MissionTextBlock))
	{
		MissionSlot->SetPadding(FMargin(16.0f, 0.0f, 16.0f, 12.0f));
	}

	PanelGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("PanelGrid"));
	if (UVerticalBoxSlot* GridSlot = RootBox->AddChildToVerticalBox(PanelGrid))
	{
		GridSlot->SetPadding(FMargin(12.0f, 0.0f, 12.0f, 16.0f));
	}

	PanelBorders.Reserve(VisiblePanelCount);
	PanelButtons.Reserve(VisiblePanelCount);
	PanelTitleTextBlocks.Reserve(VisiblePanelCount);
	PanelDifficultyTextBlocks.Reserve(VisiblePanelCount);
	PanelTagTextBlocks.Reserve(VisiblePanelCount);
	PanelOrderTextBlocks.Reserve(VisiblePanelCount);
	PanelVoteTextBlocks.Reserve(VisiblePanelCount);

	for (int32 PanelIndex = 0; PanelIndex < VisiblePanelCount; ++PanelIndex)
	{
		UBorder* PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("PanelBorder_%d"), PanelIndex));
		PanelBorder->SetPadding(FMargin(8.0f));
		PanelBorder->SetBrushColor(FLinearColor(0.08f, 0.09f, 0.11f, 0.92f));

		UButton* PanelButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("PanelButton_%d"), PanelIndex));
		PanelButton->SetClickMethod(EButtonClickMethod::MouseDown);
		PanelBorder->SetContent(PanelButton);

		UVerticalBox* PanelContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("PanelContent_%d"), PanelIndex));
		PanelButton->AddChild(PanelContent);

		UTextBlock* PanelTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("PanelTitle_%d"), PanelIndex));
		PanelTitle->SetAutoWrapText(true);
		PanelContent->AddChildToVerticalBox(PanelTitle);

		UTextBlock* PanelDifficulty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("PanelDifficulty_%d"), PanelIndex));
		PanelDifficulty->SetAutoWrapText(true);
		if (UVerticalBoxSlot* DifficultySlot = PanelContent->AddChildToVerticalBox(PanelDifficulty))
		{
			DifficultySlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
		}

		UTextBlock* PanelTags = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("PanelTags_%d"), PanelIndex));
		PanelTags->SetAutoWrapText(true);
		if (UVerticalBoxSlot* TagSlot = PanelContent->AddChildToVerticalBox(PanelTags))
		{
			TagSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
		}

		UTextBlock* PanelOrders = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("PanelOrders_%d"), PanelIndex));
		PanelOrders->SetAutoWrapText(true);
		if (UVerticalBoxSlot* OrderSlot = PanelContent->AddChildToVerticalBox(PanelOrders))
		{
			OrderSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		}

		UTextBlock* PanelVotes = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("PanelVotes_%d"), PanelIndex));
		PanelVotes->SetAutoWrapText(true);
		if (UVerticalBoxSlot* VoteSlot = PanelContent->AddChildToVerticalBox(PanelVotes))
		{
			VoteSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		}

		if (UUniformGridSlot* GridChildSlot = PanelGrid->AddChildToUniformGrid(PanelBorder, PanelIndex / 2, PanelIndex % 2))
		{
			GridChildSlot->SetHorizontalAlignment(HAlign_Fill);
			GridChildSlot->SetVerticalAlignment(VAlign_Fill);
		}

		switch (PanelIndex)
		{
		case 0:
			PanelButton->OnClicked.AddDynamic(this, &UTrainDestinationSelectionWidget::HandlePanel0Clicked);
			break;
		case 1:
			PanelButton->OnClicked.AddDynamic(this, &UTrainDestinationSelectionWidget::HandlePanel1Clicked);
			break;
		case 2:
			PanelButton->OnClicked.AddDynamic(this, &UTrainDestinationSelectionWidget::HandlePanel2Clicked);
			break;
		case 3:
			PanelButton->OnClicked.AddDynamic(this, &UTrainDestinationSelectionWidget::HandlePanel3Clicked);
			break;
		default:
			break;
		}

		PanelBorders.Add(PanelBorder);
		PanelButtons.Add(PanelButton);
		PanelTitleTextBlocks.Add(PanelTitle);
		PanelDifficultyTextBlocks.Add(PanelDifficulty);
		PanelTagTextBlocks.Add(PanelTags);
		PanelOrderTextBlocks.Add(PanelOrders);
		PanelVoteTextBlocks.Add(PanelVotes);
	}
}

void UTrainDestinationSelectionWidget::HandlePanelClicked(int32 PanelIndex)
{
	if (!PanelData.IsValidIndex(PanelIndex))
	{
		return;
	}

	const FTrainDestinationPanelData& DestinationPanel = PanelData[PanelIndex];
	if (!DestinationPanel.bSelectable || DestinationPanel.DestinationId.IsNone())
	{
		return;
	}

	SubmitDestinationVote(DestinationPanel.DestinationId);
}

void UTrainDestinationSelectionWidget::RefreshPanelData()
{
	PanelData.Reset();

	const int32 MaxPanels = FMath::Max(0, VisiblePanelCount);
	const TArray<FTrainDestination>& Destinations = SelectionContext.AvailableDestinations;

	for (int32 Index = 0; Index < MaxPanels; ++Index)
	{
		FTrainDestinationPanelData DestinationPanel;
		DestinationPanel.PanelIndex = Index;

		if (Destinations.IsValidIndex(Index))
		{
			const FTrainDestination& Destination = Destinations[Index];
			DestinationPanel.DestinationId = Destination.DestinationId;
			DestinationPanel.DisplayName = Destination.DisplayName.IsEmpty()
				? FText::FromName(Destination.DestinationId)
				: Destination.DisplayName;
			DestinationPanel.DifficultyText = GetDifficultyText(Destination.Difficulty);
			DestinationPanel.TagSummaryText = BuildTagSummaryText(Destination);
			DestinationPanel.OrderSummaryText = BuildOrderSummaryText(Destination);

			const bool bSelectedByLocalPlayer = VoteStates.ContainsByPredicate(
				[this, &Destination](const FTrainDestinationVoteState& VoteState)
				{
					const APlayerState* LocalPlayerState = GetOwningPlayer() ? GetOwningPlayer()->PlayerState : nullptr;
					return LocalPlayerState
						&& VoteState.PlayerId == LocalPlayerState->GetPlayerId()
						&& VoteState.DestinationId == Destination.DestinationId;
				});

			DestinationPanel.bSelectedByLocalPlayer = bSelectedByLocalPlayer;
			DestinationPanel.bLocked = !SelectionContext.LockedDestinationId.IsNone()
				&& SelectionContext.LockedDestinationId == Destination.DestinationId;
			DestinationPanel.bSelectable = SelectionContext.bSelectionOpen && !DestinationPanel.bLocked;
			DestinationPanel.VoteSummaryText = BuildVoteSummaryText(Destination, bSelectedByLocalPlayer);
		}
		else
		{
			DestinationPanel.DisplayName = FText::FromString(TEXT("Unavailable"));
			DestinationPanel.DifficultyText = FText::FromString(TEXT("No destination registered"));
			DestinationPanel.TagSummaryText = FText::GetEmpty();
			DestinationPanel.OrderSummaryText = FText::FromString(TEXT("Add more destinations to the route registry."));
			DestinationPanel.VoteSummaryText = FText::GetEmpty();
		}

		PanelData.Add(DestinationPanel);
	}
}

void UTrainDestinationSelectionWidget::RefreshPresentation()
{
	BuildWidgetTreeIfNeeded();

	if (TitleTextBlock)
	{
		TitleTextBlock->SetText(FText::FromString(TEXT("Train Route Selection")));
	}

	if (StatusTextBlock)
	{
		FText StatusText = SelectionContext.bSelectionOpen
			? FText::Format(
				FText::FromString(TEXT("Select 1 of {0} routes. Vote policy: {1}.")),
				FText::AsNumber(SelectionContext.AvailableDestinations.Num()),
				SelectionContext.VotePolicy == ETrainVoteResolutionPolicy::HostOnly
					? FText::FromString(TEXT("Host picks"))
					: FText::FromString(TEXT("Majority, then host")))
			: (!SelectionContext.LockedDestinationId.IsNone()
				? FText::Format(
					FText::FromString(TEXT("Route locked: {0}")),
					FText::FromName(SelectionContext.LockedDestinationId))
				: FText::FromString(TEXT("Waiting for route selection to open.")));
		StatusTextBlock->SetText(StatusText);
	}

	if (MissionTextBlock)
	{
		MissionTextBlock->SetText(BuildMissionSummaryText());
	}

	for (int32 PanelIndex = 0; PanelIndex < PanelData.Num(); ++PanelIndex)
	{
		const FTrainDestinationPanelData& DestinationPanel = PanelData[PanelIndex];

		if (PanelBorders.IsValidIndex(PanelIndex))
		{
			FLinearColor BorderColor = DestinationPanel.bLocked
				? FLinearColor(0.13f, 0.32f, 0.16f, 0.95f)
				: (DestinationPanel.bSelectedByLocalPlayer
					? FLinearColor(0.11f, 0.20f, 0.38f, 0.95f)
					: FLinearColor(0.08f, 0.09f, 0.11f, 0.92f));
			PanelBorders[PanelIndex]->SetBrushColor(BorderColor);
			PanelBorders[PanelIndex]->SetVisibility(ESlateVisibility::Visible);
		}

		if (PanelButtons.IsValidIndex(PanelIndex))
		{
			PanelButtons[PanelIndex]->SetIsEnabled(DestinationPanel.bSelectable);
		}

		if (PanelTitleTextBlocks.IsValidIndex(PanelIndex))
		{
			PanelTitleTextBlocks[PanelIndex]->SetText(DestinationPanel.DisplayName);
		}

		if (PanelDifficultyTextBlocks.IsValidIndex(PanelIndex))
		{
			PanelDifficultyTextBlocks[PanelIndex]->SetText(DestinationPanel.DifficultyText);
		}

		if (PanelTagTextBlocks.IsValidIndex(PanelIndex))
		{
			PanelTagTextBlocks[PanelIndex]->SetText(DestinationPanel.TagSummaryText);
		}

		if (PanelOrderTextBlocks.IsValidIndex(PanelIndex))
		{
			PanelOrderTextBlocks[PanelIndex]->SetText(DestinationPanel.OrderSummaryText);
		}

		if (PanelVoteTextBlocks.IsValidIndex(PanelIndex))
		{
			PanelVoteTextBlocks[PanelIndex]->SetText(DestinationPanel.VoteSummaryText);
		}
	}
}

bool UTrainDestinationSelectionWidget::DoesDestinationMatchOrder(const FTrainDestination& Destination, const FActiveOrderState& OrderState) const
{
	if (OrderState.bCompleted || OrderState.bFailed)
	{
		return false;
	}

	if (OrderState.RequiredItemTag.IsValid() && Destination.AvailableItemTags.HasTag(OrderState.RequiredItemTag))
	{
		return true;
	}

	if (OrderState.RequiredParcelTag.IsValid() && Destination.AvailableItemTags.HasTag(OrderState.RequiredParcelTag))
	{
		return true;
	}

	return false;
}

int32 UTrainDestinationSelectionWidget::GetVoteCountForDestination(FName DestinationId) const
{
	int32 VoteCount = 0;
	for (const FTrainDestinationVoteState& VoteState : VoteStates)
	{
		if (VoteState.DestinationId == DestinationId && VoteState.bLockedIn)
		{
			++VoteCount;
		}
	}

	return VoteCount;
}

FText UTrainDestinationSelectionWidget::BuildMissionSummaryText() const
{
	TArray<FString> SummaryParts;

	if (!SelectionContext.MissionDefinition.MissionId.IsNone())
	{
		SummaryParts.Add(FString::Printf(TEXT("Mission: %s"), *SelectionContext.MissionDefinition.MissionId.ToString()));
	}

	for (const FMissionItemTarget& ItemTarget : SelectionContext.MissionDefinition.ItemTargets)
	{
		SummaryParts.Add(FString::Printf(TEXT("%s x%d"), *GetTagDisplayText(ItemTarget.ItemTag).ToString(), ItemTarget.Quantity));
	}

	if (SummaryParts.Num() == 0)
	{
		return FText::FromString(TEXT("Mission: no explicit target. Active orders drive the route choice."));
	}

	return FText::FromString(FString::Join(SummaryParts, TEXT(" | ")));
}

FText UTrainDestinationSelectionWidget::BuildTagSummaryText(const FTrainDestination& Destination) const
{
	TArray<FString> TagParts;
	for (const FGameplayTag& ItemTag : Destination.AvailableItemTags)
	{
		TagParts.Add(GetTagDisplayText(ItemTag).ToString());
	}

	const FString TagSummary = TagParts.Num() > 0
		? FString::Join(TagParts, TEXT(", "))
		: FString(TEXT("No item tags"));
	return FText::Format(FText::FromString(TEXT("Tags: {0}")), FText::FromString(TagSummary));
}

FText UTrainDestinationSelectionWidget::BuildOrderSummaryText(const FTrainDestination& Destination) const
{
	TArray<FString> MatchingOrders;

	for (const FActiveOrderState& OrderState : SelectionContext.ActiveOrders)
	{
		if (!DoesDestinationMatchOrder(Destination, OrderState))
		{
			continue;
		}

		const FString OrderLabel = !OrderState.OrderDescription.IsEmpty()
			? OrderState.OrderDescription.ToString()
			: OrderState.OrderName.ToString();
		MatchingOrders.Add(FString::Printf(
			TEXT("%s (%d/%d)"),
			*OrderLabel,
			OrderState.SubmittedQuantity,
			OrderState.RequiredQuantity));
	}

	if (MatchingOrders.Num() == 0)
	{
		return FText::FromString(TEXT("Orders: no direct order match"));
	}

	return FText::FromString(FString::Printf(TEXT("Orders: %s"), *FString::Join(MatchingOrders, TEXT(" | "))));
}

FText UTrainDestinationSelectionWidget::BuildVoteSummaryText(const FTrainDestination& Destination, bool bSelectedByLocalPlayer) const
{
	const int32 VoteCount = GetVoteCountForDestination(Destination.DestinationId);
	const FString PlayerStateText = bSelectedByLocalPlayer ? TEXT(" You voted here.") : TEXT("");
	const FString LockedText = (!SelectionContext.LockedDestinationId.IsNone() && SelectionContext.LockedDestinationId == Destination.DestinationId)
		? TEXT(" Locked.")
		: TEXT("");

	return FText::FromString(FString::Printf(
		TEXT("Votes: %d/%d.%s%s"),
		VoteCount,
		SelectionContext.EligibleVoterCount,
		*PlayerStateText,
		*LockedText));
}

FText UTrainDestinationSelectionWidget::GetDifficultyText(EZoneDifficulty Difficulty) const
{
	switch (Difficulty)
	{
	case EZoneDifficulty::Low:
		return FText::FromString(TEXT("Difficulty: Low"));
	case EZoneDifficulty::Medium:
		return FText::FromString(TEXT("Difficulty: Medium"));
	case EZoneDifficulty::MediumHigh:
		return FText::FromString(TEXT("Difficulty: Medium-High"));
	case EZoneDifficulty::High:
		return FText::FromString(TEXT("Difficulty: High"));
	case EZoneDifficulty::Extreme:
		return FText::FromString(TEXT("Difficulty: Extreme"));
	default:
		return FText::FromString(TEXT("Difficulty: Unknown"));
	}
}

FText UTrainDestinationSelectionWidget::GetTagDisplayText(const FGameplayTag& Tag) const
{
	if (!Tag.IsValid())
	{
		return FText::FromString(TEXT("None"));
	}

	FString TagString = Tag.ToString();
	int32 SplitIndex = INDEX_NONE;
	if (TagString.FindLastChar(TEXT('.'), SplitIndex) && SplitIndex + 1 < TagString.Len())
	{
		TagString.RightChopInline(SplitIndex + 1);
	}

	return FText::FromString(TagString);
}
