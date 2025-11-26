// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blaster/Shelf/ShelfPlacementTypes.h"
#include "InteractionComponent.generated.h"

class AActor;
class UPrimitiveComponent;
class AShelfActor;
class AParcelActor;
class ACharacter;
class UWidgetComponent;

UINTERFACE(MinimalAPI, BlueprintType)
class UInteractableInterface : public UInterface
{
    GENERATED_BODY()
};

class BLASTER_API IInteractableInterface
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
    bool OnInteract(class ACharacter* Interactor);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    bool CanInteract(class ACharacter* Interactor) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    FText GetInteractText() const;

    UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
    void StartHighlight();

    UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
    void EndHighlight();

    // 액터/컴포넌트가 자체 UI 처리 시 true 반환. false면 기본 부착형 위젯 표시.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction|UI")
    bool RequestShowInteractionUI(class ACharacter* Interactor);
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BLASTER_API UInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInteractionComponent();
    
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void Interact();

    UFUNCTION(Server, Reliable)
	void Server_Interact(AActor* Target, int32 TargetSlotIndex, const FTransform& DesiredTransform);

    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void Action();

	UFUNCTION(Server, Reliable)
	void Server_Action(AActor* Target);

    UFUNCTION(BlueprintCallable, Category = "Interaction")
	void InventoryInteract();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    bool CanInteract() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    UObject* GetCurrentInteractableObject() const;

    void SetCarriedParcel(class AParcelActor* NewParcel);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    class AParcelActor* GetCarriedParcel() const { return CarriedParcel; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    int32 GetFocusedSlotIndex() const { return FocusedSlotIndex; }

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetCustomDepth(AActor* TargetActor, bool bEnable);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
    bool IsCarryingParcel() const { return CarriedParcel != nullptr; }

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Settings")
    float InteractionDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Settings")
	float DropImpulse = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Settings")
    FVector TraceStartOffset = FVector(0.0f, 0.0f, 50.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Settings")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_GameTraceChannel3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Settings")
    TArray<TSubclassOf<AActor>> AllowedActorClasses;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Settings")
    bool bRequireInteractableInterface = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Settings")
    bool bUseLineTracePriority = true;

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bDrawDebugTrace = false;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractSuccess, AActor*, InteractedActor);
    UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
    FOnInteractSuccess OnInteractSuccess;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTargetChanged, AActor*, OldTarget, AActor*, NewTarget);
    UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
    FOnTargetChanged OnTargetChanged;

    // 부착할 기본 위젯 컴포넌트 클래스 (BP로 서브클래스 설정)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|UI")
    TSubclassOf<UUserWidget> InteractionWidget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|UI")
    FVector WidgetWorldOffset = FVector(0, 0, 50.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|UI")
	int CustomDepthStencilValue = 252;

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|UI")
    bool bTagWidgetIgnoreDepth = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|UI", meta=(ClampMin="0.1", ClampMax="5.0"))
    float WidgetAnchorHeightFactor = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|UI", meta=(ClampMin="10.0"))
    float MaxWidgetAnchorHeight = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|UI")
    bool bClampWidgetAnchorHeight = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|UI")
    bool bScreenClampWidget = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|UI")
    FVector2D ScreenEdgePadding = FVector2D(24.f, 24.f);

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|UI")
    bool bMoveOffscreenWidgetToCenter = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|UI", meta=(ClampMin="10.0"))
    float OffscreenFallbackDistance = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|UI")
    FVector OffscreenFallbackOffset = FVector(0,0,0);

protected:
    void UpdateTarget();
    bool IsActorInteractable(AActor* Actor) const;

private:
    // Current and previous interaction targets.
    UPROPERTY()
    TWeakObjectPtr<AActor> CurrentTarget;

    UPROPERTY()
    TWeakObjectPtr<AActor> PreviousTarget;

    // Replicated carried parcel.
    UPROPERTY(ReplicatedUsing = OnRep_CarriedParcel)
    TObjectPtr<class AParcelActor> CarriedParcel = nullptr;

    UFUNCTION()
    void OnRep_CarriedParcel(class AParcelActor* LastParcel);

    void HandleCarriedParcelChanged(class AParcelActor* LastParcel);

    bool PerformInteract(UObject* InteractableObject, class ACharacter* OwnerCharacter);
    void UpdateShelfSlotFocus(AShelfActor* Shelf);
    void ClearShelfSlotFocus(AShelfActor* ShelfToClear = nullptr);
    void UpdateShelfPlacementPreview(AShelfActor* Shelf);
    void ClearShelfPlacementPreview(AShelfActor* ShelfToClear = nullptr);

    // Viewport interaction widget instance (screen space). Created when target changes if needed.
    UPROPERTY(Transient)
    UUserWidget* ActiveInteractionWidget = nullptr;

    // Show/hide helpers for screen widget.
    void ShowInteractionWidget(AActor* TargetActor);
    void HideInteractionWidget();

    // (Widget component & anchor removed)

    UPROPERTY()
    TWeakObjectPtr<AShelfActor> FocusedShelf;

    UPROPERTY()
    TWeakObjectPtr<AShelfActor> PreviewShelf;

    int32 FocusedSlotIndex = INDEX_NONE;

    UPROPERTY()
    FShelfPlacementPreview CachedPlacementPreview;

    bool bHasPlacementPreview = false;
};


