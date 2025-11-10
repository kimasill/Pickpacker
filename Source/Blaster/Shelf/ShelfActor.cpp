// Fill out your copyright notice in the Description page of Project Settings.

#include "ShelfActor.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Kismet/KismetMathLibrary.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInterface.h"

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

    // Setup overlap events (기존 Weapon 패턴)
    if (AreaSphere)
    {
        AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        AreaSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
        AreaSphere->OnComponentBeginOverlap.AddDynamic(this, &AShelfActor::OnSphereOverlap);
        AreaSphere->OnComponentEndOverlap.AddDynamic(this, &AShelfActor::OnSphereEndOverlap);
    }

    // 슬롯 초기화
    InitializeSlots();

    if (ShelfMesh)
    {
        ShelfMesh->SetRenderCustomDepth(false);
    }

    UE_LOG(LogTemp, Log, TEXT("[ShelfActor] Shelf initialized with %d slots"), MaxSlots);
}

void AShelfActor::InitializeSlots()
{
    Slots.Empty();
	const int32 MarkerCount = SlotMarkers.Num();
	const bool bHasMarkers = MarkerCount > 0;
	MaxSlots = bHasMarkers ? MarkerCount : MaxSlots;

    Slots.SetNum(MaxSlots);

    // SlotMarkers를 기반으로 슬롯 위치 설정
    if (bHasMarkers)
    {
        for (int32 i = 0; i < MaxSlots; ++i)
        {
            if (SlotMarkers[i])
            {
                Slots[i].SlotTransform = SlotMarkers[i]->GetComponentTransform();
            }
            else
            {
                // 마커가 없으면 자동 계산(삭제 예정: 선반 형태 여러개 존재)
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

	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShelfActor] TryPlaceParcel must run on the server"));
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

	// 선반에 올리기 전에 들고 있는 플레이어와의 연결 해제
	if (Parcel->IsAttached())
	{
		Parcel->RequestDrop(FVector::ZeroVector);
	}

    // Parcel을 슬롯에 배치
    Slots[SlotIndex].PlacedParcel = Parcel;
    Parcel->AssignToShelf(this, SlotIndex);
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

    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ShelfActor] RemoveParcel must run on the server"));
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
            Parcel->ClearShelfAssignment(this);

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

void AShelfActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (bAutoGatherSlotMarkers)
    {
        SlotMarkers.Empty();
        TArray<USceneComponent*> ChildComponents;
        GetComponents<USceneComponent>(ChildComponents);

        const FString MarkerPrefix = SlotMarkerPrefix.ToString();

        struct FEntry { int32 Index;  USceneComponent* Comp; };
        TArray<FEntry> MarkerEntries;

        for (USceneComponent* Comp : ChildComponents)
        {
            if (!Comp || Comp == RootComponent) continue;

            const FString CompName = Comp->GetName();
            if (!CompName.StartsWith(MarkerPrefix)) continue;

            const FString Suffix = CompName.Mid(MarkerPrefix.Len());
            int32 ParsedIndex = 0;
            if (Suffix.IsNumeric())
            {
                ParsedIndex = FCString::Atoi(*Suffix);
            }
            MarkerEntries.Add({ ParsedIndex, Comp });
        }

        MarkerEntries.Sort([](const FEntry& A, const FEntry& B) {return A.Index < B.Index; });
        
        for (const FEntry& Entry : MarkerEntries)
        {
            SlotMarkers.Add(Entry.Comp);
		}
    }
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

// InteractableInterface Implementation
bool AShelfActor::OnInteract_Implementation(ACharacter* Interactor)
{
    if (!Interactor)
    {
        return false;
    }

    // 캐릭터가 들고 있는 Parcel 찾기
    ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Interactor);
    if (!BlasterCharacter)
    {
        return false;
    }

    // InteractionComponent에서 현재 타겟이 이 선반인지 확인
    if (BlasterCharacter->GetInteractionComponent())
    {
        AActor* Target = BlasterCharacter->GetInteractionComponent()->GetCurrentTarget();
        if (Target != this)
        {
            return false;
        }
    }

    // 실제 Parcel 배치는 블루프린트에서 처리하도록 함
    // 또는 여기서 직접 처리 가능:
    // 블루프린트에서 CarriedParcel 변수를 확인하고 TryPlaceParcel 호출
    
    return CanInteract_Implementation(Interactor);
}

bool AShelfActor::CanInteract_Implementation(ACharacter* Interactor) const
{
    if (!Interactor)
    {
        return false;
    }

    // 캐릭터가 Parcel을 들고 있고 선반이 가득 차지 않았으면 상호작용 가능
    // 실제 구현은 블루프린트나 캐릭터 컴포넌트에서 처리
    return !IsFull();
}

FText AShelfActor::GetInteractText_Implementation() const
{
    if (IsFull())
    {
        return FText::FromString(TEXT("Shelf is Full"));
    }
    return FText::FromString(TEXT("Press E to Place Parcel"));
}

void AShelfActor::StartHighlight_Implementation()
{
    if (!ShelfMesh)
    {
        return;
    }

    // Custom Depth 사용 (간단한 하이라이트)
    ShelfMesh->SetRenderCustomDepth(true);
    ShelfMesh->SetCustomDepthStencilValue(252);
}

void AShelfActor::EndHighlight_Implementation()
{
    if (!ShelfMesh)
    {
        return;
    }

    ShelfMesh->SetRenderCustomDepth(false);
}

void AShelfActor::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
                                  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
                                  bool bFromSweep, const FHitResult& SweepResult)
{
    ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
    if (BlasterCharacter)
    {
        // 캐릭터에 오버랩 상태 설정 (기존 Weapon 패턴)
        if (BlasterCharacter->GetInteractionComponent())
        {
            BlasterCharacter->GetInteractionComponent()->AddOverlappingActor(this);
        }
    }
}

void AShelfActor::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
                                     UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
    if (BlasterCharacter)
    {
        // 캐릭터에서 오버랩 상태 제거
        if (BlasterCharacter->GetInteractionComponent())
        {
            BlasterCharacter->GetInteractionComponent()->RemoveOverlappingActor(this);
        }
    }
}

void AShelfActor::ShowInteractionWidget(bool bShowWidget)
{
    // 위젯 컴포넌트가 있다면 표시/숨김 처리
    // 필요시 위젯 컴포넌트 추가 가능
    UE_LOG(LogTemp, Verbose, TEXT("[ShelfActor] ShowInteractionWidget: %s"), bShowWidget ? TEXT("True") : TEXT("False"));
}

