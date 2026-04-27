// Fill out your copyright notice in the Description page of Project Settings.

#include "InteractionComponent.h"
#include "Interfaces/InteractableInterface.h"
#include "Parcel/ParcelActor.h"
#include "Shelf/ShelfActor.h"
#include "Character/BlasterCharacter.h"
#include "Components/PlayerInventoryComponent.h"
#include "Components/CreditUnlockComponent.h"
#include "Interfaces/GameplayActionInterface.h"
#include "Library/DynamicGameplayStatics.h"
#include "Components/ParcelStateComponent.h"
#include "GameState/PickpackerGameState.h"
#include "Interaction/InteractionUIData.h"
#include "Gate/GateActor.h"
#include "UI/InteractionPromptWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PrimitiveComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/InputSettings.h"
#include "Engine/LocalPlayer.h"
#include "PickpackerAssetPaths.h"
#include "UObject/ConstructorHelpers.h"

// Removed EnhancedInputSubsystems include and usage due to API mismatch; use legacy InputSettings instead.

UInteractionComponent::UInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    InteractionDistance = 300.0f;
    TraceStartOffset = FVector(0.0f, 0.0f, 50.0f);
    TraceChannel = ECC_GameTraceChannel3;
    bRequireInteractableInterface = true;
    bDrawDebugTrace = false;
    SetIsReplicatedByDefault(true);

    static ConstructorHelpers::FClassFinder<UUserWidget> InteractionWidgetClassRef(
        PickpackerAssetPaths::Blueprints::WidgetInteraction);
    if (InteractionWidgetClassRef.Succeeded())
    {
        InteractionWidget = InteractionWidgetClassRef.Class;
    }
    else
    {
        InteractionWidget = UInteractionPromptWidget::StaticClass();
    }
}

void UInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UInteractionComponent, CarriedParcel, COND_OwnerOnly);
}

void UInteractionComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;
    OverlappingActors.RemoveAll([](const TWeakObjectPtr<AActor>& ActorPtr)
    {
        return !ActorPtr.IsValid();
    });
    UpdateTarget();
}

void UInteractionComponent::ShowInteractionWidget(AActor* TargetActor)
{
    if (!InteractionWidget || !TargetActor)
    {
        HideInteractionWidget();
        return;
    }

    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    APlayerController* PlayerController = OwnerCharacter ? Cast<APlayerController>(OwnerCharacter->GetController()) : nullptr;
    if (!OwnerCharacter || !PlayerController || !PlayerController->IsLocalController())
    {
        HideInteractionWidget();
        return;
    }

    if (ActiveInteractionWidget && ActiveInteractionWidget->GetClass() != InteractionWidget)
    {
        HideInteractionWidget();
    }

    if (!ActiveInteractionWidget)
    {
        ActiveInteractionWidget = CreateWidget<UUserWidget>(PlayerController, InteractionWidget);
        if (!ActiveInteractionWidget)
        {
            return;
        }
        ActiveInteractionWidget->AddToViewport(50);
    }
    else if (!ActiveInteractionWidget->IsInViewport())
    {
        ActiveInteractionWidget->AddToViewport(50);
    }

    if (ActiveInteractionWidgetComponent.IsValid())
    {
        ActiveInteractionWidgetComponent->DestroyComponent();
        ActiveInteractionWidgetComponent = nullptr;
    }

    ActiveInteractionWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

    UpdateInteractionWidgetUI(TargetActor, ActiveInteractionWidget);
    UpdateInteractionWidgetScreenPosition(TargetActor);
}

void UInteractionComponent::HideInteractionWidget()
{
    if (UInteractionPromptWidget* PromptWidget = Cast<UInteractionPromptWidget>(ActiveInteractionWidget))
    {
        PromptWidget->ClearInteractionData();
    }

    if (ActiveInteractionWidget)
    {
        ActiveInteractionWidget->RemoveFromParent();
        ActiveInteractionWidget = nullptr;
    }

    if (ActiveInteractionWidgetComponent.IsValid())
    {
        ActiveInteractionWidgetComponent->DestroyComponent();
        ActiveInteractionWidgetComponent = nullptr;
    }
}

static void SetParcelTargetedFlag(AActor* Actor, bool bTargeted)
{
	if (AParcelActor* Parcel = Cast<AParcelActor>(Actor))
	{
		Parcel->SetTargetedByLocalPlayer(bTargeted);
	}
}

UWidgetComponent* UInteractionComponent::FindInteractionWidgetAnchor(AActor* TargetActor) const
{
    if (!TargetActor)
    {
        return nullptr;
    }

    TArray<UWidgetComponent*> WidgetComponents;
    TargetActor->GetComponents<UWidgetComponent>(WidgetComponents);

    for (UWidgetComponent* WidgetComp : WidgetComponents)
    {
        if (!WidgetComp || WidgetComp == ActiveInteractionWidgetComponent.Get())
        {
            continue;
        }

        const bool bTaggedAnchor = WidgetComp->ComponentHasTag(FName("InteractionAnchor")) || WidgetComp->ComponentHasTag(FName("InteractionUIAnchor"));
        const bool bNamedAnchor = WidgetComp->GetName().Contains(TEXT("InteractionAnchor")) || WidgetComp->GetName().Contains(TEXT("UIAnchor"));
        if (bTaggedAnchor || bNamedAnchor)
        {
            return WidgetComp;
        }
    }

    return nullptr;
}

bool UInteractionComponent::InvokeWidgetCreditUpdate(UUserWidget* Widget, bool bRequiresUnlock, int32 UnlockCost, const FText& LockedMessage, const FText& UnlockedMessage, int32 CurrentCredits)
{
    if (!Widget)
    {
        return false;
    }

    // If no unlock required or zero cost, try clear handler first.
    if (!bRequiresUnlock || UnlockCost <= 0)
    {
        static const FName ClearFuncName(TEXT("OnInteractionCreditInfoCleared"));
        if (UFunction* ClearFunc = Widget->FindFunction(ClearFuncName))
        {
            Widget->ProcessEvent(ClearFunc, nullptr);
            return true;
        }
        return false;
    }

    // Unlock path: provide data to a BP event if present.
    static const FName UpdateFuncName(TEXT("OnInteractionCreditInfoUpdated"));
    if (UFunction* UpdateFunc = Widget->FindFunction(UpdateFuncName))
    {
        struct FUpdateParams
        {
            bool bRequiresUnlock;
            int32 UnlockCost;
            FText LockedMessage;
            FText UnlockedMessage;
            int32 CurrentCredits;
        };

        FUpdateParams Params{ bRequiresUnlock, UnlockCost, LockedMessage, UnlockedMessage, CurrentCredits };
        Widget->ProcessEvent(UpdateFunc, &Params);
        return true;
    }

    return false;
}

static UUserWidget* GetWidgetFromComponent(TWeakObjectPtr<UWidgetComponent> WidgetComp)
{
    return WidgetComp.IsValid() ? WidgetComp->GetUserWidgetObject() : nullptr;
}

void UInteractionComponent::UpdateInteractionWidgetCreditInfo(AActor* TargetActor)
{
    UUserWidget* WidgetToUse = ActiveInteractionWidget;
    if (!WidgetToUse && ActiveInteractionWidgetComponent.IsValid())
    {
        WidgetToUse = GetWidgetFromComponent(ActiveInteractionWidgetComponent);
    }
    if (!WidgetToUse)
    {
        return;
    }

    FInteractionUIData UIData;
    if (!GatherInteractionUIData(TargetActor, UIData))
    {
        return;
    }

    InvokeWidgetCreditUpdate(WidgetToUse, UIData.bRequiresUnlock, UIData.UnlockCost, UIData.LockedMessage, UIData.UnlockedMessage, UIData.CurrentCredits);
}

void UInteractionComponent::UpdateTarget()
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;

    UCameraComponent* Camera = OwnerCharacter->FindComponentByClass<UCameraComponent>();
    const FVector CameraLocation = Camera ? Camera->GetComponentLocation() : OwnerCharacter->GetActorLocation() + OwnerCharacter->GetActorRotation().RotateVector(TraceStartOffset);
    const FVector Direction = Camera ? Camera->GetForwardVector() : OwnerCharacter->GetActorForwardVector();
    const FVector Start = CameraLocation;
    const FVector End   = CameraLocation + Direction * InteractionDistance;

    // Primary interaction trace channel (existing logic)
    const ECollisionChannel InteractionChannel = TraceChannel; // expected ECC_GameTraceChannel3
    // Separate shelf trace channel (only when carrying a parcel)
    const ECollisionChannel ShelfTraceChannel = ECC_GameTraceChannel4;

    // 추가 허용 반경: 약간 벗어난 대상도 선택되도록 캡슐 스윕 사용
    const float TargetingRadius = 30.0f;

    // Build base query params
    FCollisionQueryParams InteractionParams(SCENE_QUERY_STAT(InteractionTargetTrace), false);
    InteractionParams.bReturnPhysicalMaterial = false;
    InteractionParams.AddIgnoredActor(OwnerCharacter);


    if (IsValid(CarriedParcel))
    {
        // Ignore carried parcel so it does not consume primary interaction hit when looking at shelves/items
        InteractionParams.AddIgnoredActor(CarriedParcel);
        if (AActor* Carrier = CarriedParcel->GetAttachParentActor())
        {
            InteractionParams.AddIgnoredActor(Carrier);
        }
    }
        
    // Optional shelf trace (only while carrying a parcel) - allow seeing shelf even if parcel blocks interaction channel

    // Perform primary single trace (interaction channel)
    TArray<FHitResult> InteractionHits;
    bool bInteractionTrace = false;

    FHitResult InteractionHit;
    bInteractionTrace = GetWorld() && GetWorld()->LineTraceSingleByChannel(InteractionHit, Start, End, InteractionChannel, InteractionParams);
    if (bInteractionTrace)
    {
        InteractionHits.Add(InteractionHit);
    }

    // 라인 트레이스에 없으면/보완용으로 좁은 캡슐 스윕 추가 (싱글)
    if (!bInteractionTrace && GetWorld())
    {
        FHitResult SweepHit;
        const FCollisionShape Capsule = FCollisionShape::MakeCapsule(TargetingRadius, TargetingRadius);
        if (GetWorld()->SweepSingleByChannel(SweepHit, Start, End, FQuat::Identity, InteractionChannel, Capsule, InteractionParams))
        {
            InteractionHits.Add(SweepHit);
            bInteractionTrace = true;
        }
    }
    TArray<FHitResult> ShelfHits;
    AShelfActor* BestShelfFromTrace = nullptr;
    if (IsValid(CarriedParcel) && GetWorld())
    {
        FCollisionQueryParams ShelfParams = InteractionParams;
        if (GetWorld()->LineTraceMultiByChannel(ShelfHits, Start, End, ShelfTraceChannel, ShelfParams))
        {
            // Map: AShelfActor* -> closest hit distance (smallest wins)
            TMap<AShelfActor*, float> ShelfToDistance;
            for (const FHitResult& H : ShelfHits)
            {
                AShelfActor* Shelf = Cast<AShelfActor>(H.GetActor());
                if (!Shelf) continue;

                const float Dist = FVector::Dist(Start, H.ImpactPoint);
                float* Existing = ShelfToDistance.Find(Shelf);
                if (!Existing || Dist < *Existing)
                {
                    ShelfToDistance.Add(Shelf, Dist);
                }
            }

            if (ShelfToDistance.Num() > 0)
            {
                // Reference height: carried parcel (prefer shelves lower than item)
                float ParcelRefZ = CarriedParcel->GetActorLocation().Z;

                // Get placement surface top Z for each shelf
                auto GetShelfSurfaceZ = [](AShelfActor* Shelf) -> float
                {
                    if (!Shelf || !Shelf->PlacementSurface) return Shelf ? Shelf->GetActorLocation().Z : 0.f;
                    const UBoxComponent* Box = Shelf->PlacementSurface;
                    FVector TopWorld = Box->GetComponentTransform().TransformPosition(FVector(0, 0, Box->GetScaledBoxExtent().Z));
                    return TopWorld.Z;
                };

                // Sort: 1) prefer shelf surface Z < parcel Z (lower shelves first), 2) then by distance (closest)
                TArray<TPair<AShelfActor*, float>> Sorted;
                for (const auto& Elem : ShelfToDistance)
                {
                    Sorted.Add(TPair<AShelfActor*, float>(Elem.Key, Elem.Value));
                }
                Sorted.Sort([ParcelRefZ, GetShelfSurfaceZ](const TPair<AShelfActor*, float>& A, const TPair<AShelfActor*, float>& B)
                {
                    const float AZ = GetShelfSurfaceZ(A.Key);
                    const float BZ = GetShelfSurfaceZ(B.Key);
                    const bool AIsLower = AZ < ParcelRefZ;
                    const bool BIsLower = BZ < ParcelRefZ;
                    if (AIsLower != BIsLower) return AIsLower; // lower shelf wins
                    return A.Value < B.Value; // same tier: closer wins
                });

                AShelfActor* ClosestShelf = Sorted[0].Key;
                const float ClosestDist = Sorted[0].Value;

                // Hysteresis: when stacked shelves are close, keep current target to prevent flickering
                const float ShelfHysteresisDist = 35.0f;
                AShelfActor* CurrentShelf = Cast<AShelfActor>(CurrentTarget.Get());
                if (CurrentShelf && ShelfToDistance.Contains(CurrentShelf))
                {
                    const float CurrentDist = ShelfToDistance[CurrentShelf];
                    if (ClosestShelf != CurrentShelf && FMath::Abs(ClosestDist - CurrentDist) < ShelfHysteresisDist)
                    {
                        BestShelfFromTrace = CurrentShelf; // keep current to avoid jitter
                    }
                }

                if (!BestShelfFromTrace)
                {
                    BestShelfFromTrace = ClosestShelf;
                }
            }
        }
    }

#if !UE_BUILD_SHIPPING
    if (bDrawDebugTrace)
    {
        const FColor LineColor = bInteractionTrace ? FColor::Green : FColor::Red;
        DrawDebugLine(GetWorld(), Start, End, LineColor, false, 0.05f, 0, 0.5f);
        for (const FHitResult& H : InteractionHits)
        {
            DrawDebugSphere(GetWorld(), H.ImpactPoint, 6.f, 12, FColor::Yellow, false, 0.05f);
        }
        if (BestShelfFromTrace)
        {
            DrawDebugSphere(GetWorld(), BestShelfFromTrace->GetActorLocation(), 10.f, 16, FColor::Cyan, false, 0.1f);
        }
    }
#endif

    // Select best interactable from primary hits (가장 시선 중심에 가까운 것 우선)
    AActor* NewTarget = nullptr;
    AActor* PrimaryTaggedTarget = nullptr;
    float BestScore = TNumericLimits<float>::Max();
    float BestPrimaryScore = TNumericLimits<float>::Max();

    if (InteractionHits.Num() > 0)
    {
        const float MaxDistance = FMath::Max(InteractionDistance, 1.0f);
        for (const FHitResult& Hit : InteractionHits)
        {
            AActor* HitActor = Hit.GetActor();
            if (!HitActor) continue;

            // If carrying parcel, exclude other parcels from being chosen (we want shelf preference); picking parcels only when hands free
            if (IsValid(CarriedParcel) && HitActor->IsA(AParcelActor::StaticClass()))
            {
                continue;
            }

            TArray<UObject*> InteractableObjects;
            UDynamicGameplayStatics::GetActorAndChildObjectsWithInterface(HitActor, UInteractableInterface::StaticClass(), InteractableObjects);
            if (InteractableObjects.Num() == 0)
            {
                continue;
            }

            float BestObjectScore = TNumericLimits<float>::Max();
            AActor* BestObjectActor = nullptr;
            for (UObject* InteractableObj : InteractableObjects)
            {
                if (!InteractableObj)
                {
                    continue;
                }               

                FVector TargetLocation = HitActor->GetActorLocation();
                AActor* CandidateActor = HitActor;
                if (USceneComponent* InteractableComponent = Cast<USceneComponent>(InteractableObj))
                {
                    TargetLocation = InteractableComponent->GetComponentLocation();
                }
                else if (AActor* InteractableActor = Cast<AActor>(InteractableObj))
                {
                    if(!CanInteractWith(InteractableActor))
                    {
                        continue;
                    }
                    TargetLocation = InteractableActor->GetActorLocation();
                    CandidateActor = InteractableActor;
                }

                const FVector ToTarget = TargetLocation - Start;
                const float Distance = FMath::Max(ToTarget.Size(), 1.0f);
                const FVector ToDir = ToTarget / Distance;

                const float AngleCos = FVector::DotProduct(Direction.GetSafeNormal(), ToDir);
                const float AngleScore = 1.0f - AngleCos; // 0이면 정확히 정중앙
                const float DistanceScore = Distance / MaxDistance;
                const float FinalScore = (AngleScore * 0.7f) + (DistanceScore * 0.3f);

                if (FinalScore < BestObjectScore)
                {
                    BestObjectScore = FinalScore;
                    BestObjectActor = CandidateActor;
                }
            }

            if (BestObjectScore == TNumericLimits<float>::Max() || !BestObjectActor)
            {
                continue;
            }

            const bool bIsPrimary = BestObjectActor->ActorHasTag(FName("Primary"));

            if (bIsPrimary)
            {
                if (BestObjectScore < BestPrimaryScore)
                {
                    BestPrimaryScore = BestObjectScore;
                    PrimaryTaggedTarget = BestObjectActor;
                }
            }
            else if (BestObjectScore < BestScore)
            {
                BestScore = BestObjectScore;
                NewTarget = BestObjectActor;
            }
        }
    }

    if (PrimaryTaggedTarget)
    {
        NewTarget = PrimaryTaggedTarget;
    }

    TArray<AActor*> ValidOverlappingActors;
    ValidOverlappingActors.Reserve(OverlappingActors.Num());
    for (const TWeakObjectPtr<AActor>& ActorPtr : OverlappingActors)
    {
        AActor* OverlapActor = ActorPtr.Get();
        if (!OverlapActor || !IsActorInteractable(OverlapActor))
        {
            continue;
        }
        if (IsValid(CarriedParcel) && OverlapActor->IsA(AParcelActor::StaticClass()))
        {
            continue;
        }
        ValidOverlappingActors.Add(OverlapActor);
    }

    // When carrying a parcel, shelf trace (ECC_GameTraceChannel4) can detect shelves
    // that the interaction channel (ECC_GameTraceChannel3) may miss.
    // Multi-trace + hysteresis prevents flickering when shelves are stacked.
    if (!NewTarget && BestShelfFromTrace)
    {
        NewTarget = BestShelfFromTrace;
    }

    if (!NewTarget && ValidOverlappingActors.Num() > 0)
    {
        NewTarget = SelectBestOverlapTarget(ValidOverlappingActors, Start, Direction);
    }

    PreviousTarget = CurrentTarget;

    if (!NewTarget)
    {
        ClearShelfPlacementPreview();
        if (CurrentTarget.IsValid())
        {
            SetParcelTargetedFlag(CurrentTarget.Get(), false);
            HideInteractionWidget();
            SetCustomDepth(CurrentTarget.Get(), false);
            OnTargetChanged.Broadcast(PreviousTarget.Get(), nullptr);
        }
        CurrentTarget = nullptr;
        return;
    }

    const bool bChanged = CurrentTarget.Get() != NewTarget;
    if (bChanged)
    {
        if (CurrentTarget.IsValid())
        {
            SetParcelTargetedFlag(CurrentTarget.Get(), false);
            SetCustomDepth(CurrentTarget.Get(), false);
        }
        CurrentTarget = NewTarget;
        SetParcelTargetedFlag(CurrentTarget.Get(), true);

        // CanInteract가 false면 UI/하이라이트 제거 후 종료
        if (!CanInteract())
        {
            HideInteractionWidget();
            SetCustomDepth(CurrentTarget.Get(), false);
            OnTargetChanged.Broadcast(PreviousTarget.Get(), CurrentTarget.Get());
            return;
        }

        if (CurrentTarget.IsValid())
        {
            UObject* InteractableObj = UDynamicGameplayStatics::GetActorOrComponentWithInterface(CurrentTarget.Get(), UInteractableInterface::StaticClass());
            bool bHandled = false;
            if (InteractableObj && InteractableObj->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
            {
                bHandled = IInteractableInterface::Execute_RequestShowInteractionUI(InteractableObj, OwnerCharacter);
            }
            if (CurrentTarget->ActorHasTag(FName("IgnoreInteractionUI")))
            {
                bHandled = true;
            }

            if (!bHandled) 
            {
                ShowInteractionWidget(CurrentTarget.Get());
                UpdateInteractionWidgetUI(CurrentTarget.Get(), ActiveInteractionWidgetComponent.IsValid() ? ActiveInteractionWidgetComponent->GetUserWidgetObject() : nullptr);
            }
            else 
            {
                HideInteractionWidget();
            }
            SetCustomDepth(CurrentTarget.Get(), true);
        }
        OnTargetChanged.Broadcast(PreviousTarget.Get(), CurrentTarget.Get());
    }
    else if (CurrentTarget.IsValid())
    {
        if (!CanInteract())
        {
            HideInteractionWidget();
            SetCustomDepth(CurrentTarget.Get(), false);
            return;
        }

        bool bHandled = false;
        UObject* InteractableObj = UDynamicGameplayStatics::GetActorOrComponentWithInterface(CurrentTarget.Get(), UInteractableInterface::StaticClass());
        if (InteractableObj && InteractableObj->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
        {
            bHandled = IInteractableInterface::Execute_RequestShowInteractionUI(InteractableObj, OwnerCharacter);
        }

        if (!bHandled)
        {
            ShowInteractionWidget(CurrentTarget.Get());
            UpdateInteractionWidgetUI(CurrentTarget.Get(), ActiveInteractionWidget);
        }
        else
        {
            HideInteractionWidget();
        }
    }

    if (AShelfActor* ShelfTarget = Cast<AShelfActor>(CurrentTarget.Get()))
    {
        if (ShelfTarget->SupportsFreePlacement())
        {
            UpdateShelfPlacementPreview(ShelfTarget);
            ClearShelfSlotFocus(ShelfTarget);
        }
        else
        {
            UpdateShelfSlotFocus(ShelfTarget);
            ClearShelfPlacementPreview(ShelfTarget);
        }
    }
    else
    {
        ClearShelfSlotFocus();
        ClearShelfPlacementPreview();
    }
}

AActor* UInteractionComponent::SelectBestOverlapTarget(const TArray<AActor*>& InActors, const FVector& ViewOrigin, const FVector& ViewDirection) const
{
    if (InActors.Num() == 0)
    {
        return nullptr;
    }

    AActor* BestTarget = nullptr;
    float BestScore = TNumericLimits<float>::Max();
    const FVector SafeViewDirection = ViewDirection.GetSafeNormal();
    const float MaxDistance = FMath::Max(InteractionDistance, 1.0f);

    for (AActor* Actor : InActors)
    {
        if (!Actor)
        {
            continue;
        }

        const FVector ToActor = Actor->GetActorLocation() - ViewOrigin;
        const float Distance = ToActor.Size();
        if (Distance > InteractionDistance)
        {
            continue;
        }

        const FVector ToActorDir = Distance > KINDA_SMALL_NUMBER ? ToActor / Distance : SafeViewDirection;
        const float AngleCos = FVector::DotProduct(SafeViewDirection, ToActorDir);
        if (AngleCos <= 0.25f)
        {
            continue;
        }

        const float AngleScore = 1.0f - AngleCos;
        const float DistanceScore = Distance / MaxDistance;
        const float FinalScore = (AngleScore * 0.7f) + (DistanceScore * 0.3f);
        if (FinalScore < BestScore)
        {
            BestScore = FinalScore;
            BestTarget = Actor;
        }
    }

    return BestTarget ? BestTarget : InActors[0];
}

void UInteractionComponent::AddOverlappingActor(AActor* Actor)
{
    if (!Actor || !IsValid(Actor))
    {
        return;
    }

    if (OverlappingActors.Contains(Actor))
    {
        return;
    }

    OverlappingActors.Add(Actor);

    if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()); OwnerCharacter && OwnerCharacter->IsLocallyControlled())
    {
        UpdateTarget();
    }
}

void UInteractionComponent::RemoveOverlappingActor(AActor* Actor)
{
    OverlappingActors.RemoveAllSwap([Actor](const TWeakObjectPtr<AActor>& ActorPtr)
    {
        return !ActorPtr.IsValid() || ActorPtr.Get() == Actor;
    });

    if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()); OwnerCharacter && OwnerCharacter->IsLocallyControlled())
    {
        UpdateTarget();
    }
}

TArray<AActor*> UInteractionComponent::GetOverlappingActors() const
{
    TArray<AActor*> Result;
    Result.Reserve(OverlappingActors.Num());
    for (const TWeakObjectPtr<AActor>& ActorPtr : OverlappingActors)
    {
        if (AActor* Actor = ActorPtr.Get())
        {
            Result.Add(Actor);
        }
    }
    return Result;
}

void UInteractionComponent::Interact()
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()); if (!OwnerCharacter) return;

    // 웅크린 상태에서는 새로 줍기 금지 (기존 소지품 드랍은 허용)
    if (!IsValid(CarriedParcel) && OwnerCharacter->bIsCrouched)
    {
        if (AActor* TargetActor = CurrentTarget.Get())
        {
            if (TargetActor->IsA(AParcelActor::StaticClass()))
            {
                return;
            }
        }
    }

    // If carrying a parcel, try placing onto current shelf target using preview transform when available.
    if (IsValid(CarriedParcel))
    {
        AParcelActor* Parcel = CarriedParcel;
        if (CurrentTarget.IsValid())
        {
            if (AShelfActor* Shelf = Cast<AShelfActor>(CurrentTarget.Get()))
            {
                if (Shelf->SupportsFreePlacement())
                {
                    // Ensure preview is fresh and valid at interaction time
                    UpdateShelfPlacementPreview(Shelf);
                    if (!bHasPlacementPreview || !CachedPlacementPreview.bIsPlaceable)
                    {
                        return; // no valid placement preview
                    }

                    if (OwnerCharacter->HasAuthority())
                    {
                        if (Shelf->TryPlaceParcelWithTransform(Parcel, CachedPlacementPreview.WorldTransform))
                        {
                            ClearShelfPlacementPreview(Shelf);
                            SetCarriedParcel(nullptr);
                            OnInteractSuccess.Broadcast(Shelf);
                        }
                    }
                    else
                    {
                        Server_Interact(Shelf, INDEX_NONE, CachedPlacementPreview.WorldTransform);
                        ClearShelfPlacementPreview(Shelf);
                    }
                    return;
                }
                else
                {
                    // Slot mode path no longer used; fall back to free placement behavior not available
                    return;
                }
            }

            if (CanInteract())
            {
                AActor* TargetActor = CurrentTarget.Get();
                UObject* InteractableObj = UDynamicGameplayStatics::GetActorOrComponentWithInterface(TargetActor, UInteractableInterface::StaticClass());
                if (InteractableObj)
                {
                    if (OwnerCharacter->HasAuthority())
                    {
                        PerformInteract(InteractableObj, OwnerCharacter);
                        OnInteractSuccess.Broadcast(TargetActor);
                    }
                    else
                    {
                        Server_Interact(TargetActor, INDEX_NONE, FTransform::Identity);
                    }
                    return;
                }
            }
        }
        // If not targeting shelf or interactable, drop parcel with impulse
        Parcel->RequestDrop(OwnerCharacter->GetActorForwardVector() * DropImpulse);
        if (OwnerCharacter->HasAuthority()) SetCarriedParcel(nullptr);
        return;
    }

    // Not carrying parcel: default interaction with target object via interface
    if (!CanInteract()) return;
    AActor* TargetActor = CurrentTarget.Get(); if (!TargetActor) return;
    UObject* InteractableObj = UDynamicGameplayStatics::GetActorOrComponentWithInterface(TargetActor, UInteractableInterface::StaticClass()); if (!InteractableObj) return;

    // Check if target requires credit unlock
    UCreditUnlockComponent* CreditUnlockComp = TargetActor->FindComponentByClass<UCreditUnlockComponent>();
    if (CreditUnlockComp && !CreditUnlockComp->IsUnlocked())
    {
        // Try to unlock first
        if (OwnerCharacter->HasAuthority())
        {
            if (CreditUnlockComp->RequestUnlock(OwnerCharacter))
            {
                // Unlock successful, now perform interaction
                PerformInteract(InteractableObj, OwnerCharacter);
                OnInteractSuccess.Broadcast(TargetActor);
                // Update UI after unlock
                UpdateInteractionWidgetCreditInfo(TargetActor);
            }
            else
            {
                // Unlock failed (not enough credits)
                UE_LOG(LogTemp, Warning, TEXT("[InteractionComponent] Failed to unlock - insufficient credits"));
            }
        }
        else
        {
            // Client: RequestUnlock uses GetAuthGameMode (server-only). Send to server; server handles unlock.
            Server_Interact(TargetActor, INDEX_NONE, FTransform::Identity);
        }
        return;
    }

    if (OwnerCharacter->HasAuthority())
    {
        PerformInteract(InteractableObj, OwnerCharacter);
        if (AParcelActor* Parcel = Cast<AParcelActor>(TargetActor))
        {
            if (Parcel->IsAttached()) SetCarriedParcel(Parcel);
        }
        OnInteractSuccess.Broadcast(TargetActor);
    }
    else { Server_Interact(TargetActor, INDEX_NONE, FTransform::Identity); }
}

void UInteractionComponent::InventoryInteract()
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()); if (!OwnerCharacter) return;
    ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OwnerCharacter); if (!BlasterCharacter || !BlasterCharacter->GetPlayerInventoryComponent()) return;
    UPlayerInventoryComponent* Inventory = BlasterCharacter->GetPlayerInventoryComponent();
    // If carrying, put into inventory (auto slot)
    if (IsValid(CarriedParcel))
    {
        if (CarriedParcel->IsPackaged()) { UE_LOG(LogTemp, Warning, TEXT("[InteractionComponent] Cannot put packaged parcel into inventory")); return; }
        if (CarriedParcel->IsItem())
        {
            if (Inventory->PutCarriedParcelIntoInventory(-1))
            {
                SetCarriedParcel(nullptr);
                UE_LOG(LogTemp, Log, TEXT("[InteractionComponent] Put parcel into inventory"));
            }
        }
        return;
    }

    // Not carrying: equip first inventory item (if any)
    AParcelActor* Equipped = Inventory->EquipItemFromInventory(0);
    if (Equipped)
    {
        SetCarriedParcel(Equipped);
        UE_LOG(LogTemp, Log, TEXT("[InteractionComponent] Took parcel from inventory (auto slot 0)"));
    }
}

void UInteractionComponent::Server_Interact_Implementation(AActor* Target, int32 TargetSlotIndex, const FTransform& DesiredTransform)
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()); if (!OwnerCharacter) return;
    if (IsValid(CarriedParcel))
    {
        if (AShelfActor* Shelf = Cast<AShelfActor>(Target))
        {
            if (Shelf->SupportsFreePlacement())
            {
                if (Shelf->TryPlaceParcelWithTransform(CarriedParcel, DesiredTransform)) { SetCarriedParcel(nullptr); }
            }
            else
            {
                if (Shelf->TryPlaceParcelAtSlot(CarriedParcel, TargetSlotIndex)) { SetCarriedParcel(nullptr); }
            }
            return;
        }
    }
    UObject* InteractableObj = UDynamicGameplayStatics::GetActorOrComponentWithInterface(Target, UInteractableInterface::StaticClass()); if (!InteractableObj) return;

    // Credit unlock: RequestUnlock must run on server (GetAuthGameMode). Client sends RPC; we handle here.
    if (UCreditUnlockComponent* CreditUnlockComp = Target->FindComponentByClass<UCreditUnlockComponent>())
    {
        if (!CreditUnlockComp->IsUnlocked())
        {
            if (!CreditUnlockComp->RequestUnlock(OwnerCharacter))
            {
                return; // Not enough credits
            }
        }
    }
    PerformInteract(InteractableObj, OwnerCharacter);
}

bool UInteractionComponent::CanInteract() const
{
    if (!CurrentTarget.IsValid()) return false;
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()); if (!OwnerCharacter) return false;
    UObject* Obj = UDynamicGameplayStatics::GetActorOrComponentWithInterface(CurrentTarget.Get(), UInteractableInterface::StaticClass()); if (!Obj) return false;
    return IInteractableInterface::Execute_CanInteract(Obj, OwnerCharacter);
}

bool UInteractionComponent::CanInteractWith(AActor* Target) const
{
	if (!Target) return false;
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()); if (!OwnerCharacter) return false;
	UObject* Obj = UDynamicGameplayStatics::GetActorOrComponentWithInterface(Target, UInteractableInterface::StaticClass()); if (!Obj) return false;
	return IInteractableInterface::Execute_CanInteract(Obj, OwnerCharacter);
}

UObject* UInteractionComponent::GetCurrentInteractableObject() const
{
    return CurrentTarget.IsValid() ? UDynamicGameplayStatics::GetActorOrComponentWithInterface(CurrentTarget.Get(), UInteractableInterface::StaticClass()) : nullptr;
}

void UInteractionComponent::PerformInteract(UObject* InteractableObject, ACharacter* OwnerCharacter)
{
    if (!InteractableObject || !OwnerCharacter) return;
    if (!InteractableObject->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass())) return;
    IInteractableInterface::Execute_OnInteract(InteractableObject, OwnerCharacter);
}

void UInteractionComponent::SetCustomDepth(AActor* TargetActor, bool bEnable)
{
    if (!TargetActor) return;
    TArray<UActorComponent*> PrimitiveComponents;
    TargetActor->GetComponents(UPrimitiveComponent::StaticClass(), PrimitiveComponents);
    const int StencilValue = bEnable ? CustomDepthStencilValue : 0;
    for (UActorComponent* Component : PrimitiveComponents)
    {
        if (Component->ComponentHasTag(FName("IgnoreDepth"))) continue;
        if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Component))
        {
            Prim->SetRenderCustomDepth(bEnable);
            Prim->SetCustomDepthStencilValue(StencilValue);
        }
    }
}

void UInteractionComponent::SetCarriedParcel(AParcelActor* NewParcel)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (CarriedParcel == NewParcel) return;
    AParcelActor* Last = CarriedParcel;
    CarriedParcel = NewParcel;
    HandleCarriedParcelChanged(Last);
}

void UInteractionComponent::OnRep_CarriedParcel(AParcelActor* LastParcel)
{
    HandleCarriedParcelChanged(LastParcel);
}

void UInteractionComponent::HandleCarriedParcelChanged(AParcelActor* LastParcel)
{
    UE_LOG(LogTemp, Log, TEXT("[InteractionComponent] Carried parcel changed %s -> %s"),
        LastParcel ? *LastParcel->GetName() : TEXT("None"), CarriedParcel ? *CarriedParcel->GetName() : TEXT("None"));

    if (!CarriedParcel)
    {
        ClearShelfPlacementPreview();

        // Movement penalty is now handled by BlasterCharacter::UpdateMovementSpeedFromCarriedParcel()
        // No need to clear here as it's updated every tick
    }
    else
    {
        // Movement penalty is now handled by BlasterCharacter::UpdateMovementSpeedFromCarriedParcel()
        // No need to apply here as it's updated every tick
    }
}

void UInteractionComponent::ApplyParcelMovementPenalty(ACharacter* OwnerCharacter, AParcelActor* Parcel)
{
    if (!OwnerCharacter || !Parcel) return;

    if (UCharacterMovementComponent* MoveComp = OwnerCharacter->FindComponentByClass<UCharacterMovementComponent>())
    {
        // Cache original speed once
        if (!bMovementPenaltyApplied)
        {
            CachedOriginalMaxWalkSpeed = MoveComp->MaxWalkSpeed;
        }

        float SpeedMultiplier = 1.0f;
        if (UParcelStateComponent* StateComp = Parcel->GetParcelStateComponent())
        {
            SpeedMultiplier = StateComp->GetEffectiveMovementSpeedMultiplier();
        }

        MoveComp->MaxWalkSpeed = CachedOriginalMaxWalkSpeed * SpeedMultiplier;
        bMovementPenaltyApplied = true;
    }
}

void UInteractionComponent::ClearParcelMovementPenalty(ACharacter* OwnerCharacter)
{
    if (!OwnerCharacter) return;
    if (!bMovementPenaltyApplied) return;

    if (UCharacterMovementComponent* MoveComp = OwnerCharacter->FindComponentByClass<UCharacterMovementComponent>())
    {
        if (CachedOriginalMaxWalkSpeed > 0.f)
        {
            MoveComp->MaxWalkSpeed = CachedOriginalMaxWalkSpeed;
        }
    }

    bMovementPenaltyApplied = false;
    CachedOriginalMaxWalkSpeed = -1.0f;
}

bool UInteractionComponent::IsActorInteractable(AActor* Actor) const
{
    if (!Actor) return false;
    if (AllowedActorClasses.Num() > 0)
    {
        bool bAllowed = false;
        for (TSubclassOf<AActor> Allowed : AllowedActorClasses)
        {
            if (Actor->IsA(Allowed)) { bAllowed = true; break; }
        }
        if (!bAllowed) return false;
    }
    if (bRequireInteractableInterface)
    {
        if (!Actor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()) &&
            Actor->GetComponentsByInterface(UInteractableInterface::StaticClass()).Num() == 0)
        {
            return false;
        }
    }
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()); if (!OwnerCharacter) return false;
    UObject* Obj = UDynamicGameplayStatics::GetActorOrComponentWithInterface(Actor, UInteractableInterface::StaticClass());
    if (Obj)
    {
        return IInteractableInterface::Execute_CanInteract(Obj, OwnerCharacter);
    }
    return false;
}

void UInteractionComponent::UpdateShelfSlotFocus(AShelfActor* Shelf)
{
    if (!Shelf) return;
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()); if (!OwnerCharacter) return;
    if (!OwnerCharacter->IsLocallyControlled()) return;
    if (!FocusedShelf.IsValid() || FocusedShelf.Get() != Shelf)
    {
        ClearShelfSlotFocus(FocusedShelf.Get());
        FocusedShelf = Shelf;
    }
    FVector ViewLocation = OwnerCharacter->GetActorLocation();
    FVector ViewDirection = OwnerCharacter->GetActorForwardVector();
    if (UCameraComponent* Camera = OwnerCharacter->FindComponentByClass<UCameraComponent>())
    {
        ViewLocation = Camera->GetComponentLocation();
        ViewDirection = Camera->GetForwardVector();
    }
    else
    {
        ViewLocation += OwnerCharacter->GetActorRotation().RotateVector(TraceStartOffset);
    }
    const int32 CandidateSlot = Shelf->FindBestSlotForView(ViewLocation, ViewDirection, InteractionDistance);
    Shelf->SetFocusedSlot(CandidateSlot);
    FocusedSlotIndex = CandidateSlot;
}

void UInteractionComponent::ClearShelfSlotFocus(AShelfActor* ShelfToClear)
{
    if (ShelfToClear == nullptr && FocusedShelf.IsValid()) { ShelfToClear = FocusedShelf.Get(); }
    if (ShelfToClear) { ShelfToClear->SetFocusedSlot(INDEX_NONE); }
    FocusedShelf = nullptr; FocusedSlotIndex = INDEX_NONE;
}

void UInteractionComponent::UpdateShelfPlacementPreview(AShelfActor* Shelf)
{
    if (!Shelf)
    {
        ClearShelfPlacementPreview();
        return;
    }

    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled())
    {
        return;
    }

    if (!IsValid(CarriedParcel))
    {
        ClearShelfPlacementPreview(Shelf);
        return;
    }

    FVector ViewLocation = OwnerCharacter->GetActorLocation();
    FVector ViewDirection = OwnerCharacter->GetActorForwardVector();
    if (UCameraComponent* Camera = OwnerCharacter->FindComponentByClass<UCameraComponent>())
    {
        ViewLocation = Camera->GetComponentLocation();
        ViewDirection = Camera->GetForwardVector();
    }
    else
    {
        ViewLocation += OwnerCharacter->GetActorRotation().RotateVector(TraceStartOffset);
    }

    FShelfPlacementPreview Preview;
    if (Shelf->ComputePlacementPreview(CarriedParcel, ViewLocation, ViewDirection, InteractionDistance, Preview))
    {
        // Hide carried parcel mesh to avoid view obstruction
        UStaticMeshComponent* ParcelMesh = CarriedParcel->GetParcelMesh();
        if (Preview.bIsPlaceable && ParcelMesh)
        {
            ParcelMesh->SetHiddenInGame(true);
            ParcelMesh->SetVisibility(false);
        }
        else
        {
            ClearShelfPlacementPreview();
        }
        
        // Ensure preview mesh clones carried parcel if needed
        Shelf->EnsurePreviewMeshForParcel(CarriedParcel);
        Shelf->UpdatePlacementPreviewVisual(Preview);
        CachedPlacementPreview = Preview;
        bHasPlacementPreview = true;
        PreviewShelf = Shelf;
    }
    else
    {
        ClearShelfPlacementPreview(Shelf);
    }
}

void UInteractionComponent::ClearShelfPlacementPreview(AShelfActor* ShelfToClear)
{
    AShelfActor* TargetShelf = ShelfToClear;
    if (TargetShelf == nullptr && PreviewShelf.IsValid())
    {
        TargetShelf = PreviewShelf.Get();
    }

    if (TargetShelf)
    {
        TargetShelf->ClearPlacementPreviewVisual();
    }

    // Restore carried parcel visibility
    if (IsValid(CarriedParcel))
    {
        if (UStaticMeshComponent* ParcelMesh = CarriedParcel->GetParcelMesh())
        {
            ParcelMesh->SetHiddenInGame(false);
            ParcelMesh->SetVisibility(true);
        }
    }

    PreviewShelf = nullptr;
    CachedPlacementPreview.Reset();
    bHasPlacementPreview = false;
}

void UInteractionComponent::Action() {}
void UInteractionComponent::Server_Action_Implementation(AActor* Target) {}

void UInteractionComponent::UpdateInteractionWidgetUI(AActor* TargetActor, UUserWidget* WidgetInstance)
{
    if (!TargetActor)
    {
        return;
    }

    UUserWidget* WidgetToUse = WidgetInstance;
    if (!WidgetToUse && ActiveInteractionWidget)
    {
        WidgetToUse = ActiveInteractionWidget;
    }
    if (!WidgetToUse && ActiveInteractionWidgetComponent.IsValid())
    {
        WidgetToUse = ActiveInteractionWidgetComponent->GetUserWidgetObject();
    }
    if (!WidgetToUse)
    {
        return;
    }

    FInteractionUIData UIData;
    if (!GatherInteractionUIData(TargetActor, UIData))
    {
        return;
    }

    const FText InputKeyText = BuildInputPromptText(UIData.ActionText);

    if (UInteractionPromptWidget* PromptWidget = Cast<UInteractionPromptWidget>(WidgetToUse))
    {
        PromptWidget->UpdateFromInteractionData(UIData, InputKeyText);
        return;
    }

    // Fallback: optional Blueprint event for arbitrary widgets
    static const FName UpdateFuncName(TEXT("OnInteractionUIDataUpdated"));
    if (UFunction* UpdateFunc = WidgetToUse->FindFunction(UpdateFuncName))
    {
        struct FUpdateParams
        {
            FInteractionUIData Data;
            FText InputKey;
        };

        FUpdateParams Params{ UIData, InputKeyText };
        WidgetToUse->ProcessEvent(UpdateFunc, &Params);
    }
    else
    {
        // 마지막 수단: 크레딧 정보만 전달
        InvokeWidgetCreditUpdate(WidgetToUse, UIData.bRequiresUnlock, UIData.UnlockCost, UIData.LockedMessage, UIData.UnlockedMessage, UIData.CurrentCredits);
    }
}

void UInteractionComponent::UpdateInteractionWidgetScreenPosition(AActor* TargetActor)
{
    if (!TargetActor || !ActiveInteractionWidget)
    {
        return;
    }

    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    APlayerController* PlayerController = OwnerCharacter ? Cast<APlayerController>(OwnerCharacter->GetController()) : nullptr;
    if (!OwnerCharacter || !PlayerController || !PlayerController->IsLocalController())
    {
        return;
    }

    FVector WorldLocation = GetInteractionWidgetWorldLocation(TargetActor);
    FVector2D ScreenPosition = FVector2D::ZeroVector;
    bool bProjected = PlayerController->ProjectWorldLocationToScreen(WorldLocation, ScreenPosition, true);

    if (!bProjected && bMoveOffscreenWidgetToCenter)
    {
        if (APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
        {
            WorldLocation = CameraManager->GetCameraLocation()
                + CameraManager->GetActorForwardVector() * OffscreenFallbackDistance
                + OffscreenFallbackOffset;
            bProjected = PlayerController->ProjectWorldLocationToScreen(WorldLocation, ScreenPosition, true);
        }
    }

    if (!bProjected)
    {
        return;
    }

    if (bScreenClampWidget)
    {
        const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
        if (ViewportSize.X > 0.f && ViewportSize.Y > 0.f)
        {
            ScreenPosition.X = FMath::Clamp(ScreenPosition.X, ScreenEdgePadding.X, ViewportSize.X - ScreenEdgePadding.X);
            ScreenPosition.Y = FMath::Clamp(ScreenPosition.Y, ScreenEdgePadding.Y, ViewportSize.Y - ScreenEdgePadding.Y);
        }
    }

    if (UInteractionPromptWidget* PromptWidget = Cast<UInteractionPromptWidget>(ActiveInteractionWidget))
    {
        PromptWidget->SetPromptPosition(ScreenPosition);
        PromptWidget->SetPromptVisibility(true);
        return;
    }

    ActiveInteractionWidget->SetAlignmentInViewport(FVector2D(0.5f, 1.0f));
    ActiveInteractionWidget->SetPositionInViewport(ScreenPosition, false);
    ActiveInteractionWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
}

bool UInteractionComponent::GatherInteractionUIData(AActor* TargetActor, FInteractionUIData& OutData) const
{
    if (!TargetActor)
    {
        return false;
    }

    UObject* InteractableObj = UDynamicGameplayStatics::GetActorOrComponentWithInterface(TargetActor, UInteractableInterface::StaticClass());
    if (InteractableObj && InteractableObj->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
    {
        IInteractableInterface::Execute_GetInteractionUIData(InteractableObj, OutData);
        if (OutData.ActionText.IsEmpty())
        {
            OutData.ActionText = IInteractableInterface::Execute_GetInteractText(InteractableObj);
        }
        if (OutData.InteractionType == EInteractionType::None)
        {
            OutData.InteractionType = EInteractionType::Default;
        }
    }

    // 잠금 정보 보완
    UCreditUnlockComponent* CreditUnlockComp = TargetActor->FindComponentByClass<UCreditUnlockComponent>();
    if (CreditUnlockComp && !CreditUnlockComp->IsUnlocked())
    {
        OutData.bRequiresUnlock = true;
        OutData.UnlockCost = CreditUnlockComp->UnlockCost;
        OutData.LockedMessage = CreditUnlockComp->LockedMessage;
        OutData.UnlockedMessage = CreditUnlockComp->UnlockedMessage;
        if (OutData.InteractionType == EInteractionType::None)
        {
            OutData.InteractionType = EInteractionType::Unlock;
        }
    }
    else if (InteractableObj && InteractableObj->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
    {
        bool bRequiresUnlock = false;
        int32 UnlockCost = 0;
        FText LockedMessage;
        FText UnlockedMessage;
        IInteractableInterface::Execute_GetCreditUnlockInfo(InteractableObj, bRequiresUnlock, UnlockCost, LockedMessage, UnlockedMessage);
        if (bRequiresUnlock)
        {
            OutData.bRequiresUnlock = true;
            OutData.UnlockCost = UnlockCost;
            OutData.LockedMessage = LockedMessage;
            OutData.UnlockedMessage = UnlockedMessage;
            if (OutData.InteractionType == EInteractionType::None)
            {
                OutData.InteractionType = EInteractionType::Unlock;
            }
        }
    }

    // 팀 크레딧
    if (UWorld* World = GetWorld())
    {
        if (APickpackerGameState* GameState = World->GetGameState<APickpackerGameState>())
        {
            OutData.CurrentCredits = GameState->GetTeamCredits();
        }
    }

	// Gate 요구 조건 보완 표시
	if (AGateActor* Gate = Cast<AGateActor>(TargetActor))
	{
		FGameplayTagContainer CombinedUseActions = OutData.RequiredUseActions;
		for (const FGateCondition& Condition : Gate->GetRequiredConditions())
		{
			CombinedUseActions.AppendTags(Condition.RequiredUseActions);
		}
		OutData.RequiredUseActions = CombinedUseActions;

		if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
		{
			const bool bMeets = Gate->DoesPlayerMeetConditions(OwnerCharacter);
			OutData.bPlayerHasRequiredItem = bMeets;
			if (!bMeets && OutData.MissingRequirementsText.IsEmpty())
			{
				OutData.MissingRequirementsText = FText::FromString(TEXT("필요한 아이템/액션이 없습니다"));
			}
		}

		if (OutData.InteractionType == EInteractionType::None)
		{
			OutData.InteractionType = EInteractionType::Use;
		}
	}

    // 기본 텍스트 확보
    if (OutData.ActionText.IsEmpty() && InteractableObj && InteractableObj->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
    {
        OutData.ActionText = IInteractableInterface::Execute_GetInteractText(InteractableObj);
    }

    return true;
}

FVector UInteractionComponent::GetInteractionWidgetWorldLocation(AActor* TargetActor) const
{
    if (!TargetActor)
    {
        return FVector::ZeroVector;
    }

    if (UWidgetComponent* AnchorComponent = FindInteractionWidgetAnchor(TargetActor))
    {
        return AnchorComponent->GetComponentLocation() + WidgetWorldOffset;
    }

    FVector Origin = FVector::ZeroVector;
    FVector Extent = FVector::ZeroVector;
    TargetActor->GetActorBounds(true, Origin, Extent);

    float AnchorHeight = Extent.Z * WidgetAnchorHeightFactor;
    if (bClampWidgetAnchorHeight)
    {
        AnchorHeight = FMath::Min(AnchorHeight, MaxWidgetAnchorHeight);
    }

    return Origin + FVector(0.f, 0.f, AnchorHeight) + WidgetWorldOffset;
}

FText UInteractionComponent::BuildInputPromptText(const FText& /*ActionText*/) const
{
    if (InteractInputAction)
    {
        if (FText InputKey = GetPrimaryKeyForInputAction(InteractInputAction); !InputKey.IsEmpty())
        {
            return InputKey;
        }
    }

    return GetPrimaryKeyForAction(InteractActionName);
}

FText UInteractionComponent::GetPrimaryKeyForAction(const FName& ActionName) const
{
    const UInputSettings* InputSettings = UInputSettings::GetInputSettings();
    if (!InputSettings)
    {
        return FText();
    }

    TArray<FInputActionKeyMapping> Mappings;
    InputSettings->GetActionMappingByName(ActionName, Mappings);
    if (Mappings.Num() > 0)
    {
        return Mappings[0].Key.GetDisplayName(false);
    }

    return FText();
}

FText UInteractionComponent::GetPrimaryKeyForInputAction(const UInputAction* InputAction) const
{
    if (!InputAction)
    {
        return FText();
    }

    const UInputSettings* InputSettings = UInputSettings::GetInputSettings();
    if (!InputSettings)
    {
        return FText();
    }

    TArray<FInputActionKeyMapping> Mappings;
    InputSettings->GetActionMappingByName(InteractActionName, Mappings);
    if (Mappings.Num() > 0)
    {
        return Mappings[0].Key.GetDisplayName(false);
    }

    return FText();
}
