#include "UI/EasyMainMenuControllerCompat.h"

#include "GameFramework/PlayerController.h"

void AEasyMainMenuControllerCompat::StartMainMenu_Event(APlayerController* PlayerControllerRef)
{
	if (PlayerControllerRef && Controller == nullptr)
	{
		Controller = PlayerControllerRef;
	}

	if (UFunction* OpenMainMenuFunction = FindFunction(TEXT("OpenMainMenu")))
	{
		ProcessEvent(OpenMainMenuFunction, nullptr);
	}
}
