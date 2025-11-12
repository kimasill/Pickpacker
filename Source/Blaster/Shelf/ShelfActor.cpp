#include "ShelfActor.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/ActorInstanceHandle.h"
#include "Engine/OverlapResult.h"
#include "WorldCollision.h" 
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInterface.h"
#include "CollisionShape.h"
#include "CollisionQueryParams.h"

bool AShelfActor::TryPlaceParcelAtSlot(AParcelActor* Parcel, int32 SlotIndex)
{
    if (!Parcel)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ShelfActor] Invalid parcel"));
        return false;
    }

    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ShelfActor] TryPlaceParcelAtSlot must run on the server"));
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

    const bool bSlotRequested = SlotIndex != INDEX_NONE;
    if (!bSlotRequested)
    {
        SlotIndex = FindEmptySlot();
        if (SlotIndex == INDEX_NONE)
        {
            if (bEnableDebugLogging)
            {
                UE_LOG(LogTemp, Warning, TEXT("[ShelfActor] No empty slot available"));
            }
            return false;
        }
    }

    if (!IsValidSlotIndex(SlotIndex))
    {
        if (bEnableDebugLogging)
        {
            UE_LOG(LogTemp, Warning, TEXT("[ShelfActor] Invalid slot index (%d)"), SlotIndex);
        }
        return false;
    }

    if (!Slots[SlotIndex].IsEmpty())
    {
        if (bEnableDebugLogging)
        {
            UE_LOG(LogTemp, Warning, TEXT("[ShelfActor] Slot %d already occupied"), SlotIndex);
        }
        return false;
    }

    // 먼저 캐리어로부터 분리하여 캐리어가 Overlap 테스트에 잡히지 않도록 함
    if (Parcel->IsAttached())
    {
        Parcel->RequestDrop(FVector::ZeroVector);
    }

    FString FailureReason;
    if (!CanPlaceParcelAtSlot(Parcel, SlotIndex, FailureReason))
    {
        if (bEnableDebugLogging)
        {
            UE_LOG(LogTemp, Warning, TEXT("[ShelfActor] Cannot place parcel into slot %d: %s"), SlotIndex, *FailureReason);
        }
        return false;
    }

    // Parcel을 슬롯에 배치
    FYShelfSlot& TargetSlot = Slots[SlotIndex];
    TargetSlot.PlacedParcel = Parcel;
    Parcel->AssignToShelf(this, SlotIndex);
    TargetSlot.bIsCorrect = ValidateSlot(SlotIndex, Parcel);

    // Parcel 물리 및 충돌 설정
    if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Parcel->GetRootComponent()))
    {
        RootPrim->SetSimulatePhysics(false);
        RootPrim->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }

    // 슬롯 위치로 정렬
    AlignParcelToSlot(Parcel, SlotIndex);

    // 이벤트 브로드캐스트
    OnParcelPlaced.Broadcast(Parcel, TargetSlot.bIsCorrect);

    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("[ShelfActor] Parcel placed in slot %d: %s (Correct: %s)"),
            SlotIndex, *Parcel->GetName(), TargetSlot.bIsCorrect ? TEXT("Yes") : TEXT("No"));
    }

    // 모든 슬롯이 올바르게 배치되었는지 확인
    if (IsAllCorrect())
    {
        OnAllCorrect.Broadcast();
        UE_LOG(LogTemp, Log, TEXT("[ShelfActor] All parcels correctly placed!"));
    }

    return true;
}
// Fill out your copyright notice in the Description page of Project Settings.

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
    SlotHighlightPrimitives.Empty();
    const int32 MarkerCount = SlotMarkers.Num();
    const bool bHasMarkers = MarkerCount > 0;
    MaxSlots = bHasMarkers ? MarkerCount : MaxSlots;

    Slots.SetNum(MaxSlots);
    SlotHighlightPrimitives.SetNum(MaxSlots);

    // SlotMarkers를 기반으로 슬롯 위치 설정
    if (bHasMarkers)
    {
        for (int32 i = 0; i < MaxSlots; ++i)
        {
            if (SlotMarkers[i])
            {
                Slots[i].SlotTransform = SlotMarkers[i]->GetComponentTransform();
                Slots[i].SlotMarker = SlotMarkers[i];

                /**
				* SlotMarker : USceneComponent, Location 기준
				* SlotMarker 자식: UPrimitiveComponent (Box Collision 또는 Static Mesh)
                */ 
                UPrimitiveComponent* FoundPrimitive = nullptr;
                TArray<USceneComponent*> ChildComponents;
                SlotMarkers[i]->GetChildrenComponents(false, ChildComponents);

                // Box Collision 또는 Static Mesh
                for (USceneComponent* Child : ChildComponents)
                {
                    if (UPrimitiveComponent* PrimitiveChild = Cast<UPrimitiveComponent>(Child))
                    {
                        FoundPrimitive = PrimitiveChild;
                        break; // 첫 번째 UPrimitiveComponent 사용
                    }
                }

                // 하이라이트 설정
                if (FoundPrimitive)
                {
                    SlotHighlightPrimitives[i] = FoundPrimitive;
                    FoundPrimitive->SetRenderCustomDepth(false);
                    FoundPrimitive->SetCustomDepthStencilValue(SlotHighlightStencilValue);
                    FoundPrimitive->SetHiddenInGame(true); // 게임에서 숨기기

                    // 크기 정보 가져오기
                    if (UBoxComponent* BoxChild = Cast<UBoxComponent>(FoundPrimitive))
                    {
                        Slots[i].SlotDimensions = BoxChild->GetScaledBoxExtent() * 2.0f;
                    }
                    else if (UStaticMeshComponent* MeshChild = Cast<UStaticMeshComponent>(FoundPrimitive))
                    {
                        if(UStaticMesh* StaticMesh = MeshChild->GetStaticMesh())
                        {
                            const FBoxSphereBounds MeshBounds = StaticMesh->GetBoundingBox();
							FVector Scale = MeshChild->GetComponentScale();
							FVector ScaledExtent = MeshBounds.BoxExtent * Scale;
							Slots[i].SlotDimensions = ScaledExtent * 2.0f;
						}
                        else
                        {
                            // Static Mesh가 없으면 CalcBounds 사용 (폴백)
                            const FBoxSphereBounds Bounds = MeshChild->CalcBounds(MeshChild->GetComponentTransform());
                            Slots[i].SlotDimensions = Bounds.BoxExtent * 2.0f;
                        }
                    }
                    else
                    {
                        // 일반 PrimitiveComponent의 경우 바운드로 크기 계산
                        const FBoxSphereBounds Bounds = FoundPrimitive->CalcBounds(FoundPrimitive->GetComponentTransform());
                        Slots[i].SlotDimensions = Bounds.BoxExtent * 2.0f;
                    }
                }
                else
                {
                    SlotHighlightPrimitives[i] = nullptr;
                    Slots[i].SlotDimensions = DefaultSlotDimensions;
                    if (bEnableDebugLogging)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("[ShelfActor] SlotMarker %d has no UPrimitiveComponent child. Highlight and size detection will not work. Add a Box Collision or Static Mesh as a child component."), i);
                    }
                }
            }
            else
            {
                // 마커가 없으면 자동 계산
                FVector Offset = FVector(0.0f, i * SlotSpacing - (MaxSlots - 1) * SlotSpacing / 2.0f, 0.0f);
                Slots[i].SlotTransform = FTransform(GetActorRotation(), GetActorLocation() + Offset);
                Slots[i].SlotMarker = nullptr;
                Slots[i].SlotDimensions = DefaultSlotDimensions;
                SlotHighlightPrimitives[i] = nullptr;
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
            Slots[i].SlotMarker = nullptr;
            Slots[i].SlotDimensions = DefaultSlotDimensions;
            SlotHighlightPrimitives[i] = nullptr;
        }
    }

    FocusedSlotIndex = INDEX_NONE;

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
    return TryPlaceParcelAtSlot(Parcel, INDEX_NONE);
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

bool AShelfActor::IsValidSlotIndex(int32 SlotIndex) const
{
    return Slots.IsValidIndex(SlotIndex);
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

bool AShelfActor::CheckSlotSize(AParcelActor* Parcel, int32 SlotIndex, FString* OutFailureReason) const
{
    if (!Parcel || !IsValidSlotIndex(SlotIndex))
    {
        return false;
    }

    const USceneComponent* RootParcel = Parcel->GetRootComponent();
    const UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(RootParcel);
    if (!RootPrim)
    {
        return true;
    }

    const FYShelfSlot& Slot = Slots[SlotIndex];

    // Use the parcel's root component, not this actor's RootComponent
    const FTransform ComponentRelativeTransform = RootParcel->GetRelativeTransform();
    const FTransform DesiredComponentTransform = ComponentRelativeTransform * Slot.SlotTransform;

    const FBoxSphereBounds DesiredBounds = RootPrim->CalcBounds(DesiredComponentTransform);
    const FVector ParcelHalfSize = DesiredBounds.BoxExtent;
    FVector AllowedHalfSize = Slot.SlotDimensions * 0.5f - FVector(SlotSizePadding);
    AllowedHalfSize = AllowedHalfSize.ComponentMax(FVector::ZeroVector);

    const bool bFits =
        ParcelHalfSize.X <= AllowedHalfSize.X &&
        ParcelHalfSize.Y <= AllowedHalfSize.Y &&
        ParcelHalfSize.Z <= AllowedHalfSize.Z;

    if (!bFits && OutFailureReason)
    {
        *OutFailureReason = FString::Printf(
            TEXT("Parcel size %s exceeds slot allowance %s"),
            *ParcelHalfSize.ToString(),
            *AllowedHalfSize.ToString());
    }

    return bFits;
}

bool AShelfActor::CheckSlotOverlap(AParcelActor* Parcel, int32 SlotIndex, FString* OutFailureReason) const
{
    if (!Parcel || !IsValidSlotIndex(SlotIndex))
    {
        return false;
    }

    const USceneComponent* RootParcel = Parcel->GetRootComponent();
    const UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(RootParcel);
    if (!RootPrim)
    {
        return true; // 루트가 프리미티브가 아니면 Overlap 검사 불필요
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        if (OutFailureReason)
        {
            *OutFailureReason = TEXT("World unavailable");
        }
        return false;
    }

    const FYShelfSlot& Slot = Slots[SlotIndex];
    const FTransform ComponentRelativeTransform = RootParcel->GetRelativeTransform();
    const FTransform DesiredComponentTransform = ComponentRelativeTransform * Slot.SlotTransform;
    const FBoxSphereBounds DesiredBounds = RootPrim->CalcBounds(DesiredComponentTransform);
    const FVector HalfExtent = DesiredBounds.BoxExtent + FVector(SlotOverlapTolerance);

    if (HalfExtent.IsNearlyZero())
    {
        return true;
    }

    FCollisionShape CollisionShape = FCollisionShape::MakeBox(HalfExtent);

    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
    ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShelfSlotOverlap), false);
    QueryParams.AddIgnoredActor(this);       // 선반 자체 무시
    QueryParams.AddIgnoredActor(Parcel);     // 검사 중인 Parcel 무시

    // 캐리어(아직 붙어 있을 가능성) 무시
    if (AActor* Parent = Parcel->GetAttachParentActor())
    {
        QueryParams.AddIgnoredActor(Parent);
    }

    TArray<FOverlapResult> OverlapResults;
    const bool bHasOverlap = World->OverlapMultiByObjectType(
        OverlapResults,
        DesiredBounds.Origin,
        DesiredComponentTransform.GetRotation(),
        ObjectQueryParams,
        CollisionShape,
        QueryParams);

    if (!bHasOverlap)
    {
        return true; // 아무 것도 겹치지 않음
    }

    for (const FOverlapResult& Res : OverlapResults)
    {
        AActor* OverlapActor = Res.GetActor();
        if (!OverlapActor)
        {
            continue;
        }

        // 이미 무시 설정한 것들 또는 자기 자신/검사 대상 Parcel은 continue
        if (OverlapActor == this || OverlapActor == Parcel)
        {
            continue;
        }

        // 캐리어(드롭 전 단계에서 남아있는 경우)도 허용
        if (OverlapActor == Parcel->GetAttachParentActor())
        {
            continue;
        }

        // 슬롯에 배치된 다른 Parcel과의 충돌만 실패 처리
        bool bIsPlacedParcel = false;
        for (const FYShelfSlot& ExistingSlot : Slots)
        {
            if (ExistingSlot.PlacedParcel.IsValid() && ExistingSlot.PlacedParcel.Get() == OverlapActor)
            {
                bIsPlacedParcel = true;
                break;
            }
        }

        if (!bIsPlacedParcel)
        {
            // 선반 주변의 다른 오브젝트(캐릭터, 환경 등)는 무시 (원치 않는 false 판정 방지)
            continue;
        }

        if (OutFailureReason)
        {
            *OutFailureReason = FString::Printf(TEXT("Overlaps placed parcel: %s"), *OverlapActor->GetName());
        }
        return false;
    }

    return true;
}

bool AShelfActor::CanPlaceParcelAtSlot(AParcelActor* Parcel, int32 SlotIndex, FString& OutFailureReason) const
{
    OutFailureReason.Reset();

    if (!Parcel)
    {
        OutFailureReason = TEXT("Parcel is invalid");
        return false;
    }

    if (!IsValidSlotIndex(SlotIndex))
    {
        OutFailureReason = TEXT("Invalid slot index");
        return false;
    }

    if (!Slots[SlotIndex].IsEmpty())
    {
        OutFailureReason = TEXT("Slot already occupied");
        return false;
    }

    if (!CheckSlotSize(Parcel, SlotIndex, &OutFailureReason))
    {
        if (OutFailureReason.IsEmpty())
        {
            OutFailureReason = TEXT("Parcel is too large for the slot");
        }
        return false;
    }

    if (!CheckSlotOverlap(Parcel, SlotIndex, &OutFailureReason))
    {
        if (OutFailureReason.IsEmpty())
        {
            OutFailureReason = TEXT("Parcel overlaps other objects");
        }
        return false;
    }

    return true;
}

FVector AShelfActor::GetSlotLocation(int32 SlotIndex) const
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return FVector::ZeroVector;
    }

    return Slots[SlotIndex].SlotTransform.GetLocation();
}

int32 AShelfActor::FindBestSlotForView(const FVector& ViewLocation, const FVector& ViewDirection, float MaxDistance) const
{
    if (Slots.Num() == 0)
    {
        return INDEX_NONE;
    }

    const float EffectiveMaxDistance = MaxDistance > 0.0f
        ? FMath::Min(MaxDistance, SlotSelectionMaxDistance)
        : SlotSelectionMaxDistance;
    int32 BestIndex = INDEX_NONE;
    float BestScore = SlotSelectionDotThreshold;

    for (int32 i = 0; i < Slots.Num(); ++i)
    {
        const FVector SlotLocation = Slots[i].SlotTransform.GetLocation();
        const FVector ToSlot = SlotLocation - ViewLocation;
        const float Distance = ToSlot.Size();

        if (Distance > EffectiveMaxDistance || Distance <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        const FVector DirectionToSlot = ToSlot / Distance;
        const float Dot = FVector::DotProduct(ViewDirection.GetSafeNormal(), DirectionToSlot);

        if (Dot < SlotSelectionDotThreshold)
        {
            continue;
        }

        float Score = Dot;
        if (!Slots[i].PlacedParcel.IsValid())
        {
            Score += EmptySlotScoreBonus;
        }

        if (Score > BestScore)
        {
            BestScore = Score;
            BestIndex = i;
        }
    }

    return BestIndex;
}

void AShelfActor::UpdateSlotHighlight(int32 SlotIndex, bool bEnable)
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return;
    }

    if (UPrimitiveComponent* HighlightComp = SlotHighlightPrimitives[SlotIndex].Get())
    {
		HighlightComp->SetHiddenInGame(!bEnable);
        HighlightComp->SetRenderCustomDepth(bEnable);
        HighlightComp->SetCustomDepthStencilValue(SlotHighlightStencilValue);
    }
}

void AShelfActor::UpdateFocusedSlotInternal(int32 NewSlotIndex)
{
    if (FocusedSlotIndex == NewSlotIndex)
    {
        return;
    }

    if (IsValidSlotIndex(FocusedSlotIndex))
    {
        UpdateSlotHighlight(FocusedSlotIndex, false);
    }

    FocusedSlotIndex = NewSlotIndex;

    if (IsValidSlotIndex(FocusedSlotIndex))
    {
        UpdateSlotHighlight(FocusedSlotIndex, true);
    }
}

void AShelfActor::SetFocusedSlot(int32 NewSlotIndex)
{
    if (NewSlotIndex != INDEX_NONE && !IsValidSlotIndex(NewSlotIndex))
    {
        return;
    }

    UpdateFocusedSlotInternal(NewSlotIndex);
}

void AShelfActor::ClearSlotHighlights()
{
    for (int32 i = 0; i < SlotHighlightPrimitives.Num(); ++i)
    {
        if (UPrimitiveComponent* HighlightComp = SlotHighlightPrimitives[i].Get())
        {
            HighlightComp->SetRenderCustomDepth(false);
        }
    }
    FocusedSlotIndex = INDEX_NONE;
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
    
    return CanInteract_Implementation(Interactor);
}

bool AShelfActor::CanInteract_Implementation(ACharacter* Interactor) const
{
    if (!Interactor)
    {
        return false;
    }
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

    if (IsValidSlotIndex(FocusedSlotIndex))
    {
        UpdateSlotHighlight(FocusedSlotIndex, true);
    }
}

void AShelfActor::EndHighlight_Implementation()
{
    if (!ShelfMesh)
    {
        return;
    }

    ShelfMesh->SetRenderCustomDepth(false);
    ClearSlotHighlights();
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

