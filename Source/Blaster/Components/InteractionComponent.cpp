// Fill out your copyright notice in the Description page of Project Settings.

#include "InteractionComponent.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/Shelf/ShelfActor.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/Components/PlayerInventoryComponent.h"
#include "Blaster/Interfaces/GameplayActionInterface.h"
#include "Blaster/Library/DynamicGameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PrimitiveComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "Blueprint/UserWidget.h"

UInteractionComponent::UInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    InteractionDistance = 300.0f;
    TraceStartOffset = FVector(0.0f, 0.0f, 50.0f);
    TraceChannel = ECC_Visibility;
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
    if (!InteractionWidget) { HideInteractionWidget(); return; }
    if (!ActiveInteractionWidget)
    {
        if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
        {
            if (OwnerCharacter->IsLocallyControlled())
            {
                APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
                if (PC)
                {
                    ActiveInteractionWidget = CreateWidget<UUserWidget>(PC, InteractionWidget);
                }
                else if (UWorld* World = GetWorld())
                {
                    ActiveInteractionWidget = CreateWidget<UUserWidget>(World, InteractionWidget);
                }
                if (ActiveInteractionWidget)
                {
                    ActiveInteractionWidget->AddToViewport();
                }
            }
        }
    }
    else
    {
        ActiveInteractionWidget->SetVisibility(ESlateVisibility::Visible);
    }
}

void UInteractionComponent::HideInteractionWidget()
{
    if (ActiveInteractionWidget)
    {
        ActiveInteractionWidget->RemoveFromParent();
        ActiveInteractionWidget = nullptr;
    }
}

void UInteractionComponent::UpdateTarget()
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;

    UCameraComponent* Camera = OwnerCharacter->FindComponentByClass<UCameraComponent>();
    const FVector CameraLocation = Camera ? Camera->GetComponentLocation() : OwnerCharacter->GetActorLocation() + OwnerCharacter->GetActorRotation().RotateVector(TraceStartOffset);
    const FVector Direction = Camera ? Camera->GetForwardVector() : OwnerCharacter->GetActorForwardVector();
    const FVector Start = CameraLocation;
    const FVector End = CameraLocation + Direction * InteractionDistance;

    FHitResult Hit;
    TArray<AActor*> Ignored; Ignored.Add(OwnerCharacter); if (IsValid(CarriedParcel)) Ignored.Add(CarriedParcel);
    const ETraceTypeQuery Channel = UEngineTypes::ConvertToTraceType(TraceChannel);
    UKismetSystemLibrary::LineTraceSingle(GetWorld(), Start, End, Channel, false, Ignored, EDrawDebugTrace::None, Hit, false);

    AActor* NewTarget = nullptr;
    if (Hit.bBlockingHit)
    {
        if (UDynamicGameplayStatics::GetActorOrComponentWithInterface(Hit.GetActor(), UInteractableInterface::StaticClass()))
        {
            NewTarget = Hit.GetActor();
        }
    }

    PreviousTarget = CurrentTarget;
    if (NewTarget == nullptr)
    {
        ClearShelfSlotFocus();
        if (CurrentTarget.IsValid())
        {
            HideInteractionWidget();
            SetCustomDepth(CurrentTarget.Get(), false);
            OnTargetChanged.Broadcast(PreviousTarget.Get(), nullptr);
        }
        CurrentTarget = nullptr; return;
    }

    const bool bChanged = CurrentTarget.Get() != NewTarget;
    if (bChanged)
    {
        CurrentTarget = NewTarget;
        if (CurrentTarget.IsValid())
        {
            UObject* InteractableObj = UDynamicGameplayStatics::GetActorOrComponentWithInterface(CurrentTarget.Get(), UInteractableInterface::StaticClass());
            bool bHandled = false;
            if (InteractableObj && InteractableObj->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
            {
                bHandled = IInteractableInterface::Execute_RequestShowInteractionUI(InteractableObj, OwnerCharacter);
            }
            if (!bHandled)
            {
                ShowInteractionWidget(CurrentTarget.Get());
            }
            else
            {
                HideInteractionWidget();
            }
            SetCustomDepth(CurrentTarget.Get(), true);
        }
        OnTargetChanged.Broadcast(PreviousTarget.Get(), CurrentTarget.Get());
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

    if (IsValid(CarriedParcel))
    {
        AParcelActor* Parcel = CarriedParcel;
        if (CurrentTarget.IsValid())
        {
            if (AShelfActor* Shelf = Cast<AShelfActor>(CurrentTarget.Get()))
            {
                if (Shelf->SupportsFreePlacement())
                {
                    UpdateShelfPlacementPreview(Shelf);
                    if (!bHasPlacementPreview || !CachedPlacementPreview.bIsPlaceable)
                    {
                        return;
                    }

                    if (OwnerCharacter->HasAuthority())
                    {
                        if (Shelf->TryPlaceParcelWithTransform(Parcel, CachedPlacementPreview.WorldTransform))
                        {
                            SetCarriedParcel(nullptr);
                            ClearShelfPlacementPreview(Shelf);
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
                    const int32 TargetSlot = FocusedSlotIndex;
                    if (OwnerCharacter->HasAuthority())
                    {
                        if (Shelf->TryPlaceParcelAtSlot(Parcel, TargetSlot))
                        {
                            SetCarriedParcel(nullptr);
                            OnInteractSuccess.Broadcast(Shelf);
                            return;
                        }
                    }
                    else
                    {
                        Server_Interact(Shelf, TargetSlot, FTransform::Identity);
                        return;
                    }
                }
            }
        }
        if (Parcel)
        {
            Parcel->RequestDrop(OwnerCharacter->GetActorForwardVector() * DropImpulse);
            if (OwnerCharacter->HasAuthority()) SetCarriedParcel(nullptr);
        }
        return;
    }

    if (!CanInteract()) return;
    AActor* TargetActor = CurrentTarget.Get(); if (!TargetActor) return;
    UObject* InteractableObj = UDynamicGameplayStatics::GetActorOrComponentWithInterface(TargetActor, UInteractableInterface::StaticClass()); if (!InteractableObj) return;

    if (OwnerCharacter->HasAuthority())
    {
        if (PerformInteract(InteractableObj, OwnerCharacter))
        {
            if (AParcelActor* Parcel = Cast<AParcelActor>(TargetActor))
            {
                if (Parcel->IsAttached()) SetCarriedParcel(Parcel);
            }
            OnInteractSuccess.Broadcast(TargetActor);
        }
    }
    else { Server_Interact(TargetActor, INDEX_NONE, FTransform::Identity); }
}

void UInteractionComponent::InventoryInteract()
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()); if (!OwnerCharacter) return;
    ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OwnerCharacter); if (!BlasterCharacter || !BlasterCharacter->GetPlayerInventoryComponent()) return;
    UPlayerInventoryComponent* Inventory = BlasterCharacter->GetPlayerInventoryComponent();
    if (IsValid(CarriedParcel))
    {
        if (CarriedParcel->IsPackaged()) { UE_LOG(LogTemp, Warning, TEXT("[InteractionComponent] Cannot put packaged parcel into inventory")); return; }
        if (CarriedParcel->IsItem())
        {
            if (Inventory->CollectItem(CarriedParcel)) { SetCarriedParcel(nullptr); UE_LOG(LogTemp, Log, TEXT("[InteractionComponent] Put parcel into inventory")); }
        }
    }
    else
    {
        const TArray<AParcelActor*> Items = Inventory->GetCollectedItems();
        if (Items.Num() > 0)
        {
            AParcelActor* First = Items[0];
            if (First && Inventory->RemoveItem(First))
            {
                SetCarriedParcel(First);
                First->RequestAttach(OwnerCharacter, FName("CarrySocket"));
                UE_LOG(LogTemp, Log, TEXT("[InteractionComponent] Took parcel from inventory"));
            }
        }
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
    if (PerformInteract(InteractableObj, OwnerCharacter))
    {
        if (AParcelActor* Parcel = Cast<AParcelActor>(Target))
        {
            if (Parcel->IsAttached() && OwnerCharacter->HasAuthority()) SetCarriedParcel(Parcel);
        }
    }
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

bool UInteractionComponent::PerformInteract(UObject* InteractableObject, ACharacter* OwnerCharacter)
{
    if (!InteractableObject || !OwnerCharacter) return false;
    if (!InteractableObject->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass())) return false;
    return IInteractableInterface::Execute_OnInteract(InteractableObject, OwnerCharacter);
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
    }
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

    PreviewShelf = nullptr;
    CachedPlacementPreview.Reset();
    bHasPlacementPreview = false;
}

void UInteractionComponent::Action() {}
void UInteractionComponent::Server_Action_Implementation(AActor* Target) {}
