#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Blaster/Interaction/InteractionUIData.h"
#include "InteractableInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class BLASTER_API IInteractableInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	void OnInteract(class ACharacter* Interactor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction", meta=(ExpandBoolAsExecs="ReturnValue"))
	bool CanInteract(class ACharacter* Interactor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction", meta=(ExpandBoolAsExecs="ReturnValue"))
	FText GetInteractText();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction|UI")
	void GetInteractionUIData(FInteractionUIData& OutData);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	void StartHighlight();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	void EndHighlight();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction|UI")
	bool RequestShowInteractionUI(class ACharacter* Interactor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction|Credits")
	void GetCreditUnlockInfo(bool& bRequiresUnlock, int32& UnlockCost, FText& LockedMessage, FText& UnlockedMessage);

	// 기본 구현
	virtual void GetInteractionUIData_Implementation(FInteractionUIData& OutData) {}
};
