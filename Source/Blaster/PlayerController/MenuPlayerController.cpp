#include "MenuPlayerController.h"

#include "GameFramework/Pawn.h"

AMenuPlayerController::AMenuPlayerController()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	ApplyMenuInputSettings();
}

void AMenuPlayerController::ApplyMenuInputSettings()
{
	if (bShowCursor)
	{
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;
	}

	if (bUIOnlyInputMode)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetWidgetToFocus(nullptr);
		SetInputMode(InputMode);
	}
}

