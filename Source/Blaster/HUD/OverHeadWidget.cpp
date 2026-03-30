// Fill out your copyright notice in the Description page of Project Settings.
#include "OverHeadWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "Slate/SlateBrushAsset.h"
#include "Components/WidgetComponent.h"

void UOverHeadWidget::SetDisplayText(FString TextToDisplay)
{
	if (DisplayText)
	{
		DisplayText->SetText(FText::FromString(TextToDisplay));
	}
}

void UOverHeadWidget::ShowPlayerNetRole(APawn* InPawn)
{
	// nullptr 체크 추가
	if (!InPawn)
	{
		SetDisplayText(TEXT("No Pawn"));
		return;
	}

	ENetRole RemoteRole = InPawn->GetLocalRole();
	FString Role;
	switch (RemoteRole)
	{
		case ENetRole::ROLE_Authority:
			Role = TEXT("Authority");
			break;
		case ENetRole::ROLE_AutonomousProxy:
			Role = TEXT("Autonomous Proxy");
			break;
		case ENetRole::ROLE_SimulatedProxy:
			Role = TEXT("Simulated Proxy");
			break;
		case ENetRole::ROLE_None:
			Role = TEXT("None");
			break;
	}
	FString RemoteRoleString = FString::Printf(TEXT("Remote Role: %s"), *Role);
	SetDisplayText(RemoteRoleString);
}

void UOverHeadWidget::SetPlayerName(const FString& PlayerName)
{
	const bool bHideForLocal = [this]()
	{
		if (const UWidgetComponent* WidgetComp = Cast<UWidgetComponent>(GetOuter()))
		{
			if (const APawn* OwnerPawn = Cast<APawn>(WidgetComp->GetOwner()))
			{
				return OwnerPawn->IsLocallyControlled();
			}
		}
		return false;
	}();

	if (bHideForLocal)
	{
		if (PlayerNameText)
		{
			PlayerNameText->SetVisibility(ESlateVisibility::Collapsed);
			PlayerNameText->SetText(FText());
		}
		if (DisplayText)
		{
			DisplayText->SetVisibility(ESlateVisibility::Collapsed);
			DisplayText->SetText(FText());
		}
		return;
	}

	if (PlayerNameText)
	{
		PlayerNameText->SetVisibility(ESlateVisibility::Visible);
		PlayerNameText->SetText(FText::FromString(PlayerName));
	}
	else if (DisplayText)
	{
		DisplayText->SetVisibility(ESlateVisibility::Visible);
		// PlayerNameText가 없으면 DisplayText에 표시
		DisplayText->SetText(FText::FromString(PlayerName));
	}
}

void UOverHeadWidget::SetReadyStatus(bool bReady)
{
	if (ReadyStatusText)
	{
		FString Status = bReady ? TEXT("✓ Ready") : TEXT("Not Ready");
		FLinearColor Color = bReady ? FLinearColor(0.0f, 1.0f, 0.0f, 1.0f) : FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
		ReadyStatusText->SetText(FText::FromString(Status));
		ReadyStatusText->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UOverHeadWidget::SetPlayerInfo(const FString& PlayerName, bool bReady)
{
	SetPlayerName(PlayerName);
	SetReadyStatus(bReady);
}

void UOverHeadWidget::NativeDestruct()
{
	RemoveFromParent(); // �θ𿡰Լ� ����� ó��
	Super::NativeDestruct(); // �θ� Ŭ������ NativeDestruct
}