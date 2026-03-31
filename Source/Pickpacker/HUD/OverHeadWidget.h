// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OverHeadWidget.generated.h"

/**
 * 플레이어 머리 위에 표시되는 위젯 (이름, 준비 상태 등)
 */
UCLASS()
class BLASTER_API UOverHeadWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* DisplayText;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* PlayerNameText;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* ReadyStatusText;

	void SetDisplayText(FString TextToDisplay);

	UFUNCTION(BlueprintCallable)
	void ShowPlayerNetRole(APawn* InPawn);

	/** 플레이어 이름 설정 */
	UFUNCTION(BlueprintCallable, Category = "OverHead")
	void SetPlayerName(const FString& PlayerName);

	/** 준비 상태 설정 */
	UFUNCTION(BlueprintCallable, Category = "OverHead")
	void SetReadyStatus(bool bReady);

	/** 플레이어 이름과 준비 상태를 한 번에 설정 */
	UFUNCTION(BlueprintCallable, Category = "OverHead")
	void SetPlayerInfo(const FString& PlayerName, bool bReady);

protected:	
	virtual void NativeDestruct() override;
};
