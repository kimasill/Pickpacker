#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NPCDialogueUIData.h"
#include "NPCDialogueWidget.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class UNPCDialogueChoiceWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCDialogueChoiceSelected, int32, ChoiceIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNPCDialogueAdvanceRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNPCDialogueClosedRequested);

UCLASS()
class PICKPACKER_API UNPCDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void ApplyDialogueState(const FDialogueUIState& InDialogueState);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void FocusPreferredWidget();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Dialogue")
	void BP_OnDialogueInitialized();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Dialogue")
	void BP_OnDialogueConstructed();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Dialogue")
	void BP_OnDialogueDestructed();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Dialogue")
	void BP_OnDialogueStateApplied(const FDialogueUIState& InDialogueState);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Dialogue")
	void BP_OnChoicesRebuilt();

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	const FDialogueUIState& GetDialogueState() const { return DialogueState; }

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnNPCDialogueChoiceSelected OnChoiceSelected;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnNPCDialogueAdvanceRequested OnAdvanceRequested;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnNPCDialogueClosedRequested OnClosedRequested;

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SpeakerTextBlock;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DialogueTextBlock;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ChoicesPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ContinueButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ContinueButtonTextBlock;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CloseButtonTextBlock;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TSubclassOf<UNPCDialogueChoiceWidget> ChoiceWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FDialogueUIState DialogueState;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	TArray<TObjectPtr<UNPCDialogueChoiceWidget>> SpawnedChoiceWidgets;

private:
	UFUNCTION()
	void HandleChoiceSelected(int32 ChoiceIndex);

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleCloseClicked();

	void BindAuxiliaryButtons();
	void BuildFallbackWidgetTree();
	void RebuildChoices();
	void UpdateAuxiliaryButtons();
	void ReleaseChoiceWidgets();
};
