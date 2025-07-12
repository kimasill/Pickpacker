// Fill out your copyright notice in the Description page of Project Settings.
#include "OverHeadWidget.h"
#include "Components/TextBlock.h"

void UOverHeadWidget::SetDisplayText(FString TextToDisplay)
{
	if (DisplayText)
	{
		DisplayText->SetText(FText::FromString(TextToDisplay));
	}
}

void UOverHeadWidget::ShowPlayerNetRole(APawn* InPawn)
{
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

void UOverHeadWidget::NativeDestruct()
{
	RemoveFromParent(); // 부모에게서 떼어내는 처리
	Super::NativeDestruct(); // 부모 클래스의 NativeDestruct
}