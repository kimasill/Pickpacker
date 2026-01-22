// Fill out your copyright notice in the Description page of Project Settings.

#include "InteractionComponent.h"
#include "Blaster/Interfaces/InteractableInterface.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/Shelf/ShelfActor.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/Components/PlayerInventoryComponent.h"
#include "Blaster/Components/CreditUnlockComponent.h"
#include "Blaster/Interfaces/GameplayActionInterface.h"
#include "Blaster/Library/DynamicGameplayStatics.h"
#include "Blaster/Components/ParcelStateComponent.h"
#include "Blaster/GameState/PickpackerGameState.h"
#include "Blaster/Interaction/InteractionUIData.h"
#include "Blaster/Gate/GateActor.h"
#include "Blaster/UI/InteractionPromptWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PrimitiveComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/InputSettings.h"
#include "Engine/LocalPlayer.h"

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
    UpdateTarget();
}

void UInteractionComponent::ShowInteractionWidget(AActor* TargetActor)
{
    if (!InteractionWidget || !TargetActor)
    {
        HideInteractionWidget();
        return;
    }

    // Destroy previous widget component if we switched targets.
    if (ActiveInteractionWidgetComponent.IsValid() && ActiveInteractionWidgetComponent->GetOwner() != TargetActor)
    {
        HideInteractionWidget();
    }

    UWidgetComponent* WidgetComp = ActiveInteractionWidgetComponent.Get();
    if (!WidgetComp)
    {
        WidgetComp = NewObject<UWidgetComponent>(TargetActor, TEXT("InteractionWidgetComponent"));
        if (!WidgetComp)
        {
            return;
        }

        // Screen space keeps the widget camera-facing without manual rotation.
        WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
        WidgetComp->SetDrawAtDesiredSize(true);
        WidgetComp->SetTwoSided(true);
        WidgetComp->SetWidgetClass(InteractionWidget);

        UWidgetComponent* AnchorComponent = FindInteractionWidgetAnchor(TargetActor);
        if (AnchorComponent)
        {
            WidgetComp->AttachToComponent(AnchorComponent, FAttachmentTransformRules::KeepRelativeTransform);
            WidgetComp->SetRelativeTransform(FTransform::Identity);
        }
        else if (USceneComponent* Root = TargetActor->GetRootComponent())
        {
            WidgetComp->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
        }
        WidgetComp->RegisterComponent();
    }
    else if (WidgetComp->GetWidgetClass() != InteractionWidget)
    {
        WidgetComp->SetWidgetClass(InteractionWidget);
    }

    // Anchor above the actor using bounds height + configurable offset when no explicit anchor component.
    if (!WidgetComp->GetAttachParent() || !WidgetComp->GetAttachParent()->IsA<UWidgetComponent>())
    {
        FVector Origin, Extent;
        TargetActor->GetActorBounds(true, Origin, Extent);
        float AnchorHeight = Extent.Z * WidgetAnchorHeightFactor;
        if (bClampWidgetAnchorHeight)
        {
            AnchorHeight = FMath::Min(AnchorHeight, MaxWidgetAnchorHeight);
        }
        WidgetComp->SetRelativeLocation(FVector(0.f, 0.f, AnchorHeight) + WidgetWorldOffset);
    }

    WidgetComp->SetVisibility(true);
    WidgetComp->SetHiddenInGame(false);

    ActiveInteractionWidgetComponent = WidgetComp;

    // Update UI on the current widget instance (only meaningful for prompt-derived widgets).
    UpdateInteractionWidgetUI(TargetActor, WidgetComp->GetUserWidgetObject());
}

void UInteractionComponent::HideInteractionWidget()
{
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
    if (!ActiveInteractionWidgetComponent.IsValid())
    {
        return;
    }

    UUserWidget* WidgetToUse = GetWidgetFromComponent(ActiveInteractionWidgetComponent);
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

    // Perform primary multi trace (interaction channel)
    TArray<FHitResult> InteractionHits;
    bool bInteractionTrace = false;

    // 우선 라인 트레이스
    bInteractionTrace = GetWorld() && GetWorld()->LineTraceMultiByChannel(InteractionHits, Start, End, InteractionChannel, InteractionParams);

    // 라인 트레이스에 없으면/보완용으로 좁은 캡슐 스윕 추가
    if (GetWorld())
    {
        TArray<FHitResult> SweepHits;
        const FCollisionShape Capsule = FCollisionShape::MakeCapsule(TargetingRadius, TargetingRadius);
        if (GetWorld()->SweepMultiByChannel(SweepHits, Start, End, FQuat::Identity, InteractionChannel, Capsule, InteractionParams))
        {
            InteractionHits.Append(SweepHits);
        }
    }
    FHitResult ShelfHit;
    bool bShelfHitValid = false;
    if (IsValid(CarriedParcel))
    {
        FCollisionQueryParams ShelfParams = InteractionParams; // reuse ignores
        // We allow shelf trace to pass through items; do NOT ignore static world so shelf collision works
        bShelfHitValid = GetWorld() && GetWorld()->LineTraceSingleByChannel(ShelfHit, Start, End, ShelfTraceChannel, ShelfParams)
                         && ShelfHit.GetActor() && ShelfHit.GetActor()->IsA(AShelfActor::StaticClass());
    }

    // Debug draw (short lived)
    if (bDrawDebugTrace)
    {
        const FColor LineColor = bInteractionTrace ? FColor::Green : FColor::Red;
        DrawDebugLine(GetWorld(), Start, End, LineColor, false, 0.05f, 0, 0.5f);
        for (const FHitResult& H : InteractionHits)
        {
            DrawDebugSphere(GetWorld(), H.ImpactPoint, 6.f, 12, FColor::Yellow, false, 0.05f);
        }
        if (bShelfHitValid)
        {
            DrawDebugSphere(GetWorld(), ShelfHit.ImpactPoint, 10.f, 16, FColor::Cyan, false, 0.1f);
        }
    }

    // Select best interactable from primary hits (가장 시선 중심에 가까운 것 우선)
    AActor* NewTarget = nullptr;
    float BestAngleScore = TNumericLimits<float>::Max(); // 작은 값이 더 중심

    if (InteractionHits.Num() > 0)
    {
        for (const FHitResult& Hit : InteractionHits)
        {
            AActor* HitActor = Hit.GetActor();
            if (!HitActor) continue;

            // If carrying parcel, exclude other parcels from being chosen (we want shelf preference); picking parcels only when hands free
            if (IsValid(CarriedParcel) && HitActor->IsA(AParcelActor::StaticClass()))
            {
                continue;
            }

            if (!UDynamicGameplayStatics::GetActorOrComponentWithInterface(HitActor, UInteractableInterface::StaticClass()))
            {
                continue;
            }

            const FVector ToHit = (Hit.ImpactPoint - Start).GetSafeNormal();
            const float AngleCos = FVector::DotProduct(Direction.GetSafeNormal(), ToHit);
            const float AngleScore = 1.0f - AngleCos; // 0이면 정확히 정중앙

            if (AngleScore < BestAngleScore)
            {
                BestAngleScore = AngleScore;
                NewTarget  = HitActor;
            }
        }
    }

    // If carrying a parcel and shelf trace succeeded, override target with shelf (always prioritize shelf when holding)
    if (bShelfHitValid)
    {
        NewTarget = ShelfHit.GetActor();
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
            if (ActiveInteractionWidgetComponent.IsValid())
            {
                UpdateInteractionWidgetUI(CurrentTarget.Get(), ActiveInteractionWidgetComponent->GetUserWidgetObject());
            }
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
        }
        // If not targeting shelf, drop parcel with impulse
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
            // Client: request unlock on server
            if (CreditUnlockComp->RequestUnlock(OwnerCharacter))
            {
                Server_Interact(TargetActor, INDEX_NONE, FTransform::Identity);
            }
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
    PerformInteract(InteractableObj, OwnerCharacter);
}

bool UInteractionComponent::CanInteract() const
{
    if (!CurrentTarget.IsValid()) return false;
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()); if (!OwnerCharacter) return false;
    UObject* Obj = UDynamicGameplayStatics::GetActorOrComponentWithInterface(CurrentTarget.Get(), UInteractableInterface::StaticClass()); if (!Obj) return false;
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