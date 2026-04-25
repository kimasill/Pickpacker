#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NPCDialogueUIData.h"
#include "NPCDialogueChoiceWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCDialogueChoiceClicked, int32, ChoiceIndex);

UCLASS()
class PICKPACKER_API UNPCDialogueChoiceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void ApplyChoiceData(const FDialogueChoiceUIData& InChoiceData);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void FocusChoiceButton();

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool HasFocusableChoiceButton() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Dialogue")
	void BP_OnChoiceDataApplied(const FDialogueChoiceUIData& InChoiceData);

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	const FDialogueChoiceUIData& GetChoiceData() const { return ChoiceData; }

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnNPCDialogueChoiceClicked OnChoiceClicked;

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ChoiceButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ChoiceTextBlock;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FDialogueChoiceUIData ChoiceData;

private:
	UFUNCTION()
	void HandleChoiceButtonClicked();

	void BuildFallbackWidgetTree();
};
