// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Blaster/PickpackerTypes/PickpackerTypes.h"
#include "GameplayTagContainer.h"
#include "Blaster/Components/InteractionComponent.h"
#include "ShelfActor.generated.h"

class AParcelActor;
class USplineMovementComponent;
class AConveyorBeltActor;

/**
 * 선반 슬롯 구조체 - 선반의 한 칸을 나타냄
 */
USTRUCT(BlueprintType)
struct FYShelfSlot
{
    GENERATED_BODY()

    /** 슬롯에 올바른 Parcel이 배치되었는지 */
    UPROPERTY()
    bool bIsCorrect = false;

    /** 배치된 Parcel 액터 */
    UPROPERTY()
    TWeakObjectPtr<AParcelActor> PlacedParcel;

    /** 슬롯을 나타내는 마커 컴포넌트 */
    UPROPERTY()
    TWeakObjectPtr<USceneComponent> SlotMarker;

    /** 슬롯 위치 (월드 좌표) */
    UPROPERTY()
    FTransform SlotTransform;

    /** 슬롯 크기 (가로, 세로, 높이) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shelf")
    FVector SlotDimensions = FVector(80.0f, 80.0f, 80.0f);

    /** 예상되는 Parcel 태그 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shelf")
    FGameplayTagContainer ExpectedTags;

    /** 슬롯이 비어있는지 확인 */
    bool IsEmpty() const { return !PlacedParcel.IsValid(); }

    /** 올바른 Parcel이 배치되었는지 확인 */
    bool IsCorrect() const { return bIsCorrect; }
};

/**
 * 선반 액터 - Parcel을 배치할 수 있는 선반
 */
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API AShelfActor : public AActor, public IInteractableInterface
{
    GENERATED_BODY()

public:
    AShelfActor();

    virtual void BeginPlay() override;

    /**
     * Parcel을 선반에 배치 시도
     * @param Parcel 배치할 Parcel
     * @return 배치 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Shelf")
    bool TryPlaceParcel(AParcelActor* Parcel);

    /**
     * 특정 슬롯에 Parcel 배치 시도
     */
    UFUNCTION(BlueprintCallable, Category = "Shelf")
    bool TryPlaceParcelAtSlot(AParcelActor* Parcel, int32 SlotIndex);

    /**
     * Parcel을 선반에서 제거
     * @param Parcel 제거할 Parcel
     */
    UFUNCTION(BlueprintCallable, Category = "Shelf")
    void RemoveParcel(AParcelActor* Parcel);

    /**
     * 선반이 가득 찼는지 확인
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Shelf")
    bool IsFull() const;

    /**
     * 선반에 올바르게 배치된 Parcel 개수
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Shelf")
    int32 GetCorrectPlacedCount() const;

    /**
     * 선반의 모든 슬롯이 올바르게 배치되었는지
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Shelf")
    bool IsAllCorrect() const;

    /**
     * 해당 위치가 선반 상호작용 가능한지 확인
     * @param Location 확인할 위치
     * @param OutSlotIndex 가장 가까운 슬롯 인덱스 출력
     * @return 상호작용 가능 여부
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Shelf")
    bool CanInteractAtLocation(const FVector& Location, int32& OutSlotIndex) const;

    /**
     * 현재 포커스된 슬롯 인덱스
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Shelf")
    int32 GetFocusedSlotIndex() const { return FocusedSlotIndex; }

    /**
     * 뷰 방향으로 가장 잘 맞는 슬롯 찾기
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Shelf")
    int32 FindBestSlotForView(const FVector& ViewLocation, const FVector& ViewDirection, float MaxDistance) const;

    /**
     * 슬롯 하이라이트 업데이트
     */
    UFUNCTION(BlueprintCallable, Category = "Shelf")
    void SetFocusedSlot(int32 NewSlotIndex);

    /**
     * 슬롯 하이라이트 리셋
     */
    UFUNCTION(BlueprintCallable, Category = "Shelf")
    void ClearSlotHighlights();

    /**
     * 슬롯 수 반환
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Shelf")
    int32 GetSlotCount() const { return Slots.Num(); }

    /**
     * 슬롯 위치 반환
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Shelf")
    FVector GetSlotLocation(int32 SlotIndex) const;

    /**
     * 해당 슬롯에 Parcel 배치 가능 여부 확인
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Shelf")
    bool CanPlaceParcelAtSlot(AParcelActor* Parcel, int32 SlotIndex, FString& OutFailureReason) const;

public:
    /** 선반 메시 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* ShelfMesh;

    /** 오버랩 감지 영역 (기존 Weapon 패턴) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USphereComponent* AreaSphere;

    /** 상호작용 영역 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* InteractionArea;

    /** 슬롯 위치 마커들 (자식 컴포넌트로 배치) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TArray<USceneComponent*> SlotMarkers;

    /** 슬롯 데이터 */
    UPROPERTY()
    TArray<FYShelfSlot> Slots;

    /** 최대 슬롯 수 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shelf Settings")
    int32 MaxSlots = 0;

    /** 상호작용 거리 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shelf Settings")
    float InteractionDistance = 150.0f;

    /** 자동 정렬 거리 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shelf Settings")
    float AutoAlignDistance = 50.0f;

    /** 정렬 애니메이션 시간 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shelf Settings")
    float AlignAnimationTime = 0.5f;

    /** 슬롯 간격 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shelf Settings")
    float SlotSpacing = 100.0f;    

public:
    /** Parcel 배치 성공 시 이벤트 */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnParcelPlaced, AParcelActor*, Parcel, bool, bCorrect);
    UPROPERTY(BlueprintAssignable, Category = "Shelf|Events")
    FOnParcelPlaced OnParcelPlaced;

    /** Parcel 제거 시 이벤트 */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnParcelRemoved, AParcelActor*, Parcel);
    UPROPERTY(BlueprintAssignable, Category = "Shelf|Events")
    FOnParcelRemoved OnParcelRemoved;

    /** 모든 슬롯이 올바르게 배치되었을 때 이벤트 */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllCorrect);
    UPROPERTY(BlueprintAssignable, Category = "Shelf|Events")
    FOnAllCorrect OnAllCorrect;

	virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(EditAnywhere, Category = "Shelf|Markers")
    bool bAutoGatherSlotMarkers = true;

    UPROPERTY(EditAnywhere, Category = "Shelf|Markers")
    FName SlotMarkerPrefix = "SlotMarker_";

protected:
    /**
     * 빈 슬롯 찾기
     */
    UFUNCTION(BlueprintCallable, Category = "Shelf")
    int32 FindEmptySlot() const;

    /**
     * Parcel 위치에 가장 가까운 슬롯 찾기
     */
    UFUNCTION(BlueprintCallable, Category = "Shelf")
    int32 FindNearestSlot(const FVector& Location) const;

    /**
     * 슬롯 검증 (올바른 Parcel인지)
     */
    UFUNCTION(BlueprintCallable, Category = "Shelf")
    bool ValidateSlot(int32 SlotIndex, AParcelActor* Parcel);

    /**
     * Parcel을 슬롯 위치로 정렬
     */
    UFUNCTION(BlueprintCallable, Category = "Shelf")
    void AlignParcelToSlot(AParcelActor* Parcel, int32 SlotIndex);

    /**
     * 슬롯 초기화
     */
    void InitializeSlots();

    /**
     * Called when character overlaps with this shelf
     */
    UFUNCTION()
    void OnSphereOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    );

    /**
     * Called when character ends overlap with this shelf
     */
    UFUNCTION()
    void OnSphereEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex
    );

    /**
     * Show/hide interaction widget
     */
    UFUNCTION(BlueprintCallable, Category = "Shelf")
    void ShowInteractionWidget(bool bShowWidget);

    /** 디버그 설정 */
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bEnableDebugLogging = true;

    // InteractableInterface Implementation
    virtual bool OnInteract_Implementation(ACharacter* Interactor) override;
    virtual bool CanInteract_Implementation(ACharacter* Interactor) const override;
    virtual FText GetInteractText_Implementation() const override;
    virtual void StartHighlight_Implementation() override;
    virtual void EndHighlight_Implementation() override;

private:
    /** 슬롯 인덱스 유효성 검사 */
    bool IsValidSlotIndex(int32 SlotIndex) const;

    /** 슬롯 크기 검사 */
    bool CheckSlotSize(AParcelActor* Parcel, int32 SlotIndex, FString* OutFailureReason = nullptr) const;

    /** 슬롯 겹침 검사 */
    bool CheckSlotOverlap(AParcelActor* Parcel, int32 SlotIndex, FString* OutFailureReason = nullptr) const;

    /** 슬롯 하이라이트 적용 */
    void UpdateSlotHighlight(int32 SlotIndex, bool bEnable);

    /** 포커스 슬롯 내부 업데이트 */
    void UpdateFocusedSlotInternal(int32 NewSlotIndex);

    /** 하이라이트를 위한 원본 머티리얼 저장 */
    UPROPERTY()
    TArray<UMaterialInterface*> OriginalMaterials;

    /** 하이라이트 머티리얼 */
    UPROPERTY(EditAnywhere, Category = "Interaction")
    UMaterialInterface* HighlightMaterial = nullptr;

    /** 슬롯 하이라이트용 프리미티브 */
    UPROPERTY()
    TArray<TWeakObjectPtr<UPrimitiveComponent>> SlotHighlightPrimitives;

    /** 현재 포커스된 슬롯 인덱스 */
    UPROPERTY()
    int32 FocusedSlotIndex = INDEX_NONE;

    /** 슬롯 선택 최소 Dot 임계값 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shelf|Interaction", meta = (AllowPrivateAccess = "true"))
    float SlotSelectionDotThreshold = 0.75f;

    /** 슬롯 선택 최대 거리 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shelf|Interaction", meta = (AllowPrivateAccess = "true"))
    float SlotSelectionMaxDistance = 250.0f;

    /** 비어있는 슬롯 가중치 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shelf|Interaction", meta = (AllowPrivateAccess = "true"))
    float EmptySlotScoreBonus = 0.05f;

    /** 기본 슬롯 크기 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shelf|Interaction", meta = (AllowPrivateAccess = "true"))
    FVector DefaultSlotDimensions = FVector(80.0f, 80.0f, 80.0f);

    /** 슬롯 허용 패딩 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shelf|Interaction", meta = (AllowPrivateAccess = "true"))
    float SlotSizePadding = 2.5f;

    /** 슬롯 겹침 허용 오차 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shelf|Interaction", meta = (AllowPrivateAccess = "true"))
    float SlotOverlapTolerance = 2.0f;

    /** 슬롯 하이라이트 스텐실 값 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shelf|Interaction", meta = (AllowPrivateAccess = "true"))
    int32 SlotHighlightStencilValue = 253;
};

