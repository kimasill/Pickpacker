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
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Blaster/DataAssets/DA_ParcelData.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

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
		MeshComponent->SetSimulatePhysics(true);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
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

		// Update parcel state
		if (ParcelStateComponent)
		{
			ParcelStateComponent->SetAttachedState(true, SocketId);

			// Check if this is two-person carry
			bool bIsTwoPersonCarry = CarryPointsComponent && CarryPointsComponent->IsTwoPersonCarry();
			ParcelStateComponent->ApplyTwoPersonCarryBonuses(bIsTwoPersonCarry);
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

	// Detach from character mesh
	if (CurrentCarrier)
	{
		USkeletalMeshComponent* CharacterMesh = CurrentCarrier->GetMesh();
		if (CharacterMesh)
		{
			DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}
	}

	// Detach from carry points
	if (CarryPointsComponent && CurrentCarrier)
	{
		CarryPointsComponent->DetachFromSocket(CurrentCarrier, CurrentSocketId);
	}

	// Update parcel state
	if (ParcelStateComponent)
	{
		ParcelStateComponent->SetAttachedState(false, NAME_None);
		ParcelStateComponent->ApplyTwoPersonCarryBonuses(false); // No longer two-person carry
	}

	// Reset attachment state
	bIsAttached = false;
	CurrentSocketId = NAME_None;
	CurrentCarrier = nullptr;

	// Configure physics for drop
	ConfigureDropPhysics(Impulse);

	// Broadcast drop event
	Multicast_ParcelDropped(DropLocation);

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Server drop successful - Location: %s"),
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

    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Multicast attach - Carrier: %s, Socket: %s"),
            Carrier ? *Carrier->GetName() : TEXT("None"), *SocketId.ToString());
    }
}

void AParcelActor::Multicast_ParcelDropped_Implementation(FVector DropLocation)
{
    OnParcelDropped.Broadcast(DropLocation);

    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("[ParcelActor] Multicast drop - Location: %s"),
            *DropLocation.ToString());
    }
}

void AParcelActor::NotifyBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	// 캐릭터 캡슐에서 발생한 Overlap을 캐릭터 BP가 넘겨줍니다.
    const APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn) return;

    // 로컬 클라만 위젯 토글
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
	if (!Interactor || !CanBeAttached())
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
}

void AParcelActor::EndHighlight_Implementation()
{
	if (!MeshComponent)
	{
		return;
	}
	MeshComponent->SetRenderCustomDepth(false);
}

void AParcelActor::ShowPickupWidget(bool bShowWidget)
{
	if (PickupWidget)
	{
		PickupWidget->SetVisibility(bShowWidget);
	}
}
