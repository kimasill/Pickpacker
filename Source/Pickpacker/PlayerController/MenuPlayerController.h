#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "MenuPlayerController.generated.h"

/**
 * 메인 메뉴 전용 PlayerController
 * - UI 전용 입력 모드 + 마우스 커서 표시
 * - 이동/카메라 입력 차단
 */
UCLASS()
class PICKPACKER_API AMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMenuPlayerController();

	virtual void BeginPlay() override;

	/** 메뉴 입장 시 호출하여 입력/커서/시점 세팅 */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ApplyMenuInputSettings();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Menu|Input")
	bool bShowCursor = true;

	UPROPERTY(EditDefaultsOnly, Category = "Menu|Input")
	bool bUIOnlyInputMode = true;

	UPROPERTY(EditDefaultsOnly, Category = "Menu|Input")
	bool bBlockMovementAndLook = true;
};

