// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Blaster/Components/InteractionComponent.h"
#include "Blaster/Station/PackagingStationActor.h"
#include "PackagingSwitchActor.generated.h"

/**
 * 포장 조작 패널:PackagingStationActor
 */
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API APackagingSwitchActor : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	APackagingSwitchActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/**
	 * 조작 - 포장 모드 스위치 토글
	 */
	UFUNCTION(BlueprintCallable, Category = "PackagingSwitch")
	void ToggleSwitch();

	/**
	 * PackagingStationActor 설정
	 */
	UFUNCTION(BlueprintCallable, Category = "PackagingSwitch")
	void SetPackagingStation(APackagingStationActor* Station);

	/**
	 * 연결된 PackagingStationActor 가져오기
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PackagingSwitch")
	APackagingStationActor* GetPackagingStation() const { return PackagingStation; }

	// InteractableInterface Implementation
	virtual bool OnInteract_Implementation(ACharacter* Interactor) override;
	virtual bool CanInteract_Implementation(ACharacter* Interactor) const override;
	virtual FText GetInteractText_Implementation() const override;
	virtual void StartHighlight_Implementation() override;
	virtual void EndHighlight_Implementation() override;


	UFUNCTION(BlueprintCallable, Category = "PackagingSwitch")
	void ShowInteractionWidget(bool bShow);

public:
	/** 스위치 메시 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* SwitchMesh;

	/** 연결된 포장 스테이션 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PackagingSwitch")
	APackagingStationActor* PackagingStation = nullptr;	

	/** 자동으로 레벨에서 가장 가까운 PackagingStationActor 찾기 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PackagingSwitch")
	bool bAutoFindStation = false;

	/** 자동 검색 거리 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PackagingSwitch", meta = (EditCondition = "bAutoFindStation"))
	float AutoFindDistance = 1000.0f;

	/** 디버그 설정 */
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bEnableDebugLogging = true;

public:
	/** 스위치 토글 이벤트 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSwitchToggled, EPackagingMode, NewMode);
	UPROPERTY(BlueprintAssignable, Category = "PackagingSwitch|Events")
	FOnSwitchToggled OnSwitchToggled;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UWidgetComponent* InteractionWidget;

	void FindNearestPackagingStation();
};











