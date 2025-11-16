// Fill out your copyright notice in the Description page of Project Settings.

#include "ParcelActor.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/StaticMeshComponent.h"
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
}

void AParcelActor::BeginPlay()
{
	Super::BeginPlay();

	// Initialize parcel with default config if unknown
	if (ParcelConfig.ParcelType == EParcelType::Unknown)
	{
		ParcelConfig.ParcelType = EParcelType::Fragile;
		ParcelConfig.BaseDurability = 100.0f;
		ParcelConfig.BaseWeight = 1.0f;
		ParcelConfig.InstabilityFactor = 0.0f;
	}

	// 포장되지 않은 상태면 아이템으로 설정
	if (!bIsPackaged)
	{
		bIsItem = true;
		
		// Contraband 타입은 기본적으로 사용 가능한 아이템으로 설정
		if (ParcelConfig.ParcelType == EParcelType::Contraband)
		{
			// 기본 아이템 데이터 설정 (블루프린트에서 오버라이드 가능)
			if (ItemType == EItemType::Unknown)
			{
				ItemType = EItemType::Key; // 기본값
				bIsUsable = true;
				ItemData.ItemType = ItemType;
				ItemData.ItemName = ParcelConfig.ParcelName.IsEmpty() ? TEXT("Contraband Item") : ParcelConfig.ParcelName;
				ItemData.bIsUsable = true;
			}
		}
	}

	// Initialize parcel state
	if (ParcelStateComponent)
	{
		ParcelStateComponent->InitializeParcel(ParcelConfig);
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
		UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Initialized - Type: %s, Location: %s"),
			*UEnum::GetValueAsString(ParcelConfig.ParcelType), *GetActorLocation().ToString());
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
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] Only server can initialize parcel"));
        return;
    }

    ParcelConfig = Config;
    
    if (ParcelStateComponent)
    {
        ParcelStateComponent->InitializeParcel(Config);
    }

    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Parcel initialized - Type: %s, Name: %s"),
            *UEnum::GetValueAsString(Config.ParcelType), *Config.ParcelName);
    }
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

EParcelType AParcelActor::GetParcelType() const
{
    if (ParcelStateComponent)
    {
        return ParcelStateComponent->GetParcelType();
    }
    return EParcelType::Unknown;
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
	UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] Parcel broken! Type: %s"),
		*UEnum::GetValueAsString(ParcelConfig.ParcelType));

	// Detach if attached
	if (bIsAttached)
	{
		RequestDrop(FVector::ZeroVector);
	}

	// Broadcast broken event
	OnParcelBroken.Broadcast(this);

	// TODO: Handle broken parcel (destroy, replace, etc.)
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
		EParcelType CurrentType = ParcelStateComponent->GetParcelType();
		
		HUDWidget->UpdateParcelState(CurrentState, CurrentType);
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

	bIsPackaged = bPackaged;
	
	// 포장되지 않은 상태 = 아이템
	bIsItem = !bPackaged;

	if (!MeshComponent)
	{
		return;
	}

	// 원본 메시 저장 (첫 포장 시)
	if (bPackaged && !OriginalMesh)
	{
		OriginalMesh = MeshComponent->GetStaticMesh();
	}

	// 메시 변경
	if (bPackaged)
	{
		// 포장 메시로 변경
		if (PackagedMesh)
		{
			MeshComponent->SetStaticMesh(PackagedMesh);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ParcelActor] PackagedMesh not set, cannot package"));
			bIsPackaged = false;
			bIsItem = true; // 포장 실패 시 아이템 상태 유지
			return;
		}
	}
	else
	{
		// 원본 메시로 복원
		if (OriginalMesh)
		{
			MeshComponent->SetStaticMesh(OriginalMesh);
		}
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

