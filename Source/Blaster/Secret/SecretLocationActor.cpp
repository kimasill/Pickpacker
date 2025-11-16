// Fill out your copyright notice in the Description page of Project Settings.

#include "SecretLocationActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/Components/PlayerInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

ASecretLocationActor::ASecretLocationActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// Create components
	LocationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LocationMesh"));
	RootComponent = LocationMesh;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(RootComponent);
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	DiscoveryWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("DiscoveryWidget"));
	DiscoveryWidget->SetupAttachment(RootComponent);
	DiscoveryWidget->SetWidgetSpace(EWidgetSpace::Screen);
	DiscoveryWidget->SetDrawAtDesiredSize(true);
	DiscoveryWidget->SetVisibility(false);

	// Initialize values
	bIsDiscovered = false;
	bRequiresItem = false;
	bHiddenFromSurveillance = true;
}

void ASecretLocationActor::BeginPlay()
{
	Super::BeginPlay();

	// Setup overlap events
	if (TriggerBox)
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ASecretLocationActor::OnPlayerEnter);
		TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ASecretLocationActor::OnPlayerExit);
	}

	// Spawn initial items
	if (HasAuthority())
	{
		SpawnInitialItems();
	}
}

void ASecretLocationActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ASecretLocationActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASecretLocationActor, bIsDiscovered);
	DOREPLIFETIME(ASecretLocationActor, SpawnedItems);
}

void ASecretLocationActor::Discover()
{
	if (bIsDiscovered)
	{
		return;
	}

	bIsDiscovered = true;

	if (DiscoveryWidget)
	{
		DiscoveryWidget->SetVisibility(true);
	}

	OnLocationDiscovered.Broadcast(this);
}

bool ASecretLocationActor::CanAccess(ACharacter* Player) const
{
	if (!Player)
	{
		return false;
	}

	if (!bRequiresItem)
	{
		return true;
	}

	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Player);
	if (!BlasterCharacter)
	{
		return false;
	}

	UPlayerInventoryComponent* InventoryComponent = BlasterCharacter->GetPlayerInventoryComponent();
	if (!InventoryComponent)
	{
		return false;
	}

	return InventoryComponent->HasItemType(RequiredItemType);
}

bool ASecretLocationActor::IsPlayerInside(ACharacter* Player) const
{
	if (!Player)
	{
		return false;
	}

	return PlayersInside.Contains(Player);
}

AParcelActor* ASecretLocationActor::SpawnItem(EItemType ItemType, const FItemData& ItemData)
{
	if (!ItemParcelClass || !HasAuthority())
	{
		return nullptr;
	}

	// Find available spawn location
	FVector SpawnLocation = GetActorLocation();
	if (ItemSpawnLocations.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, ItemSpawnLocations.Num() - 1);
		SpawnLocation = GetActorLocation() + ItemSpawnLocations[RandomIndex];
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AParcelActor* NewItem = GetWorld()->SpawnActor<AParcelActor>(ItemParcelClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
	if (NewItem)
	{
		// Set as item
		NewItem->SetItemData(ItemData);
		SpawnedItems.Add(NewItem);
	}

	return NewItem;
}

void ASecretLocationActor::OnPlayerEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character)
	{
		return;
	}

	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Character);
	if (!BlasterCharacter)
	{
		return;
	}

	// Check if player can access
	if (!CanAccess(Character))
	{
		return;
	}

	// Discover location if not already discovered
	if (!bIsDiscovered)
	{
		Discover();
	}

	// Add to players inside
	if (!PlayersInside.Contains(Character))
	{
		PlayersInside.Add(Character);
		OnPlayerEntered.Broadcast(this, Character);
	}
}

void ASecretLocationActor::OnPlayerExit(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character)
	{
		return;
	}

	// Remove from players inside
	if (PlayersInside.Remove(Character) > 0)
	{
		OnPlayerExited.Broadcast(this, Character);
	}
}

void ASecretLocationActor::SpawnInitialItems()
{
	for (const FItemData& ItemData : ItemsToSpawn)
	{
		SpawnItem(ItemData.ItemType, ItemData);
	}
}

