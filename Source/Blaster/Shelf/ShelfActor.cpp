// Fill out your copyright notice in the Description page of Project Settings.

#include "ShelfActor.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Kismet/KismetMathLibrary.h"
#include "EngineUtils.h"

AShelfActor::AShelfActor()
{
    PrimaryActorTick.bCanEverTick = false;

    // 메시 컴포넌트 생성
    ShelfMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShelfMesh"));
    RootComponent = ShelfMesh;

    // 상호작용 영역 생성
    InteractionArea = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionArea"));
    InteractionArea->SetupAttachment(RootComponent);
    InteractionArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionArea->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
    InteractionArea->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    InteractionArea->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);

    // 기본 설정
    MaxSlots = 6;
    InteractionDistance = 150.0f;
    AutoAlignDistance = 50.0f;
    AlignAnimationTime = 0.5f;
    SlotSpacing = 100.0f;
    bEnableDebugLogging = true;
}

void AShelfActor::BeginPlay()
{
    Super::BeginPlay();

    // 슬롯 초기화
    InitializeSlots();

    UE_LOG(LogTemp, Log, TEXT("[ShelfActor] Shelf initialized with %d slots"), MaxSlots);
}

void AShelfActor::InitializeSlots()
{
    Slots.Empty();
    Slots.SetNum(MaxSlots);

    // SlotMarkers를 기반으로 슬롯 위치 설정
    if (SlotMarkers.Num() > 0 && SlotMarkers.Num() >= MaxSlots)
    {
        for (int32 i = 0; i < MaxSlots; ++i)
        {
            if (SlotMarkers[i])
            {
                Slots[i].SlotTransform = SlotMarkers[i]->GetComponentTransform();
            }
            else
            {
                // 마커가 없으면 자동 계산
                FVector Offset = FVector(0.0f, i * SlotSpacing - (MaxSlots - 1) * SlotSpacing / 2.0f, 0.0f);
                Slots[i].SlotTransform = FTransform(GetActorRotation(), GetActorLocation() + Offset);
            }
        }
    }
    else
    {
        // 자동 배치: 선반 메시 앞면에 일정 간격으로 배치
        for (int32 i = 0; i < MaxSlots; ++i)
        {
            FVector LocalOffset = FVector(0.0f, i * SlotSpacing - (MaxSlots - 1) * SlotSpacing / 2.0f, 100.0f);
            FVector WorldOffset = GetActorTransform().TransformVector(LocalOffset);
            Slots[i].SlotTransform = FTransform(GetActorRotation(), GetActorLocation() + WorldOffset);
        }
    }

    if (bEnableDebugLogging)
    {
        for (int32 i = 0; i < Slots.Num(); ++i)
        {
            UE_LOG(LogTemp, Log, TEXT("[ShelfActor] Slot %d: %s"), i, *Slots[i].SlotTransform.GetLocation().ToString());
        }
    }
}

bool AShelfActor::TryPlaceParcel(AParcelActor* Parcel)
{
    if (!Parcel)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ShelfActor] Invalid parcel"));
        return false;
    }

    // 이미 배치된 Parcel인지 확인
    for (const FYShelfSlot& Slot : Slots)
    {
        if (Slot.PlacedParcel.Get() == Parcel)
        {
            UE_LOG(LogTemp, Warning, TEXT("[ShelfActor] Parcel already placed"));
            return false;
        }
    }

    // 빈 슬롯 찾기
    int32 SlotIndex = FindEmptySlot();
    if (SlotIndex == INDEX_NONE)
    {
        if (bEnableDebugLogging)
        {
            UE_LOG(LogTemp, Warning, TEXT("[ShelfActor] No empty slot available"));
        }
        return false;
    }

    // Parcel을 슬롯에 배치
    Slots[SlotIndex].PlacedParcel = Parcel;
    Slots[SlotIndex].bIsCorrect = ValidateSlot(SlotIndex, Parcel);

    // Parcel 물리 및 충돌 설정
    if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Parcel->GetRootComponent()))
    {
        RootPrim->SetSimulatePhysics(false);
        RootPrim->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }

    // 컨베이어에서 제거 (컨베이어 시스템 구현 시 활성화)
    // USplineMovementComponent* SplineMovement = Parcel->FindComponentByClass<USplineMovementComponent>();
    // if (SplineMovement)
    // {
    //     UWorld* World = GetWorld();
    //     if (World)
    //     {
    //         for (TActorIterator<AConveyorBeltActor> It(World); It; ++It)
    //         {
    //             AConveyorBeltActor* Conveyor = *It;
    //             if (Conveyor)
    //             {
    //                 Conveyor->DetachActorFromConveyor(Parcel);
    //                 break;
    //             }
    //         }
    //     }
    // }

    // 슬롯 위치로 정렬
    AlignParcelToSlot(Parcel, SlotIndex);

    // 이벤트 브로드캐스트
    OnParcelPlaced.Broadcast(Parcel, Slots[SlotIndex].bIsCorrect);

    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("[ShelfActor] Parcel placed in slot %d: %s (Correct: %s)"),
            SlotIndex, *Parcel->GetName(), Slots[SlotIndex].bIsCorrect ? TEXT("Yes") : TEXT("No"));
    }

    // 모든 슬롯이 올바르게 배치되었는지 확인
    if (IsAllCorrect())
    {
        OnAllCorrect.Broadcast();
        UE_LOG(LogTemp, Log, TEXT("[ShelfActor] All parcels correctly placed!"));
    }

    return true;
}

void AShelfActor::RemoveParcel(AParcelActor* Parcel)
{
    if (!Parcel)
    {
        return;
    }

    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        if (Slots[i].PlacedParcel.Get() == Parcel)
        {
            // 물리 다시 활성화
            if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Parcel->GetRootComponent()))
            {
                RootPrim->SetSimulatePhysics(true);
                RootPrim->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }

            Slots[i].PlacedParcel = nullptr;
            Slots[i].bIsCorrect = false;

            OnParcelRemoved.Broadcast(Parcel);

            if (bEnableDebugLogging)
            {
                UE_LOG(LogTemp, Log, TEXT("[ShelfActor] Parcel removed from slot %d: %s"), i, *Parcel->GetName());
            }
            break;
        }
    }
}

bool AShelfActor::IsFull() const
{
    for (const FYShelfSlot& Slot : Slots)
    {
        if (Slot.IsEmpty())
        {
            return false;
        }
    }
    return true;
}

int32 AShelfActor::GetCorrectPlacedCount() const
{
    int32 CorrectCount = 0;
    for (const FYShelfSlot& Slot : Slots)
    {
        if (Slot.IsCorrect())
        {
            ++CorrectCount;
        }
    }
    return CorrectCount;
}

bool AShelfActor::IsAllCorrect() const
{
    for (const FYShelfSlot& Slot : Slots)
    {
        if (!Slot.IsEmpty() && !Slot.IsCorrect())
        {
            return false;
        }
    }
    return true;
}

bool AShelfActor::CanInteractAtLocation(const FVector& Location, int32& OutSlotIndex) const
{
    OutSlotIndex = FindNearestSlot(Location);
    if (OutSlotIndex == INDEX_NONE)
    {
        return false;
    }

    float Distance = FVector::Dist(Location, Slots[OutSlotIndex].SlotTransform.GetLocation());
    return Distance <= InteractionDistance;
}

int32 AShelfActor::FindEmptySlot() const
{
    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        if (Slots[i].IsEmpty())
        {
            return i;
        }
    }
    return INDEX_NONE;
}

int32 AShelfActor::FindNearestSlot(const FVector& Location) const
{
    int32 NearestSlot = INDEX_NONE;
    float NearestDistance = MAX_FLT;

    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        float Distance = FVector::Dist(Location, Slots[i].SlotTransform.GetLocation());
        if (Distance < NearestDistance)
        {
            NearestDistance = Distance;
            NearestSlot = i;
        }
    }

    return NearestSlot;
}

bool AShelfActor::ValidateSlot(int32 SlotIndex, AParcelActor* Parcel)
{
    if (SlotIndex < 0 || SlotIndex >= Slots.Num() || !Parcel)
    {
        return false;
    }

    // 슬롯에 예상 태그가 없으면 항상 올바른 것으로 간주
    if (Slots[SlotIndex].ExpectedTags.Num() == 0)
    {
        return true;
    }

    // Parcel의 태그와 비교
    FGameplayTagContainer ParcelTags = Parcel->GetParcelTags();
    return ParcelTags.HasAll(Slots[SlotIndex].ExpectedTags);
}

void AShelfActor::AlignParcelToSlot(AParcelActor* Parcel, int32 SlotIndex)
{
    if (!Parcel || SlotIndex < 0 || SlotIndex >= Slots.Num())
    {
        return;
    }

    const FTransform& SlotTransform = Slots[SlotIndex].SlotTransform;
    
    // 즉시 정렬 (블루프린트에서 애니메이션 추가 가능)
    Parcel->SetActorTransform(SlotTransform);

    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("[ShelfActor] Aligned parcel to slot %d: %s"),
            SlotIndex, *SlotTransform.GetLocation().ToString());
    }
}

