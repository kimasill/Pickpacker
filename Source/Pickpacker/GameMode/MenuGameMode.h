#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "MenuGameMode.generated.h"

class AMenuPlayerController;

/**
 * 메인 메뉴 전용 GameMode
 * - 지정된 카메라(또는 태그/첫 카메라)로 초기 시점을 고정
 * - 플레이어 입력을 차단하고 UI 전용 입력 모드 적용
 */
UCLASS()
class PICKPACKER_API AMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMenuGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

protected:
	/** 레벨에서 직접 할당할 초기 카메라 (없으면 태그/첫 카메라로 대체) */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Menu|Camera")
	TObjectPtr<AActor> InitialCameraActor;

	/** 이 태그를 가진 카메라를 우선 사용 (InitialCameraActor가 없을 때) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Menu|Camera")
	FName MenuCameraTag = TEXT("MenuCamera");

	/** 태그도 없으면 첫 번째 CameraActor를 사용하도록 허용 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Menu|Camera")
	bool bFallbackToAnyCamera = true;

	/** 컨트롤러에 시점/입력 설정 적용 */
	void ApplyMenuSetupToController(APlayerController* PlayerController);

	/** 현재 맵에서 사용할 시점 카메라 결정 */
	AActor* ResolveInitialCamera() const;
};

