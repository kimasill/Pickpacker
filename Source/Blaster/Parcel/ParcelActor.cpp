// Fill out your copyright notice in the Description page of Project Settings.

#include "ParcelActor.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/Components/CarryIKComponent.h"
#include "Blaster/Shelf/ShelfActor.h"
#include "Blaster/Components/InteractionComponent.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Blaster/DataAssets/DA_ParcelData.h"
#include "Blaster/DataAssets/DA_ItemData.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Blaster/Components/PlayerInventoryComponent.h"  // 추가
#include "GameplayTagsManager.h"
#include "Engine/StaticMesh.h"

AParcelActor::AParcelActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	// Create components
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	MeshComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	MeshComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
	MeshComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_EngineTraceChannel4, ECollisionResponse::ECR_Ignore);
	MeshComponent->SetIsReplicated(true);

	ParcelStateComponent = CreateDefaultSubobject<UParcelStateComponent>(TEXT("ParcelStateComponent"));
	CarryPointsComponent = CreateDefaultSubobject<UCarryPointsComponent>(TEXT("CarryPointsComponent"));
	
	HUDWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HUDWidgetComponent"));
	HUDWidgetComponent->SetupAttachment(RootComponent);
	HUDWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	HUDWidgetComponent->SetDrawAtDesiredSize(true);

	PickupWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PickupWidget"));
	PickupWidget->SetupAttachment(RootComponent);
	PickupWidget->SetWidgetSpace(EWidgetSpace::Screen);
	PickupWidget->SetDrawAtDesiredSize(true);
	

	// Initialize state
	bIsAttached = false;
	CurrentSocketId = NAME_None;
	CurrentCarrier = nullptr;
	DropStabilizeTime = 1.0f;
	DropImpulseMultiplier = 1.0f;
	bEnableDebugLogging = true;
}

void AParcelActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bAutoApplyParcelData)
	{
		ApplyParcelConfigFromDataAssetInternal(false, false);
	}
	else
	{
		UpdateMeshForCurrentPackagingState();
	}
}

void AParcelActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AParcelActor, bIsAttached);
	DOREPLIFETIME(AParcelActor, CurrentSocketId);
	DOREPLIFETIME(AParcelActor, CurrentCarrier);
	DOREPLIFETIME(AParcelActor, bIsPackaged);
	DOREPLIFETIME(AParcelActor, bIsItem);
	DOREPLIFETIME(AParcelActor, bIsUsable);
	DOREPLIFETIME(AParcelActor, ItemType);
	DOREPLIFETIME(AParcelActor, ParcelPrice);
	DOREPLIFETIME(AParcelActor, ParcelClassificationTag);
	DOREPLIFETIME(AParcelActor, ParcelItemTag);
}

void AParcelActor::BeginPlay()
{
	Super::BeginPlay();

	const bool bShouldInitializeRuntimeConfig = HasAuthority();
	bool bConfigApplied = false;
	if (bAutoApplyParcelData)
	{
		bConfigApplied = ApplyParcelConfigFromDataAssetInternal(bShouldInitializeRuntimeConfig, true);
	}

	if (!bConfigApplied)
	{
		if (bShouldInitializeRuntimeConfig)
		{
			InitializeParcel(ParcelConfig);
		}
		else
		{
			ApplyParcelConfigVisuals(ParcelConfig);
		}
	}

	if (ParcelStateComponent)
	{
		ParcelStateComponent->OnParcelStateChanged.AddDynamic(this, &AParcelActor::OnParcelStateChanged);		
	}

	// Initialize HUD widget
	if (HUDWidgetClass && HUDWidgetComponent)
	{
		HUDWidget = CreateWidget<UParcelHUDWidget>(GetWorld(), HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidgetComponent->SetWidget(HUDWidget);
			HUDWidgetComponent->SetVisibility(false); // Hidden by default
			}
	}

	if (PickupWidget)
	{
		PickupWidget->SetVisibility(false); // Hidden by default
	}

	// Configure physics
	if (MeshComponent)
	{
		if (HasAuthority())
		{
			MeshComponent->OnComponentHit.AddDynamic(this, &AParcelActor::HandleParcelMeshHit);
		}

		if (HasAuthority())
		{
			if (!bIsAttached)
			{
				MeshComponent->SetSimulatePhysics(true);
				MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			}
		}
		else
		{
			MeshComponent->SetSimulatePhysics(false);
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		MeshComponent->SetRenderCustomDepth(false);
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Initialized - Classification: %s, Location: %s"),
			*ParcelConfig.ClassificationTag.ToString(), *GetActorLocation().ToString());
	}
}

void AParcelActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update HUD widget
	UpdateHUDWidget();
}

void AParcelActor::InitializeParcel(const FParcelConfig& Config)
{
	ApplyParcelConfigVisuals(Config);

	if (!HasAuthority())
	{
		return;
	}

	ParcelPrice = FMath::Max(0, Config.BasePrice);
	ParcelTags = FGameplayTagContainer();

	ParcelClassificationTag = Config.ClassificationTag;
	if (!ParcelClassificationTag.IsValid())
	{
		ParcelClassificationTag = FGameplayTag::RequestGameplayTag(TEXT("Parcel-Classification.Standard"), false);
	}
	if (ParcelClassificationTag.IsValid())
	{
		ParcelTags.AddTag(ParcelClassificationTag);
	}

	ParcelItemTag = FGameplayTag();
	if (Config.ItemTag.IsValid())
	{
		ParcelItemTag = Config.ItemTag;
		ParcelTags.AddTag(ParcelItemTag);
	}

	if (Config.ParcelTag.IsValid())
	{
		ParcelTags.AddTag(Config.ParcelTag);
	}

	if (ParcelStateComponent)
	{
		ParcelStateComponent->InitializeParcel(Config);
		ParcelStateComponent->SetInternalItemData(Config.ItemData);
	}

	if (Config.ItemData.ItemType != EItemType::Unknown)
	{
		SetItemData(Config.ItemData);
	}

	bIsItem = !bIsPackaged;

	const FGameplayTag ContrabandTag = FGameplayTag::RequestGameplayTag(TEXT("Parcel-Classification.Contraband"), false);
	if (bIsItem && ParcelClassificationTag.IsValid() && ParcelClassificationTag.MatchesTagExact(ContrabandTag) && ItemType == EItemType::Unknown)
	{
		ItemType = EItemType::Key;
		bIsUsable = true;
		ItemData.ItemType = ItemType;
		ItemData.ItemName = Config.ParcelName.IsEmpty() ? TEXT("Contraband Item") : Config.ParcelName;
		ItemData.bIsUsable = true;
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Parcel initialized - Classification: %s, Name: %s"),
			*ParcelClassificationTag.ToString(), *Config.ParcelName);
	}
}

bool AParcelActor::ApplyParcelConfigFromDataAsset(bool bInitializeRuntime)
{
	return ApplyParcelConfigFromDataAssetInternal(bInitializeRuntime, true);
}

bool AParcelActor::ApplyParcelConfigFromDataAssetInternal(bool bInitializeRuntime, bool bLogWarnings)
{
	FParcelConfig ResolvedConfig;
	if (!TryResolveParcelConfig(ResolvedConfig, bLogWarnings))
	{
		return false;
	}

	if (bInitializeRuntime)
	{
		InitializeParcel(ResolvedConfig);
	}
	else
	{
		ApplyParcelConfigVisuals(ResolvedConfig);
	}

	return true;
}

bool AParcelActor::TryResolveParcelConfig(FParcelConfig& OutConfig, bool bLogWarnings) const
{
	if (!ParcelDataAsset)
	{
		if (bLogWarnings)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] ParcelDataAsset is not assigned on %s"), *GetName());
		}
		return false;
	}

	if (!ParcelDefinitionTag.IsValid())
	{
		if (bLogWarnings)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] ParcelDefinitionTag is not set on %s"), *GetName());
		}
		return false;
	}

	if (!ParcelDataAsset->GetParcelConfigByTag(ParcelDefinitionTag, OutConfig))
	{
		if (bLogWarnings)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] Failed to resolve parcel config for tag %s on %s"),
				*ParcelDefinitionTag.ToString(), *GetName());
		}
		return false;
	}

	return true;
}

void AParcelActor::ApplyParcelConfigVisuals(const FParcelConfig& Config)
{
	ParcelConfig = Config;

	if (!Config.UnpackagedMeshAsset.IsNull())
	{
		DefaultUnpackagedMesh = Config.UnpackagedMeshAsset.LoadSynchronous();
		OriginalMesh = DefaultUnpackagedMesh;
	}

	if (!Config.PackagedMeshAsset.IsNull())
	{
		PackagedMesh = Config.PackagedMeshAsset.LoadSynchronous();
	}

	if (!OriginalMesh && MeshComponent)
	{
		OriginalMesh = MeshComponent->GetStaticMesh();
	}

	if (!DefaultUnpackagedMesh && OriginalMesh)
	{
		DefaultUnpackagedMesh = OriginalMesh;
	}

	UpdateMeshForCurrentPackagingState();
}

bool AParcelActor::UpdateMeshForCurrentPackagingState()
{
	if (!MeshComponent)
	{
		return false;
	}

	UStaticMesh* TargetMesh = nullptr;

	if (bIsPackaged || PackageOnSpawn)
	{
		TargetMesh = PackagedMesh;
		bIsPackaged = TargetMesh != nullptr;
	}
	else
	{
		TargetMesh = OriginalMesh ? OriginalMesh : DefaultUnpackagedMesh;
	}

	if (!TargetMesh)
	{
		return false;
	}

	MeshComponent->SetStaticMesh(TargetMesh);
	return true;
}

void AParcelActor::RequestAttach(ACharacter* Carrier, const FName& SocketId)
{
    if (!Carrier)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] Invalid carrier"));
        return;
    }

    if (!CanBeAttached())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] Parcel cannot be attached"));
        return;
    }

    Server_RequestAttach(Carrier, SocketId);
}

void AParcelActor::RequestDrop(const FVector& Impulse)
{
    if (!IsAttached())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] Parcel is not attached"));
        return;
    }

    Server_RequestDrop(Impulse);
}
void AParcelActor::Server_RequestAttach_Implementation(ACharacter* Carrier, FName SocketId)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!Carrier || !CanBeAttached())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] Server attach request failed - Invalid carrier or cannot attach"));
		return;
	}

	// Try to attach to carry points
	if (CarryPointsComponent && CarryPointsComponent->TryAttachToSocket(Carrier, SocketId))
	{
		bIsAttached = true;
		CurrentSocketId = SocketId;
		CurrentCarrier = Carrier;
		SetOwner(Carrier);
		if (AShelfActor* Shelf = OccupyingShelf.Get())
		{
			Shelf->RemoveParcel(this);
		}

		// Update parcel state
		if (ParcelStateComponent)
		{
			ParcelStateComponent->SetAttachedState(true, SocketId);

			// Check if this is two-person carry
			bool bIsTwoPersonCarry = CarryPointsComponent && CarryPointsComponent->IsTwoPersonCarry();
			ParcelStateComponent->ApplyTwoPersonCarryBonuses(bIsTwoPersonCarry);
		}

		if (ABlasterCharacter* BlasterCarrier = Cast<ABlasterCharacter>(Carrier))
		{
			if (UInteractionComponent* InteractionComponent = BlasterCarrier->GetInteractionComponent())
			{
				InteractionComponent->SetCarriedParcel(this);
			}

			if (UCarryIKComponent* CarryIKComponent = BlasterCarrier->GetCarryIKComponent())
			{
				CarryIKComponent->EnableIK(this, SocketId);
			}
		}

		// Configure physics for attachment
		if (MeshComponent)
		{
			MeshComponent->SetSimulatePhysics(false);
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}

		// Attach Parcel to character mesh socket
		USkeletalMeshComponent* CharacterMesh = Carrier->GetMesh();
		if (CharacterMesh)
		{
			const USkeletalMeshSocket* CarrySocket = CharacterMesh->GetSocketByName(SocketId);
			if (CarrySocket)
			{
				CarrySocket->AttachActor(this, CharacterMesh);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] Socket not found: %s"), *SocketId.ToString());
			}
		}

		// Broadcast attach event
		Multicast_ParcelAttached(Carrier, SocketId);

		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Server attach successful - Carrier: %s, Socket: %s"),
				*Carrier->GetName(), *SocketId.ToString());
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] Server attach failed - Socket not available"));
	}
}

void AParcelActor::Server_RequestDrop_Implementation(FVector Impulse)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bIsAttached)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] Server drop request failed - Not attached"));
		return;
	}

	FVector DropLocation = GetActorLocation();
	ACharacter* DroppingCarrier = CurrentCarrier;
	const FName DroppingSocket = CurrentSocketId;

	// Detach from character mesh
	if (DroppingCarrier)
	{
		USkeletalMeshComponent* CharacterMesh = DroppingCarrier->GetMesh();
		if (CharacterMesh)
		{
			DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}
	}

	// Detach from carry points
	if (CarryPointsComponent && DroppingCarrier)
	{
		CarryPointsComponent->DetachFromSocket(DroppingCarrier, DroppingSocket);
	}

	// Update parcel state
	if (ParcelStateComponent)
	{
		ParcelStateComponent->SetAttachedState(false, NAME_None);
		ParcelStateComponent->ApplyTwoPersonCarryBonuses(false); // No longer two-person carry
	}

	if (MeshComponent)
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComponent->SetSimulatePhysics(true);
	}


	// Configure physics for drop
	ConfigureDropPhysics(Impulse);

	// Broadcast drop event
	Multicast_ParcelDropped(DroppingCarrier, DropLocation);

	if (ABlasterCharacter* BlasterCarrier = Cast<ABlasterCharacter>(DroppingCarrier))
	{
		if (UInteractionComponent* InteractionComponent = BlasterCarrier->GetInteractionComponent())
		{
			InteractionComponent->SetCarriedParcel(nullptr);
		}

		if (UCarryIKComponent* CarryIKComponent = BlasterCarrier->GetCarryIKComponent())
		{
			CarryIKComponent->DisableIK();
		}
	}

	// Reset attachment state
	bIsAttached = false;
	CurrentSocketId = NAME_None;
	CurrentCarrier = nullptr;
	SetOwner(nullptr);

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Server drop successful - Carrier: %s, Location: %s"),
			DroppingCarrier ? *DroppingCarrier->GetName() : TEXT("None"),
			*DropLocation.ToString());
	}
}

bool AParcelActor::CanBeAttached() const
{
    if (!ParcelStateComponent)
    {
        return false;
    }

    // Check if parcel is broken
    if (ParcelStateComponent->IsBroken())
    {
        return false;
    }

    // Check if already attached
    if (bIsAttached)
    {
        return false;
    }

    return true;
}

bool AParcelActor::IsAttached() const
{
    return bIsAttached;
}

const FParcelState& AParcelActor::GetParcelState() const
{
    if (ParcelStateComponent)
    {
        return ParcelStateComponent->GetParcelState();
    }
    
    static FParcelState EmptyState;
    return EmptyState;
}


void AParcelActor::Multicast_ParcelAttached_Implementation(ACharacter* Carrier, FName SocketId)
{
	OnParcelAttached.Broadcast(Carrier, SocketId);
	if (Carrier)
	{
		USkeletalMeshComponent* CharacterMesh = Carrier->GetMesh();
		if (CharacterMesh)
		{
			const USkeletalMeshSocket* CarrySocket = CharacterMesh->GetSocketByName(SocketId);
			if (CarrySocket)
			{
				CarrySocket->AttachActor(this, CharacterMesh);
			}
		}
	}
	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Multicast attach - Carrier: %s, Socket: %s"),
			Carrier ? *Carrier->GetName() : TEXT("None"), *SocketId.ToString());
	}
}

void AParcelActor::Multicast_ParcelDropped_Implementation(ACharacter* Carrier, FVector DropLocation)
{
    OnParcelDropped.Broadcast(DropLocation);
	if (Carrier)
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Multicast drop - Carrier: %s, Location: %s"),
            Carrier ? *Carrier->GetName() : TEXT("None"),
            *DropLocation.ToString());
    }
}

void AParcelActor::NotifyBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
    const APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn) return;

    if (Pawn->IsLocallyControlled() && PickupWidget)
    {        
        PickupWidget->SetVisibility(true);
    }
}

void AParcelActor::NotifyEndOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn) return;

	if (Pawn->IsLocallyControlled() && PickupWidget)
	{
		PickupWidget->SetVisibility(false);
	}
}

void AParcelActor::OnParcelStateChanged(const FParcelState& NewState)
{
	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Parcel state changed - Durability: %.2f, Weight: %.2f, Instability: %.2f"),
			NewState.Durability, NewState.Weight, NewState.Instability);
	}

	// Update HUD widget
	UpdateHUDWidget();

	// Check if parcel is broken
	if (NewState.Durability <= 0.0f)
	{
		HandleParcelBroken();
	}
}

void AParcelActor::HandleParcelBroken()
{
	UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] Parcel broken! Classification: %s"),
		*ParcelConfig.ClassificationTag.ToString());

	// Detach if attached
	if (bIsAttached)
	{
		RequestDrop(FVector::ZeroVector);
	}

	// Broadcast broken event
	OnParcelBroken.Broadcast(this);

	if (AShelfActor* Shelf = OccupyingShelf.Get())
	{
		Shelf->RemoveParcel(this);
	}

	Destroy();
}

void AParcelActor::UpdateHUDWidget()
{
	if (!HUDWidget || !ParcelStateComponent)
	{
		return;
	}

	// Update HUD visibility based on attachment state
	bool bShouldShowHUD = bIsAttached;
	if (HUDWidgetComponent)
	{
		HUDWidgetComponent->SetVisibility(bShouldShowHUD);
	}

	if (bShouldShowHUD)
	{
		// Update HUD with current parcel state
		FParcelState CurrentState = ParcelStateComponent->GetParcelState();
		HUDWidget->UpdateParcelState(CurrentState, ParcelConfig.ClassificationTag);
	}
}

void AParcelActor::SetHUDWidgetClass(TSubclassOf<UParcelHUDWidget> WidgetClass)
{
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] Invalid HUD widget class"));
		return;
	}

	HUDWidgetClass = WidgetClass;

	// Recreate HUD widget if it exists
	if (HUDWidgetComponent)
	{
		HUDWidget = CreateWidget<UParcelHUDWidget>(GetWorld(), HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidgetComponent->SetWidget(HUDWidget);
			HUDWidgetComponent->SetVisibility(false); // Hidden by default
		}
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelActor] HUD widget class set to: %s"), *WidgetClass->GetName());
	}
}

void AParcelActor::HandleParcelMeshHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!HasAuthority() || !ParcelStateComponent)
	{
		return;
	}

	const float ImpactForce = NormalImpulse.Size();
	if (ImpactForce <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	ParcelStateComponent->ApplyImpactDamage(ImpactForce, TEXT("Impact"));

	if (ParcelStateComponent->IsBroken())
	{
		HandleParcelBroken();
	}
}

void AParcelActor::ConfigureDropPhysics(const FVector& Impulse)
{
	if (!MeshComponent)
	{
		return;
	}

	// Enable physics
	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	// Apply impulse
	if (!Impulse.IsZero())
	{
		MeshComponent->AddImpulse(Impulse * DropImpulseMultiplier);
	}

	// Start stabilization timer
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(StabilizeTimerHandle, this, &AParcelActor::StabilizePhysics, DropStabilizeTime, false);
	}
}

void AParcelActor::StabilizePhysics()
{
	if (!MeshComponent)
	{
		return;
	}

	// Reduce physics simulation to improve performance
	MeshComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
	MeshComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Physics stabilized"));
	}
}

// InteractableInterface Implementation
bool AParcelActor::OnInteract_Implementation(ACharacter* Interactor)
{
	if (!Interactor)
	{
		return false;
	}	

	// 일반 택배인 경우 운반
	if (!CanBeAttached())
	{
		return false;
	}

	// RequestAttach 호출
	RequestAttach(Interactor, FName("CarrySocket"));
	return true;
}

bool AParcelActor::CanInteract_Implementation(ACharacter* Interactor) const
{
	if (!Interactor)
	{
		return false;
	}

	return CanBeAttached();
}

FText AParcelActor::GetInteractText_Implementation() const
{
	// 아이템인 경우
	if (bIsItem)
	{
		return FText::FromString(FString::Printf(TEXT("Press E to Collect %s"), *ItemData.ItemName));
	}

	// 일반 택배인 경우
	if (CanBeAttached())
	{
		return FText::FromString(TEXT("Press E to Pickup"));
	}
	return FText::GetEmpty();
}

void AParcelActor::StartHighlight_Implementation()
{
	if (!MeshComponent)
	{
		return;
	}
	MeshComponent->SetRenderCustomDepth(true);
	MeshComponent->SetCustomDepthStencilValue(252);
	ShowPickupWidget(true);
}

void AParcelActor::EndHighlight_Implementation()
{
	if (!MeshComponent)
	{
		return;
	}
	MeshComponent->SetRenderCustomDepth(false);
	ShowPickupWidget(false);
}

void AParcelActor::ShowPickupWidget(bool bShowWidget)
{
	if (PickupWidget)
	{
		PickupWidget->SetVisibility(bShowWidget);
	}
}

void AParcelActor::AssignToShelf(AShelfActor* Shelf, int32 SlotIndex)
{
    if (!HasAuthority())
    {
        return;
    }

    OccupyingShelf = Shelf;
    OccupyingShelfSlotIndex = SlotIndex;
}

void AParcelActor::SetPackaged(bool bPackaged)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bIsPackaged == bPackaged)
	{
		return; // 이미 같은 상태
	}

	const bool bPreviousPackagedState = bIsPackaged;
	bIsPackaged = bPackaged;
	
	// 포장되지 않은 상태 = 아이템
	bIsItem = !bPackaged;

	if (!MeshComponent)
	{
		return;
	}

	if (!OriginalMesh && DefaultUnpackagedMesh)
	{
		OriginalMesh = DefaultUnpackagedMesh;
	}
	else if (!OriginalMesh)
	{
		OriginalMesh = MeshComponent->GetStaticMesh();
	}

	if (bPackaged && !PackagedMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] PackagedMesh not set, cannot package"));
		bIsPackaged = bPreviousPackagedState;
		bIsItem = !bPreviousPackagedState;
		return;
	}

	if (!UpdateMeshForCurrentPackagingState())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] Failed to update mesh for packaging state"));
		bIsPackaged = bPreviousPackagedState;
		bIsItem = !bPreviousPackagedState;
		return;
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Packaging state changed to: %s (IsItem: %s)"), 
			bPackaged ? TEXT("Packaged") : TEXT("Unpackaged"),
			bIsItem ? TEXT("Yes") : TEXT("No"));
	}
}

void AParcelActor::ClearShelfAssignment(AShelfActor* Shelf)
{
    if (!HasAuthority())
    {
        return;
    }

    if (Shelf && OccupyingShelf.Get() != Shelf)
    {
        return;
    }

    OccupyingShelf = nullptr;
    OccupyingShelfSlotIndex = INDEX_NONE;
}

void AParcelActor::SetItemData(const FItemData& NewItemData)
{
	if (!HasAuthority())
	{
		return;
	}

	ItemData = NewItemData;
	ItemType = NewItemData.ItemType;
	bIsUsable = NewItemData.bIsUsable;

	if (ParcelStateComponent)
	{
		ParcelStateComponent->SetInternalItemData(NewItemData);
	}
	
	// 포장되지 않은 상태면 아이템으로 설정
	if (!bIsPackaged)
	{
		bIsItem = true;
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Item data set - Type: %s, Name: %s, Usable: %s"),
			*UEnum::GetValueAsString(ItemType), *ItemData.ItemName, bIsUsable ? TEXT("Yes") : TEXT("No"));
	}
}

bool AParcelActor::UseItem(ACharacter* User)
{
	if (!User || !bIsItem || !bIsUsable)
	{
		return false;
	}

	if (!HasAuthority())
	{
		Server_UseItem(User);
		return true;
	}

	return Handle_UseItem(User);
}

void AParcelActor::Server_UseItem_Implementation(ACharacter* User)
{
	Handle_UseItem(User);
}

bool AParcelActor::Handle_UseItem(ACharacter* User)
{

	if (!User || !bIsItem || !bIsUsable)
	{
		return false;
	}

	// 아이템 타입별 사용 효과 처리
	bool bSuccess = false;

	switch (ItemType)
	{
	case EItemType::Key:
		// 열쇠 사용 - 블루프린트에서 구현 가능하도록 이벤트 브로드캐스트
		OnItemUsed.Broadcast(User, ItemType);
		bSuccess = true;
		break;

	case EItemType::Tool:
		// 도구 사용 - 시스템 파훼 등
		OnItemUsed.Broadcast(User, ItemType);
		bSuccess = true;
		break;

	case EItemType::Consumable:
		// 소비 아이템 사용
		OnItemUsed.Broadcast(User, ItemType);
		bSuccess = true;
		if (ItemData.bConsumedOnUse)
		{
			// 아이템 소비 처리
			Destroy();
		}
		break;

	default:
		// 다른 타입은 블루프린트에서 처리
		OnItemUsed.Broadcast(User, ItemType);
		bSuccess = true;
		break;
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Item used - Type: %s, User: %s, Success: %s"),
			*UEnum::GetValueAsString(ItemType),
			User ? *User->GetName() : TEXT("None"),
			bSuccess ? TEXT("Yes") : TEXT("No"));
	}

	return bSuccess;
}

