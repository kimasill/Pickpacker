#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PickpackerTypes/CoreLoopTypes.h"
#include "TrainDestinationSelectionWidget.generated.h"

class UButton;
class UBorder;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;

USTRUCT(BlueprintType)
struct PICKPACKER_API FTrainDestinationPanelData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Train")
	int32 PanelIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Train")
	FName DestinationId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Train")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Train")
	FText DifficultyText;

	UPROPERTY(BlueprintReadOnly, Category = "Train")
	FText TagSummaryText;

	UPROPERTY(BlueprintReadOnly, Category = "Train")
	FText OrderSummaryText;

	UPROPERTY(BlueprintReadOnly, Category = "Train")
	FText VoteSummaryText;

	UPROPERTY(BlueprintReadOnly, Category = "Train")
	bool bSelectable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Train")
	bool bSelectedByLocalPlayer = false;

	UPROPERTY(BlueprintReadOnly, Category = "Train")
	bool bLocked = false;
};

UCLASS()
class PICKPACKER_API UTrainDestinationSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTrainDestinationSelectionWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "Train")
	void ApplySelectionContext(const FTrainSelectionContext& InSelectionContext);

	UFUNCTION(BlueprintCallable, Category = "Train")
	void ApplyVoteStates(const TArray<FTrainDestinationVoteState>& InVoteStates);

	UFUNCTION(BlueprintCallable, Category = "Train")
	void ApplyRouteSelectionResult(const FRouteSelectionResult& InRouteSelectionResult);

	UFUNCTION(BlueprintCallable, Category = "Train")
	void SubmitDestinationVote(FName DestinationId);

	UFUNCTION(BlueprintPure, Category = "Train")
	const TArray<FTrainDestinationPanelData>& GetPanelData() const { return PanelData; }

	UFUNCTION(BlueprintPure, Category = "Train")
	const FTrainSelectionContext& GetSelectionContext() const { return SelectionContext; }

protected:
	UFUNCTION()
	void HandlePanel0Clicked();

	UFUNCTION()
	void HandlePanel1Clicked();

	UFUNCTION()
	void HandlePanel2Clicked();

	UFUNCTION()
	void HandlePanel3Clicked();

private:
	void BuildWidgetTreeIfNeeded();
	void HandlePanelClicked(int32 PanelIndex);
	void RefreshPanelData();
	void RefreshPresentation();
	bool DoesDestinationMatchOrder(const FTrainDestination& Destination, const FActiveOrderState& OrderState) const;
	int32 GetVoteCountForDestination(FName DestinationId) const;
	FText BuildMissionSummaryText() const;
	FText BuildTagSummaryText(const FTrainDestination& Destination) const;
	FText BuildOrderSummaryText(const FTrainDestination& Destination) const;
	FText BuildVoteSummaryText(const FTrainDestination& Destination, bool bSelectedByLocalPlayer) const;
	FText GetDifficultyText(EZoneDifficulty Difficulty) const;
	FText GetTagDisplayText(const FGameplayTag& Tag) const;

	UPROPERTY()
	FTrainSelectionContext SelectionContext;

	UPROPERTY()
	TArray<FTrainDestinationVoteState> VoteStates;

	UPROPERTY()
	FRouteSelectionResult RouteSelectionResult;

	UPROPERTY()
	TArray<FTrainDestinationPanelData> PanelData;

	UPROPERTY()
	TObjectPtr<UVerticalBox> RootBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleTextBlock;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusTextBlock;

	UPROPERTY()
	TObjectPtr<UTextBlock> MissionTextBlock;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> PanelGrid;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> PanelBorders;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> PanelButtons;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> PanelTitleTextBlocks;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> PanelDifficultyTextBlocks;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> PanelTagTextBlocks;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> PanelOrderTextBlocks;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> PanelVoteTextBlocks;

	UPROPERTY(EditAnywhere, Category = "Train|UI")
	int32 VisiblePanelCount = 4;
};
