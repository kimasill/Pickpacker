// Fill out your copyright notice in the Description page of Project Settings.

#include "EscapeZoneActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/Components/PlayerInventoryComponent.h"
#include "Blaster/GameMode/PickpackerGameMode.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"

AEscapeZoneActor::AEscapeZoneActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// Create components
	ZoneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneMesh"));
	RootComponent = ZoneMesh;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(RootComponent);
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	StatusWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("StatusWidget"));
	StatusWidget->SetupAttachment(RootComponent);
	StatusWidget->SetWidgetSpace(EWidgetSpace::Screen);
	StatusWidget->SetDrawAtDesiredSize(true);

	// Initialize values
	bRequireAllPlayers = true;
}

void AEscapeZoneActor::BeginPlay()
{
	Super::BeginPlay();

	// Setup overlap events
	if (TriggerBox)
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AEscapeZoneActor::OnPlayerEnter);
		TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AEscapeZoneActor::OnPlayerExit);
	}
}

void AEscapeZoneActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AEscapeZoneActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEscapeZoneActor, EscapedPlayers);
}

bool AEscapeZoneActor::CanPlayerEscape(ACharacter* Player) const
{
	if (!Player)
	{
		return false;
	}

	// Check if already escaped
	if (EscapedPlayers.Contains(Player))
	{
		return false;
	}

	// Check requirements
	return CheckEscapeRequirements(Player);
}

bool AEscapeZoneActor::AttemptEscape(ACharacter* Player)
{
	if (!Player || !HasAuthority())
	{
		return false;
	}

	// Check if can escape
	if (!CanPlayerEscape(Player))
	{
		OnEscapeAttempted.Broadcast(Player, false);
		return false;
	}

	// Use required items
	if (!UseRequiredItems(Player))
	{
		OnEscapeAttempted.Broadcast(Player, false);
		return false;
	}

	// Process escape
	ProcessPlayerEscape(Player);
	OnEscapeAttempted.Broadcast(Player, true);

	return true;
}

float AEscapeZoneActor::GetEscapeProgress(ACharacter* Player) const
{
	if (!Player)
	{
		return 0.0f;
	}

	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Player);
	if (!BlasterCharacter)
	{
		return 0.0f;
	}

	UPlayerInventoryComponent* InventoryComponent = BlasterCharacter->GetPlayerInventoryComponent();
	if (!InventoryComponent)
	{
		return 0.0f;
	}

	// Calculate progress based on requirements
	int32 CompletedRequirements = 0;
	for (const FEscapeRequirement& Requirement : EscapeRequirements)
	{
		TArray<AParcelActor*> Items = InventoryComponent->GetItemsByType(Requirement.RequiredItemType);
		if (Items.Num() >= Requirement.RequiredCount)
		{
			CompletedRequirements++;
		}
	}

	if (EscapeRequirements.Num() == 0)
	{
		return 1.0f;
	}

	return static_cast<float>(CompletedRequirements) / static_cast<float>(EscapeRequirements.Num());
}

bool AEscapeZoneActor::AreAllPlayersEscaped() const
{
	if (!bRequireAllPlayers)
	{
		return EscapedPlayers.Num() > 0;
	}

	// Get all players in game
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	APickpackerGameMode* GameMode = Cast<APickpackerGameMode>(World->GetAuthGameMode());
	if (!GameMode)
	{
		return false;
	}

	// Count players
	int32 TotalPlayers = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (PC->GetPawn())
			{
				TotalPlayers++;
			}
		}
	}

	return EscapedPlayers.Num() >= TotalPlayers && TotalPlayers > 0;
}

void AEscapeZoneActor::OnPlayerEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
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

	if (!PlayersInZone.Contains(Character))
	{
		PlayersInZone.Add(Character);
	}

	// Auto-attempt escape if requirements are met
	if (CanPlayerEscape(Character))
	{
		AttemptEscape(Character);
	}
}

void AEscapeZoneActor::OnPlayerExit(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character)
	{
		return;
	}

	PlayersInZone.Remove(Character);
}

bool AEscapeZoneActor::CheckEscapeRequirements(ACharacter* Player) const
{
	if (!Player)
	{
		return false;
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

	// Check all requirements
	for (const FEscapeRequirement& Requirement : EscapeRequirements)
	{
		TArray<AParcelActor*> Items = InventoryComponent->GetItemsByType(Requirement.RequiredItemType);
		if (Items.Num() < Requirement.RequiredCount)
		{
			return false;
		}
	}

	return true;
}

bool AEscapeZoneActor::UseRequiredItems(ACharacter* Player)
{
	if (!Player)
	{
		return false;
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

	// Use required items
	for (const FEscapeRequirement& Requirement : EscapeRequirements)
	{
		if (Requirement.bMustBeUsed)
		{
			TArray<AParcelActor*> Items = InventoryComponent->GetItemsByType(Requirement.RequiredItemType);
			int32 UsedCount = 0;
			for (AParcelActor* Item : Items)
			{
				if (UsedCount >= Requirement.RequiredCount)
				{
					break;
				}

				if (Item && Item->IsUsable())
				{
					if (InventoryComponent->UseItem(Item))
					{
						UsedCount++;
					}
				}
			}

			if (UsedCount < Requirement.RequiredCount)
			{
				return false;
			}
		}
	}

	return true;
}

void AEscapeZoneActor::ProcessPlayerEscape(ACharacter* Player)
{
	if (!Player || !HasAuthority())
	{
		return;
	}

	// Add to escaped players
	if (!EscapedPlayers.Contains(Player))
	{
		EscapedPlayers.Add(Player);
		OnPlayerEscaped.Broadcast(Player);
	}

	// Check if all players escaped
	if (AreAllPlayersEscaped())
	{
		OnAllPlayersEscaped.Broadcast();

		// Notify game mode
		UWorld* World = GetWorld();
		if (World)
		{
			APickpackerGameMode* GameMode = Cast<APickpackerGameMode>(World->GetAuthGameMode());
			if (GameMode)
			{
				GameMode->OnAllPlayersEscaped();
			}
		}
	}
	else
	{
		// Notify game mode of individual escape
		UWorld* World = GetWorld();
		if (World)
		{
			APickpackerGameMode* GameMode = Cast<APickpackerGameMode>(World->GetAuthGameMode());
			if (GameMode)
			{
				GameMode->OnPlayerEscaped(Player);
			}
		}
	}
}

