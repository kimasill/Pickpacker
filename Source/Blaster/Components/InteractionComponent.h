// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionComponent.generated.h"

class AActor;
class UPrimitiveComponent;
class AShelfActor;

/**
 * 상호작용 가능한 액터 인터페이스
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UInteractableInterface : public UInterface
{
    GENERATED_BODY()
};

class BLASTER_API IInteractableInterface
{
    GENERATED_BODY()

public:
    /**
     * 상호작용 시작
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
    bool OnInteract(class ACharacter* Interactor);

    /**
     * 상호작용 가능 여부 확인
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    bool CanInteract(class ACharacter* Interactor) const;

    /**
     * 상호작용 텍스트 가져오기
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    FText GetInteractText() const;

    /**
     * 하이라이트 시작
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
    void StartHighlight();

    /**
     * 하이라이트 종료
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
    void EndHighlight();
};

/**
 * 상호작용 컴포넌트 - Line Trace 기반 상호작용 및 하이라이트 시스템
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BLASTER_API UInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInteractionComponent();
    
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /**
     * 상호작용 시작
     */
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void Interact();

    UFUNCTION(Server, Reliable)
	void Server_Interact(AActor* Target, int32 TargetSlotIndex);

    /**
     * 현재 타겟 액터 가져오기
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

    /**
     * 상호작용 가능한지 확인
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    bool CanInteract() const;

    /**
     * 오버랩된 액터 추가 (오버랩 이벤트에서 호출)
     */
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void AddOverlappingActor(AActor* Actor);

    /**
     * 오버랩된 액터 제거
     */
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void RemoveOverlappingActor(AActor* Actor);

    /**
     * 오버랩된 액터 목록 가져오기
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    TArray<AActor*> GetOverlappingActors() const;

    /**
     * 서버에서 Parcel 보유 상태 동기화
     */
    void SetCarriedParcel(class AParcelActor* NewParcel);

    /**
     * 현재 들고 있는 Parcel 가져오기
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    class AParcelActor* GetCarriedParcel() const { return CarriedParcel; }

    /**
     * 현재 포커스된 슬롯 인덱스
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    int32 GetFocusedSlotIndex() const { return FocusedSlotIndex; }


public:
    /** 상호작용 거리 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Settings")
    float InteractionDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Settings")
	float DropImpulse = 300.0f;

    /** Line Trace 시작 오프셋 (캐릭터 앞) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Settings")
    FVector TraceStartOffset = FVector(0.0f, 0.0f, 50.0f);

    /** Trace 채널 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Settings")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    /** 상호작용 가능한 액터 클래스 필터 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Settings")
    TArray<TSubclassOf<AActor>> AllowedActorClasses;

    /** 특정 인터페이스를 구현한 액터만 상호작용 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Settings")
    bool bRequireInteractableInterface = true;

    /** Line Trace로 타겟 우선순위 결정 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Settings")
    bool bUseLineTracePriority = true;

    /** 디버그 설정 */
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bDrawDebugTrace = false;

    /** 상호작용 성공 시 이벤트 */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractSuccess, AActor*, InteractedActor);
    UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
    FOnInteractSuccess OnInteractSuccess;

    /** 타겟 변경 시 이벤트 */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTargetChanged, AActor*, OldTarget, AActor*, NewTarget);
    UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
    FOnTargetChanged OnTargetChanged;


protected:
    /**
     * Line Trace 수행
     */
    void PerformTrace(FHitResult& OutHitResult);

    /**
     * 타겟 업데이트 (오버랩 목록 + Line Trace 우선순위)
     */
    void UpdateTarget();

    /**
     * 액터가 상호작용 가능한지 확인
     */
    bool IsActorInteractable(AActor* Actor) const;

    /**
     * 오버랩 목록에서 Line Trace로 우선 타겟 선택
     */
    AActor* SelectBestTargetFromOverlap(const TArray<AActor*>& InActors) const;

    /**
     * Parcel을 들고 있는지 확인
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    bool IsCarryingParcel() const { return CarriedParcel != nullptr; }

private:
    /** 현재 타겟 액터 */
    UPROPERTY()
    TWeakObjectPtr<AActor> CurrentTarget;

    /** 이전 프레임의 타겟 (하이라이트 관리용) */
    UPROPERTY()
    TWeakObjectPtr<AActor> PreviousTarget;

    /** 오버랩된 액터 목록 (오버랩 이벤트로 관리) */
    UPROPERTY()
    TArray<TWeakObjectPtr<AActor>> OverlappingActors;

    /** 현재 들고 있는 Parcel */
    UPROPERTY(ReplicatedUsing = OnRep_CarriedParcel)
    TObjectPtr<class AParcelActor> CarriedParcel = nullptr;

	UFUNCTION()
	void OnRep_CarriedParcel(class AParcelActor* LastParcel);

	void HandleCarriedParcelChanged(class AParcelActor* LastParcel);

	bool PerformInteract(AActor* Target, class ACharacter* OwnerCharacter);

	void UpdateShelfSlotFocus(AShelfActor* Shelf);
	void ClearShelfSlotFocus(AShelfActor* ShelfToClear = nullptr);

	UPROPERTY()
	TWeakObjectPtr<AShelfActor> FocusedShelf;

	int32 FocusedSlotIndex = INDEX_NONE;
};

