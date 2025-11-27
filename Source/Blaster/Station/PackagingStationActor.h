// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/Components/InteractionComponent.h"
#include "PackagingStationActor.generated.h"

/**
 * 포장 모드 열거형
 */
UENUM(BlueprintType)
enum class EPackagingMode : uint8
{
	Pack		UMETA(DisplayName = "Pack"),
	Unpack		UMETA(DisplayName = "Unpack")
};

/**
 * 포장 스테이션 액터 - Parcel을 포장하고 포장 해제하는 시스템
 * 단일 입력 영역에서 모드에 따라 포장/포장 해제 처리
 */
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API APackagingStationActor : public AActor
{
	GENERATED_BODY()

public:
	APackagingStationActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/**
	 * 모드 전환 (Pack <-> Unpack)
	 */
	UFUNCTION(BlueprintCallable, Category = "Packaging")
	void ToggleMode();

	/**
	 * 모드 설정
	 */
	UFUNCTION(BlueprintCallable, Category = "Packaging")
	void SetMode(EPackagingMode NewMode);

	/**
	 * 현재 모드 가져오기
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Packaging")
	EPackagingMode GetMode() const { return CurrentMode; }

	/**
	 * 입력 영역에서 Parcel 찾기 및 처리
	 */
	UFUNCTION(BlueprintCallable, Category = "Packaging")
	void ProcessInputArea();

	/**
	 * Parcel 포장 처리
	 */
	UFUNCTION(BlueprintCallable, Category = "Packaging")
	void PackageParcel(AParcelActor* Parcel);

	/**
	 * Parcel 포장 해제 처리
	 */
	UFUNCTION(BlueprintCallable, Category = "Packaging")
	void UnpackageParcel(AParcelActor* Parcel);

public:
	/** 스테이션 메시 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* StationMesh;

	/** 입력 영역 (Parcel을 놓는 곳) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* InputArea;

	/** 출력 위치 (처리된 Parcel이 나오는 곳) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* OutputLocation;

	/** 현재 모드 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Packaging")
	EPackagingMode CurrentMode = EPackagingMode::Pack;

	/** 처리 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Packaging", meta = (ClampMin = "0.1"))
	float ProcessingTime = 2.0f;

	/** 자동 처리 활성화 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Packaging")
	bool bAutoProcess = true;

	/** 처리 간격 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Packaging", meta = (ClampMin = "0.1"))
	float ProcessingInterval = 1.0f;

	/** 디버그 설정 */
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bEnableDebugLogging = true;

public:
	/** 포장 완료 이벤트 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnParcelPackaged, AParcelActor*, Parcel);
	UPROPERTY(BlueprintAssignable, Category = "Packaging|Events")
	FOnParcelPackaged OnParcelPackaged;

	/** 포장 해제 완료 이벤트 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnParcelUnpackaged, AParcelActor*, Parcel);
	UPROPERTY(BlueprintAssignable, Category = "Packaging|Events")
	FOnParcelUnpackaged OnParcelUnpackaged;

	/** 모드 변경 이벤트 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModeChanged, EPackagingMode, NewMode);
	UPROPERTY(BlueprintAssignable, Category = "Packaging|Events")
	FOnModeChanged OnModeChanged;

protected:
	/**
	 * 입력 영역 오버랩 이벤트
	 */
	UFUNCTION()
	void OnInputAreaOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	/**
	 * 입력 영역에서 Parcel 찾기
	 */
	AParcelActor* FindParcelInArea() const;

	/**
	 * 처리 큐에 추가
	 */
	void AddToProcessingQueue(AParcelActor* Parcel);

	/**
	 * 처리 큐에서 하나 처리
	 */
	void ProcessNextInQueue();

	/**
	 * 처리 완료 콜백
	 */
	UFUNCTION()
	void OnProcessingComplete();

private:
	/** 처리 중인지 여부 */
	UPROPERTY()
	bool bIsProcessing = false;

	/** 처리 대기 큐 */
	UPROPERTY()
	TArray<TWeakObjectPtr<AParcelActor>> ProcessingQueue;

	/** 현재 처리 중인 Parcel */
	UPROPERTY()
	TWeakObjectPtr<AParcelActor> CurrentProcessingParcel;

	/** 처리 타이머 핸들 */
	UPROPERTY()
	FTimerHandle ProcessingTimerHandle;

	/** 자동 처리 타이머 핸들 */
	UPROPERTY()
	FTimerHandle AutoProcessTimerHandle;
};
