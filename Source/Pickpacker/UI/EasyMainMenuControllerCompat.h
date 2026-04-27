#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "EasyMainMenuControllerCompat.generated.h"

UCLASS()
class PICKPACKER_API AEasyMainMenuControllerCompat : public APawn
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "EasyGameUI|Menu")
	void StartMainMenu_Event(APlayerController* PlayerControllerRef);
};
