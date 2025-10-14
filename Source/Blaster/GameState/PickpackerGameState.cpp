// Fill out your copyright notice in the Description page of Project Settings.

#include "PickpackerGameState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

APickpackerGameState::APickpackerGameState()
{
	PrimaryActorTick.bCanEverTick = false;
	
	// Initialize with default seed set
	SeedSet = FSeedSet();
}

void APickpackerGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APickpackerGameState, SeedSet);
	DOREPLIFETIME(APickpackerGameState, bDungeonGenerated);
}

void APickpackerGameState::BeginPlay()
{
	Super::BeginPlay();

	// Cache PCG subsystem reference
	PCGDungeonSubsystem = GetWorld()->GetSubsystem<UPCGDungeonSubSystem>();
}

void APickpackerGameState::SetSeedSet(const FSeedSet& NewSeedSet)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameState] Only server can set seed set"));
		return;
	}

	SeedSet = NewSeedSet;
	
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Seed set updated - Seed: %d, MissionId: %s"), 
		SeedSet.Seed, *SeedSet.MissionId);
}

void APickpackerGameState::OnRep_SeedSet()
{
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Seed set replicated to client - Seed: %d, MissionId: %s"), 
		SeedSet.Seed, *SeedSet.MissionId);

	// Notify clients that seed set has been updated
	// This can be used to trigger UI updates or other client-side logic
}

void APickpackerGameState::OnRep_DungeonGenerated()
{
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Dungeon generation status replicated - Generated: %s"), 
		bDungeonGenerated ? TEXT("True") : TEXT("False"));

	// Notify clients that dungeon generation is complete
	// This can be used to enable gameplay or show completion UI
}

UPCGDungeonSubSystem* APickpackerGameState::GetPCGDungeonSubsystem()
{
	if (!PCGDungeonSubsystem && GetWorld())
	{
		PCGDungeonSubsystem = GetWorld()->GetSubsystem<UPCGDungeonSubSystem>();
	}
	return PCGDungeonSubsystem;
}

