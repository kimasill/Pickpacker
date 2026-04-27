// Fill out your copyright notice in the Description page of Project Settings.

#include "ParcelActor.h"
#include "UnpackedParcelActor.h"
#include "PackedParcelActor.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "Character/BlasterCharacter.h"
#include "Components/CarryIKComponent.h"
#include "Shelf/ShelfActor.h"
#include "Components/InteractionComponent.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "DataAssets/DA_ParcelData.h"
#include "DataAssets/DA_ItemData.h"
#include "DataAssets/DA_ParcelData.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Components/PlayerInventoryComponent.h"
#include "GameplayTagsManager.h"
#include "Engine/StaticMesh.h"
#include "Parcel/ParcelAVLibrary.h"
#include "Sound/SoundBase.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/DecalComponent.h"
#include "Sound/SoundCue.h"
#include "Gate/GateActor.h"
#include "GameState/PickpackerGameState.h"
#include "Environment/ConveyorBeltActor.h"

AParcelActor::AParcelActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	bHasUnpacked = false;

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
	
	bRequiresTwoHandCarry = false;

	// Initialize state
	bIsAttached = false;
	CurrentSocketId = NAME_None;
	CurrentCarrier = nullptr;
	DropStabilizeTime = 1.0f;
	DropImpulseMultiplier = 1.0f;
	bEnableDebugLogging = true;

	// Initialize impact damage properties
	MinImpactSpeedForDamage = 10.0f;
	MinImpactImpulseForDamage = 100.0f;
	ImpactDamageCooldown = 0.5f;
	LastImpactTime = 0.0;
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
	DOREPLIFETIME(AParcelActor, bRequiresTwoHandCarry);
	DOREPLIFETIME(AParcelActor, PackageTargetTag);
	DOREPLIFETIME(AParcelActor, PackageTargetRowName);
	DOREPLIFETIME(AParcelActor, PackageRequiredCount);
	DOREPLIFETIME(AParcelActor, PackageMinRequiredCount);
	DOREPLIFETIME(AParcelActor, PackageMaxRequiredCount);
	DOREPLIFETIME(AParcelActor, bIsPackageBundle);
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

	// Derive two-hand carry requirement from available carry sockets
	if (CarryPointsComponent)
	{
		const int32 SocketCount = CarryPointsComponent->GetCarrySockets().Num();
		bRequiresTwoHandCarry = SocketCount >= 2;
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
				MeshComponent->SetNotifyRigidBodyCollision(true);
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
		UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Initialize~12=-098d - Classification: %s, Location: %s"),
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

	// Add ItemId from ItemData to ParcelTags if valid
	if (Config.ItemData.ItemId.IsValid())
	{
		ParcelTags.AddTag(Config.ItemData.ItemId);
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
	const bool bHasMeaningfulItemData =
    Config.ItemData.ItemType != EItemType::Unknown ||
    Config.ItemData.ItemId.IsValid() ||
    Config.ItemData.GripType != EGripType::None ||
    Config.ItemData.bIsUsable ||
    Config.ItemData.UseActions.Num() > 0;

	if (bHasMeaningfulItemData)
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

void AParcelActor::SetMeshPhysics(bool bEnablePhysics)
{

	// Configure physics for attachment
	if (MeshComponent)
	{
		MeshComponent->SetSimulatePhysics(bEnablePhysics);
		if (bEnablePhysics)
		{
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		else
		{
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}
	}
}

void AParcelActor::SetPackageRecipe(const FParcelPackageRecipe* InRecipe, UDA_ParcelData* InParcelDataAsset)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!InRecipe)
	{
		bIsPackageBundle = false;
		PackageTargetTag = FGameplayTag();
		PackageTargetRowName = NAME_None;
		PackageRequiredCount = 0;
		PackageMinRequiredCount = 0;
		PackageMaxRequiredCount = 0;
		PackageMeshAsset.Reset();
		PackageParcelDataAsset = nullptr;
		PackageContents.Reset();
		return;
	}

	bIsPackageBundle = true;
	PackageTargetTag = InRecipe->TargetParcelTags.Num() > 0 ? InRecipe->TargetParcelTags[0] : FGameplayTag();
	PackageTargetRowName = InRecipe->TargetParcelRowNames.Num() > 0 ? InRecipe->TargetParcelRowNames[0] : NAME_None;
	PackageRequiredCount = InRecipe->MaxRequiredCount;
	PackageMinRequiredCount = InRecipe->MinRequiredCount;
	PackageMaxRequiredCount = InRecipe->MaxRequiredCount;
	if (InRecipe->PackagedMeshes.Num() > 0)
	{
		const int32 ExistingIndex = PackageMeshAsset.IsNull()
			? INDEX_NONE
			: InRecipe->PackagedMeshes.IndexOfByKey(PackageMeshAsset);
		if (ExistingIndex == INDEX_NONE)
		{
			const int32 MeshIndex = FMath::RandRange(0, InRecipe->PackagedMeshes.Num() - 1);
			PackageMeshAsset = InRecipe->PackagedMeshes[MeshIndex];
		}
	}
	else
	{
		PackageMeshAsset.Reset();
	}
	PackageParcelDataAsset = InParcelDataAsset ? InParcelDataAsset : ParcelDataAsset;
	// Contents는 레시피에서 제거되었으므로 여기서 설정하지 않음 (InitialContents 또는 포장 시 자동 생성)

	if (!InRecipe->RecipeName.IsEmpty())
	{
		ParcelConfig.ParcelName = InRecipe->RecipeName;
	}

	if (InRecipe->GripType != EGripType::None)
	{
		FItemData UpdatedItemData = ItemData;
		UpdatedItemData.GripType = InRecipe->GripType;
		SetItemData(UpdatedItemData);
	}
	
	// 메시 업데이트 (포장 메시가 설정되면 즉시 표시)
	UpdateMeshForCurrentPackagingState();
	UpdatePackagedConfigFromContents();

}

void AParcelActor::SetPackageContents(const TArray<FParcelPackageContent>& InContents)
{
	if (!HasAuthority())
	{
		return;
	}
	PackageContents = InContents;
	UpdatePackagedConfigFromContents();
}

void AParcelActor::UpdatePackagedConfigFromContents()
{
	if (!HasAuthority())
	{
		return;
	}

	if (PackageContents.Num() == 0)
	{
		return;
	}

	UDA_ParcelData* DataAssetToUse = PackageParcelDataAsset ? PackageParcelDataAsset.Get() : ParcelDataAsset;
	if (!DataAssetToUse)
	{
		return;
	}

	float TotalWeight = 0.0f;
	int32 TotalUnits = 0;
	bool bHasStats = false;
	float MinDurability = 0.0f;
	float MinInstability = 0.0f;
	FGameplayTag ContentTag;

	for (const FParcelPackageContent& Entry : PackageContents)
	{
		if (Entry.Count <= 0 || Entry.ParcelRowName == NAME_None)
		{
			continue;
		}

		FParcelConfig ContentConfig;
		if (!DataAssetToUse->GetParcelConfigByName(Entry.ParcelRowName, ContentConfig))
		{
			continue;
		}

		TotalWeight += ContentConfig.BaseWeight * Entry.Count;
		TotalUnits += FMath::Max(1, ContentConfig.PackagingSpaceUnits) * Entry.Count;
		if (!bHasStats)
		{
			MinDurability = ContentConfig.BaseDurability;
			MinInstability = ContentConfig.InstabilityFactor;
			bHasStats = true;
		}
		else
		{
			MinDurability = FMath::Min(MinDurability, ContentConfig.BaseDurability);
			MinInstability = FMath::Min(MinInstability, ContentConfig.InstabilityFactor);
		}

		if (!ContentTag.IsValid())
		{
			ContentTag = ContentConfig.ClassificationTag;
		}
	}

	if (!bHasStats)
	{
		return;
	}

	FParcelConfig UpdatedConfig = ParcelConfig;
	UpdatedConfig.BaseWeight = TotalWeight;
	UpdatedConfig.BaseDurability = MinDurability;
	UpdatedConfig.InstabilityFactor = MinInstability;
	if (TotalUnits > 0)
	{
		UpdatedConfig.PackagingSpaceUnits = TotalUnits;
	}
	if (ContentTag.IsValid())
	{
		UpdatedConfig.ClassificationTag = ContentTag;
	}

	InitializeParcel(UpdatedConfig);
}

bool AParcelActor::TryResolveParcelConfig(FParcelConfig& OutConfig, bool bLogWarnings) const
{
	// 1) DataAsset RowName 우선
	if (ParcelDataAsset && ParcelDefinitionRowName != NAME_None)
	{
		if (ParcelDataAsset->GetParcelConfigByName(ParcelDefinitionRowName, OutConfig))
		{
			return true;
		}
		if (bLogWarnings)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] Invalid ParcelDefinitionRowName %s on %s"), *ParcelDefinitionRowName.ToString(), *GetName());
		}
	}

	// 2) 기존 Tag 경로 제거 → 실패
	if (bLogWarnings)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] No valid Parcel definition on %s (RowName/DataAsset)"), *GetName());
	}
	return false;
}

void AParcelActor::ApplyParcelConfigVisuals(const FParcelConfig& Config)
{
	ParcelConfig = Config;

	if (!Config.UnpackagedMeshAsset.IsNull())
	{
		DefaultUnpackagedMesh = Config.UnpackagedMeshAsset.LoadSynchronous();
		OriginalMesh = DefaultUnpackagedMesh;
	}

	if (!OriginalMesh && MeshComponent)
	{
		OriginalMesh = MeshComponent->GetStaticMesh();
	}

	if (!DefaultUnpackagedMesh && OriginalMesh)
	{
		DefaultUnpackagedMesh = OriginalMesh;
	}

	// 서브클래스에서 상태를 결정하므로 여기서는 메시만 업데이트
	UpdateMeshForCurrentPackagingState();
}

bool AParcelActor::UpdateMeshForCurrentPackagingState()
{
	if (!MeshComponent)
	{
		return false;
	}

	UStaticMesh* TargetMesh = nullptr;

	if (bIsPackaged)
	{
		TargetMesh = PackageMeshAsset.IsNull() ? nullptr : PackageMeshAsset.LoadSynchronous();
		if (!TargetMesh && MeshComponent)
		{
			TargetMesh = MeshComponent->GetStaticMesh();
		}
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

	FName RequestedSocket = SocketId;
	const bool bGenericSocketRequest = SocketId.IsNone() || SocketId == FName(TEXT("CarrySocket"));
	const FName CharacterAttachSocket = bGenericSocketRequest ? FName(TEXT("CarrySocket")) : SocketId;

	// Resolve a concrete socket name when a generic one is provided
	if (CarryPointsComponent && bGenericSocketRequest)
	{
		const FName FirstAvailable = CarryPointsComponent->GetFirstAvailableSocketName();
		if (!FirstAvailable.IsNone())
		{
			RequestedSocket = FirstAvailable;
		}
	}

	// Try to attach to carry points
	if (CarryPointsComponent && CarryPointsComponent->TryAttachToSocket(Carrier, RequestedSocket))
	{
		bIsAttached = true;
		// Track the actual occupied socket name
		const FName OccupiedSocket = CarryPointsComponent->GetSocketOccupiedByCharacter(Carrier);
		CurrentSocketId = !OccupiedSocket.IsNone() ? OccupiedSocket : RequestedSocket;
		CurrentCarrier = Carrier;
		SetOwner(Carrier);
		if (AShelfActor* Shelf = OccupyingShelf.Get())
		{
			Shelf->RemoveParcel(this);
		}

		// Update parcel state
		if (ParcelStateComponent)
		{
			ParcelStateComponent->SetAttachedState(true, CurrentSocketId);

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
				CarryIKComponent->EnableIK(this, CurrentSocketId);
			}
		}

		// Configure physics for attachment
		SetMeshPhysics(false);
		// Attach Parcel to character mesh socket
		USkeletalMeshComponent* CharacterMesh = Carrier->GetMesh();
		if (CharacterMesh)
		{
			const USkeletalMeshSocket* CarrySocket = CharacterMesh->GetSocketByName(CharacterAttachSocket);
			if (CarrySocket)
			{
				CarrySocket->AttachActor(this, CharacterMesh);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] Socket not found: %s"), *CharacterAttachSocket.ToString());
			}
		}

		// Broadcast attach event
		Multicast_ParcelAttached(Carrier, CharacterAttachSocket);
		Multicast_PlayParcelEffect(FName("PickUp"), GetActorLocation());
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Server attach successful - Carrier: %s, Socket: %s"),
				*Carrier->GetName(), *CurrentSocketId.ToString());
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

	PendingDropper = Cast<ABlasterCharacter>(DroppingCarrier);
	bPendingDropSuspicion = PendingDropper.IsValid();
	bImpactDamageEnabled = PendingDropper.IsValid();

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

	SetMeshPhysics(true);

	// Configure physics for drop
	ConfigureDropPhysics(Impulse);

	// Broadcast drop event
	Multicast_ParcelDropped(DroppingCarrier, DropLocation);
	Multicast_PlayParcelEffect(FName("PutDown"), DropLocation);

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

void AParcelActor::Multicast_PlayParcelEffect_Implementation(FName EventKey, FVector Location)
{
	if (!ParcelDataAsset)
	{
		return;
	}

	// Resolve and play sound
	if (USoundBase* Sound = UParcelAVLibrary::ResolveSound(ParcelConfig, ParcelDataAsset, EventKey))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location);
	}

	// Resolve and spawn VFX
	// VFX Map: Spill_Start and Spill_Loop both use "Spill" key
	FName VfxKey = EventKey;
	if (EventKey == FName("Spill_Start") || EventKey == FName("Spill_Loop"))
	{
		VfxKey = FName("Spill");
	}
	
	if (UNiagaraSystem* Vfx = UParcelAVLibrary::ResolveVfx(ParcelConfig, ParcelDataAsset, VfxKey))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Vfx, Location);
	}

	// Spawn decal for spill events
	if (EventKey == FName("Spill_Start") || EventKey == FName("Spill_Loop"))
	{
		FName DecalKey = FName("Spill_Decal");
		if (UMaterialInterface* DecalMaterial = UParcelAVLibrary::ResolveDecal(ParcelConfig, ParcelDataAsset, DecalKey))
		{
			UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
				GetWorld(),
				DecalMaterial,
				FVector(60.0f, 60.0f, 60.0f),
				Location,
				FRotator::ZeroRotator,
				30.0f
			);
		}
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
	// 서버에서만 처리
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] Parcel broken! Classification: %s"),
		*ParcelConfig.ClassificationTag.ToString());

	// Stop any active loop effects
	EndLeakLoop();

	// Play break effect
	FVector BreakLocation = GetActorLocation();
	Multicast_PlayParcelEffect(FName("Break"), BreakLocation);

	// Check if spill should start on break
	float SpillThresh = UParcelAVLibrary::GetSpecialProperty(ParcelConfig, TEXT("SpillOnDropThresh"), 0.0f);
	if (SpillThresh > 0.0f)
	{
		Multicast_PlayParcelEffect(FName("Spill_Start"), BreakLocation);
		BeginLeakLoop();
	}

	// Detach if attached
	if (bIsAttached)
	{
		RequestDrop(FVector::ZeroVector);
	}

	// Broadcast broken event (모든 클라이언트에서 발생하도록)
	OnParcelBroken.Broadcast(this);

	if (AShelfActor* Shelf = OccupyingShelf.Get())
	{
		Shelf->RemoveParcel(this);
	}

	// 포장된 번들인 경우 언패킹 처리
	if (bIsPackageBundle)
	{
		FTransform OutTransform = GetActorTransform();
		UnpackAtTransform(OutTransform, true); // 파괴 시 산란
	}
	else
	{
		// 일반 파슬은 파괴
		Destroy();
	}
}

void AParcelActor::UpdateHUDWidget()
{
	if (!HUDWidget || !ParcelStateComponent)
	{
		return;
	}

	// 로컬 타깃 상태에서만 표시, 들고 있을 때(bIsAttached)에는 숨김
	const bool bShouldShowHUD = bIsTargetedByLocalPlayer && !bIsAttached;
	if (HUDWidgetComponent)
	{
		HUDWidgetComponent->SetVisibility(bShouldShowHUD);
	}

	if (bShouldShowHUD)
	{
		// Update HUD with current parcel state
		FParcelState CurrentState = ParcelStateComponent->GetParcelState();
		if (HUDWidget)
		{
			HUDWidget->SetParcelName(FText::FromString(ParcelConfig.ParcelName));
		}
		HUDWidget->SetMinimalDisplay(bIsAttached); // 들고 있을 땐 최소 정보만
		// ParcelConfig의 BaseDurability를 최대 내구도로 사용
		const float MaxDurability = ParcelConfig.BaseDurability > 0.0f ? ParcelConfig.BaseDurability : 100.0f;
		HUDWidget->UpdateParcelState(CurrentState, ParcelConfig.ParcelTag, ParcelConfig.ClassificationTag, MaxDurability);

		TArray<FText> ContentNames;
		if (bIsPackageBundle && bIsPackaged && ParcelDataAsset && PackageContents.Num() > 0)
		{
			for (const FParcelPackageContent& Content : PackageContents)
			{
				if (Content.Count <= 0 || Content.ParcelRowName == NAME_None)
				{
					continue;
				}

				FParcelConfig ContentConfig;
				FText NameText = FText::FromString(Content.ParcelRowName.ToString());
				if (ParcelDataAsset->GetParcelConfigByName(Content.ParcelRowName, ContentConfig))
				{
					if (!ContentConfig.ParcelName.IsEmpty())
					{
						NameText = FText::FromString(ContentConfig.ParcelName);
					}
				}

				ContentNames.Add(NameText);
			}
		}

		HUDWidget->UpdateContentsList(ContentNames);
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

void AParcelActor::SetTargetedByLocalPlayer(bool bTargeted)
{
	bIsTargetedByLocalPlayer = bTargeted;
	UpdateHUDWidget();
}

TArray<FName> AParcelActor::GetParcelRowOptions() const
{
	TArray<FName> Options;
	if (!ParcelDataAsset)
	{
		return Options;
	}

	const int32 Num = ParcelDataAsset->ParcelConfigs.Num();
	Options.Reserve(Num);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const FParcelConfig& Config = ParcelDataAsset->ParcelConfigs[Index];
		FString Label = Config.ParcelName;
		if (Label.IsEmpty())
		{
			Label = Config.ParcelTag.IsValid() ? Config.ParcelTag.ToString() : FString::Printf(TEXT("Parcel_%d"), Index);
		}
		Options.Add(FName(*Label));
	}

	return Options;
}

void AParcelActor::HandleParcelMeshHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!HasAuthority() || !ParcelStateComponent) return;

	const bool bHitByPlayer = OtherActor && OtherActor->IsA(ABlasterCharacter::StaticClass());
	const bool bHasDropPending = bPendingDropSuspicion || bImpactDamageEnabled;

	if (bOnConveyor || (OtherActor && OtherActor->IsA(AConveyorBeltActor::StaticClass())))
	{
		if (bHasDropPending)
		{
			bPendingDropSuspicion = false;
			PendingDropper = nullptr;
			bImpactDamageEnabled = false;
		}
		return;
	}

	if (!bHitByPlayer && !bImpactDamageEnabled)
	{
		if (bHasDropPending)
		{
			bPendingDropSuspicion = false;
			PendingDropper = nullptr;
			bImpactDamageEnabled = false;
		}
		return;
	}

    const float RawImpulse = NormalImpulse.Size();
    if (RawImpulse <= KINDA_SMALL_NUMBER)
    {
		if (!bHitByPlayer && bHasDropPending)
		{
			bPendingDropSuspicion = false;
			PendingDropper = nullptr;
			bImpactDamageEnabled = false;
		}
		return;
    }

    if (bIsAttached) return;

    const float CurrentSpeed = MeshComponent ? MeshComponent->GetPhysicsLinearVelocity().Size() : 0.0f;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const bool bBelowMotionThreshold =
        (CurrentSpeed < MinImpactSpeedForDamage) &&
        (RawImpulse < MinImpactImpulseForDamage) &&
        CarryPointsComponent->GetOccupiedSocketCount() == 0;
    if (bBelowMotionThreshold)
	{
		if (!bHitByPlayer && bHasDropPending)
		{
			bPendingDropSuspicion = false;
			PendingDropper = nullptr;
			bImpactDamageEnabled = false;
		}
		return;
	}

    if ((Now - LastImpactTime) < ImpactDamageCooldown) return;
    LastImpactTime = Now;

    // 1) 스파이크 상한 (프레임 당 최대 임펄스 제한)
    const float MaxImpulsePerHit = 5000.0f; // 필요 시 튜닝 (예: 3k~10k)
    const float ClampedImpulse = FMath::Min(RawImpulse, MaxImpulsePerHit);

    // 2) Z축 과대 임펄스 완화 (바닥 충돌 스파이크 억제)
    const float ZBias = 0.5f; // Z 기여도를 절반으로 축소
    const FVector BiasedImpulse(
        NormalImpulse.X,
        NormalImpulse.Y,
        NormalImpulse.Z * ZBias
    );
    const float BiasedMagnitude = BiasedImpulse.Size();

    // 3) 속도 기반 보정 (속도에 비례한 데미지로 제한)
    const float VelocityScale = FMath::Clamp(CurrentSpeed, 0.0f, 3000.0f);
    const float VelocityWeighted = FMath::Min(BiasedMagnitude, VelocityScale * 50.0f);

    // 최종 유효 임펄스: 임계치 차감 후 사용
    const float EffectiveImpact = FMath::Max(0.0f,
        FMath::Min(ClampedImpulse, VelocityWeighted) - MinImpactImpulseForDamage);
    if (EffectiveImpact <= 0.0f)
	{
		if (!bHitByPlayer && bHasDropPending)
		{
			bPendingDropSuspicion = false;
			PendingDropper = nullptr;
			bImpactDamageEnabled = false;
		}
		return;
	}

	const float AppliedDamage = ParcelStateComponent->ApplyImpactDamage(EffectiveImpact, TEXT("Impact"));
	const float MaxDurability = ParcelConfig.BaseDurability > 0.0f ? ParcelConfig.BaseDurability : 100.0f;
	const float DamageRatio = MaxDurability > 0.0f ? FMath::Clamp(AppliedDamage / MaxDurability, 0.0f, 1.0f) : 0.0f;

	if (!bHitByPlayer && bImpactDamageEnabled)
	{
		bImpactDamageEnabled = false;
	}

	if (bPendingDropSuspicion)
	{
		const APickpackerGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APickpackerGameState>() : nullptr;
		const float Threshold = GameState ? GameState->GetParcelDropSuspicionDamageRatioThreshold() : 0.0f;
		if (Threshold <= 0.0f)
		{
			bPendingDropSuspicion = false;
			PendingDropper = nullptr;
		}
		else if (DamageRatio >= Threshold)
		{
			if (PendingDropper.IsValid())
			{
				PendingDropper->ReportSuspiciousBehavior(ESuspiciousBehavior::DroppingParcel);
			}
			bPendingDropSuspicion = false;
			PendingDropper = nullptr;
		}
	}

    FName ImpactKey = DamageRatio < 0.15f ? FName("Impact_Light")
                     : DamageRatio < 0.30f ? FName("Impact_Med")
                     : DamageRatio < 0.60f ? FName("Impact_Heavy")
                     : FName("Impact_Heavy");

    Multicast_PlayParcelEffect(ImpactKey, Hit.ImpactPoint);

    const float SpillThresh = UParcelAVLibrary::GetSpecialProperty(ParcelConfig, TEXT("SpillOnDropThresh"), 0.0f);
    if (SpillThresh > 0.0f && EffectiveImpact >= SpillThresh)
    {
        Multicast_PlayParcelEffect(FName("Spill_Start"), Hit.ImpactPoint);
        BeginLeakLoop();
    }

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

	if (bPendingDropSuspicion)
	{
		bPendingDropSuspicion = false;
		PendingDropper = nullptr;
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Physics stabilized"));
	}
}

// InteractableInterface Implementation
void AParcelActor::OnInteract_Implementation(ACharacter* Interactor)
{
	if (!Interactor)
	{
		return;
	}

	if (!CanBeAttached())
	{
		return;
	}

	RequestAttach(Interactor, FName("CarrySocket"));
	return;
}

bool AParcelActor::CanInteract_Implementation(ACharacter* Interactor)
{
	if (!Interactor)
	{
		return false;
	}

	return CanBeAttached();
}

FText AParcelActor::GetInteractText_Implementation()
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

void AParcelActor::Destroyed()
{
	Super::Destroyed();
}

void AParcelActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason != EEndPlayReason::Destroyed)
	{
		bSkipContentSpawnOnDestroy = true;
	}

	Super::EndPlay(EndPlayReason);
}

//-----------------------------------------------
// Packaging bundle helpers
//-----------------------------------------------

void AParcelActor::SpawnPackageContents(const FTransform& SpawnTransform, bool bScatterAroundLocation)
{
	if (!HasAuthority())
	{
		return;
	}

	if (PackageContents.Num() == 0)
	{
		return;
	}

	bHasUnpacked = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UDA_ParcelData* DataAssetToUse = PackageParcelDataAsset ? PackageParcelDataAsset.Get() : ParcelDataAsset;

	for (const FParcelPackageContent& Entry : PackageContents)
	{
		if (Entry.Count <= 0)
		{
			continue;
		}

		FName RowNameToUse = Entry.ParcelRowName;
		if (RowNameToUse == NAME_None && PackageTargetRowName != NAME_None)
		{
			RowNameToUse = PackageTargetRowName;
		}

		TSubclassOf<AParcelActor> SpawnClass = Entry.ParcelClass;
		if (RowNameToUse != NAME_None && DataAssetToUse)
		{
			FParcelConfig DummyConfig;
			if (!DataAssetToUse->GetParcelConfigByName(RowNameToUse, DummyConfig))
			{
				if (bEnableDebugLogging)
				{
					UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] Failed to resolve parcel row '%s' for package content on %s"), *RowNameToUse.ToString(), *GetName());
				}
				continue;
			}

			// UnpackedParcelActor를 기본으로 사용
			if (!SpawnClass)
			{
				SpawnClass = AUnpackedParcelActor::StaticClass();
			}
		}
		else if (RowNameToUse != NAME_None && !DataAssetToUse && bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] No ParcelDataAsset available to resolve row '%s' on %s"),
				*RowNameToUse.ToString(),
				*GetName());
		}

		// 기본 클래스로 UnpackedParcelActor 사용
		if (!SpawnClass)
		{
			SpawnClass = AUnpackedParcelActor::StaticClass();
		}

		if (!SpawnClass)
		{
			continue;
		}

		for (int32 i = 0; i < Entry.Count; ++i)
		{
			FTransform UseTransform = SpawnTransform;
			if (bScatterAroundLocation)
			{
				const FVector RandOffset = FMath::VRand() * FMath::FRandRange(10.f, 60.f);
				UseTransform.SetLocation(SpawnTransform.GetLocation() + RandOffset);
			}

			FActorSpawnParameters Params;
			Params.Owner = this;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			AParcelActor* Spawned = World->SpawnActor<AParcelActor>(SpawnClass, UseTransform, Params);
			if (Spawned)
			{
				if (RowNameToUse != NAME_None && DataAssetToUse)
				{
					Spawned->SetParcelDataAsset(DataAssetToUse);
					Spawned->SetParcelDefinitionRowName(RowNameToUse);
					Spawned->ApplyParcelConfigFromDataAsset(true);
				}

				if (bEnableDebugLogging)
				{
					UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Spawned content parcel: %s (Row: %s)"), *Spawned->GetName(), *RowNameToUse.ToString());
				}
			}
		}
	}
}

void AParcelActor::UnpackAtTransform(const FTransform& OutTransform, bool bScatterAroundLocation)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bHasUnpacked)
	{
		return;
	}

	SpawnPackageContents(OutTransform, bScatterAroundLocation);
	Destroy();
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

int32 AParcelActor::GetPackagingSpaceUnits() const
{
	return FMath::Max(1, ParcelConfig.PackagingSpaceUnits);
}

int32 AParcelActor::GetContentUnitTotal() const
{
	if (bIsPackaged && PackageContents.Num() > 0)
	{
		UDA_ParcelData* DataAsset = PackageParcelDataAsset ? PackageParcelDataAsset.Get() : ParcelDataAsset;
		if (DataAsset)
		{
			int32 Total = 0;
			for (const FParcelPackageContent& Entry : PackageContents)
			{
				if (Entry.Count <= 0 || Entry.ParcelRowName == NAME_None) continue;
				FParcelConfig ContentConfig;
				if (DataAsset->GetParcelConfigByName(Entry.ParcelRowName, ContentConfig))
				{
					Total += FMath::Max(1, ContentConfig.PackagingSpaceUnits) * Entry.Count;
				}
			}
			if (Total > 0) return Total;
		}
	}
	return GetPackagingSpaceUnits();
}

void AParcelActor::GetContentUnitsByTag(TMap<FGameplayTag, int32>& OutUnitsByTag) const
{
	OutUnitsByTag.Reset();

	if (bIsPackaged && PackageContents.Num() > 0)
	{
		UDA_ParcelData* DataAsset = PackageParcelDataAsset ? PackageParcelDataAsset.Get() : ParcelDataAsset;
		if (DataAsset)
		{
			for (const FParcelPackageContent& Entry : PackageContents)
			{
				if (Entry.Count <= 0 || Entry.ParcelRowName == NAME_None)
				{
					continue;
				}

				FParcelConfig ContentConfig;
				if (!DataAsset->GetParcelConfigByName(Entry.ParcelRowName, ContentConfig) || !ContentConfig.ParcelTag.IsValid())
				{
					continue;
				}

				OutUnitsByTag.FindOrAdd(ContentConfig.ParcelTag) += FMath::Max(1, ContentConfig.PackagingSpaceUnits) * Entry.Count;
			}
		}
	}

	if (OutUnitsByTag.Num() == 0 && ParcelConfig.ParcelTag.IsValid())
	{
		OutUnitsByTag.Add(ParcelConfig.ParcelTag, GetPackagingSpaceUnits());
	}
}

int32 AParcelActor::GetContentValueTotal() const
{
	if (bIsPackaged && PackageContents.Num() > 0)
	{
		UDA_ParcelData* DataAsset = PackageParcelDataAsset ? PackageParcelDataAsset.Get() : ParcelDataAsset;
		if (DataAsset)
		{
			int32 Total = 0;
			for (const FParcelPackageContent& Entry : PackageContents)
			{
				if (Entry.Count <= 0 || Entry.ParcelRowName == NAME_None) continue;
				FParcelConfig ContentConfig;
				if (DataAsset->GetParcelConfigByName(Entry.ParcelRowName, ContentConfig))
				{
					Total += FMath::Max(0, ContentConfig.BasePrice) * Entry.Count;
				}
			}
			if (Total > 0) return Total;
		}
	}
	return GetParcelPrice();
}

int32 AParcelActor::GetContentUnitsForTag(FGameplayTag RequiredTag) const
{
	if (!RequiredTag.IsValid())
	{
		return GetContentUnitTotal();
	}
	if (bIsPackaged && PackageContents.Num() > 0)
	{
		UDA_ParcelData* DataAsset = PackageParcelDataAsset ? PackageParcelDataAsset.Get() : ParcelDataAsset;
		if (DataAsset)
		{
			int32 Total = 0;
			for (const FParcelPackageContent& Entry : PackageContents)
			{
				if (Entry.Count <= 0 || Entry.ParcelRowName == NAME_None) continue;
				FParcelConfig ContentConfig;
				if (DataAsset->GetParcelConfigByName(Entry.ParcelRowName, ContentConfig))
				{
					if (ContentConfig.ParcelTag == RequiredTag)
					{
						Total += FMath::Max(1, ContentConfig.PackagingSpaceUnits) * Entry.Count;
					}
				}
			}
			return Total;
		}
	}
	// 언팩 파슬: 단일 아이템, ParcelTag 비교
	if (ParcelConfig.ParcelTag == RequiredTag)
	{
		return GetPackagingSpaceUnits();
	}
	return 0;
}

int32 AParcelActor::GetContentValueForTag(FGameplayTag RequiredTag) const
{
	if (!RequiredTag.IsValid())
	{
		return GetContentValueTotal();
	}
	if (bIsPackaged && PackageContents.Num() > 0)
	{
		UDA_ParcelData* DataAsset = PackageParcelDataAsset ? PackageParcelDataAsset.Get() : ParcelDataAsset;
		if (DataAsset)
		{
			int32 Total = 0;
			for (const FParcelPackageContent& Entry : PackageContents)
			{
				if (Entry.Count <= 0 || Entry.ParcelRowName == NAME_None) continue;
				FParcelConfig ContentConfig;
				if (DataAsset->GetParcelConfigByName(Entry.ParcelRowName, ContentConfig))
				{
					if (ContentConfig.ParcelTag == RequiredTag)
					{
						Total += FMath::Max(0, ContentConfig.BasePrice) * Entry.Count;
					}
				}
			}
			return Total;
		}
	}
	if (ParcelConfig.ParcelTag == RequiredTag)
	{
		return GetParcelPrice();
	}
	return 0;
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
	bool bUsedOnGate = false;

	// 게이트 타겟이 있으면 우선 해제 시도
	if (ABlasterCharacter* BlasterUser = Cast<ABlasterCharacter>(User))
	{
		if (UInteractionComponent* InteractionComp = BlasterUser->GetInteractionComponent())
		{
			if (AActor* Target = InteractionComp->GetCurrentTarget())
			{
				if (AGateActor* Gate = Cast<AGateActor>(Target))
				{
					bSuccess = Gate->TryUseItemWithGate(this, User);
					bUsedOnGate = bSuccess;
				}
			}
		}
	}

	switch (ItemType)
	{
	case EItemType::Key:
		// 열쇠 사용 - 블루프린트에서 구현 가능하도록 이벤트 브로드캐스트
		if (!bSuccess)
		{
			bSuccess = true;
		}
		OnItemUsed.Broadcast(User, ItemType);
		break;

	case EItemType::Tool:
		// 도구 사용 - 시스템 파훼 등
		if (!bSuccess)
		{
			bSuccess = true;
		}
		OnItemUsed.Broadcast(User, ItemType);
		break;

	case EItemType::Consumable:
		// 소비 아이템 사용
		if (!bSuccess)
		{
			bSuccess = true;
		}
		OnItemUsed.Broadcast(User, ItemType);
		break;

	default:
		// 다른 타입은 블루프린트에서 처리
		if (!bSuccess)
		{
			bSuccess = true;
		}
		OnItemUsed.Broadcast(User, ItemType);
		break;
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Item used - Type: %s, User: %s, Success: %s"),
			*UEnum::GetValueAsString(ItemType),
			User ? *User->GetName() : TEXT("None"),
			bSuccess ? TEXT("Yes") : TEXT("No"));
	}

	// Special item hook (BP can drive story/effects)
	if (bSuccess && (bIsSpecialItem || SpecialItemTags.Num() > 0))
	{
		BP_OnSpecialItemUsed(User);
	}

	// 소비/내구도 처리
	if (bSuccess)
	{
		if (ItemData.bConsumedOnUse || ItemData.ConsumePolicy == EItemConsumePolicy::ConsumeOnce)
		{
			Destroy();
		}
		else if (ItemData.ConsumePolicy == EItemConsumePolicy::DurabilityReduction && ItemData.DurabilityConsumeValue > 0.0f && ParcelStateComponent)
		{
			ParcelStateComponent->ApplyDamage(ItemData.DurabilityConsumeValue, TEXT("ItemUse"));
		}
	}

	return bSuccess;
}

void AParcelActor::BeginLeakLoop()
{
	if (!ParcelDataAsset || ActiveLoopAudioComponent || ActiveLoopVfxComponent)
	{
		return; // Already active or no data asset
	}

	FVector SpawnLocation = GetActorLocation();

	// Spawn loop audio component
	if (USoundBase* LoopSound = UParcelAVLibrary::ResolveSound(ParcelConfig, ParcelDataAsset, FName("Leak_Loop")))
	{
		ActiveLoopAudioComponent = UGameplayStatics::SpawnSoundAttached(
			LoopSound,
			MeshComponent,
			NAME_None,
			FVector::ZeroVector,
			EAttachLocation::KeepWorldPosition,
			false,
			1.0f,
			1.0f,
			0.0f,
			nullptr,
			nullptr,
			true
		);
	}

	// Spawn loop VFX component
	// VFX Map: Use "Spill" key for loop (unified with Spill_Start)
	if (UNiagaraSystem* LoopVfx = UParcelAVLibrary::ResolveVfx(ParcelConfig, ParcelDataAsset, FName("Spill")))
	{
		ActiveLoopVfxComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			LoopVfx,
			MeshComponent,
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepWorldPosition,
			true
		);
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Leak loop started"));
	}
}

void AParcelActor::EndLeakLoop()
{
	if (ActiveLoopAudioComponent)
	{
		ActiveLoopAudioComponent->Stop();
		ActiveLoopAudioComponent->DestroyComponent();
		ActiveLoopAudioComponent = nullptr;
	}

	if (ActiveLoopVfxComponent)
	{
		ActiveLoopVfxComponent->DestroyComponent();
		ActiveLoopVfxComponent = nullptr;
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Leak loop ended"));
	}
}
