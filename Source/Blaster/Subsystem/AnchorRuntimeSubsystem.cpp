// Fill out your copyright notice in the Description page of Project Settings.

#include "AnchorRuntimeSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameplayTagContainer.h"
#include "GameplayTagAssetInterface.h"
#include "Blaster/DataAssets/DA_LevelVariant.h"
#include "Components/ActorComponent.h"

UAnchorRuntimeSubsystem::UAnchorRuntimeSubsystem()
{
	bInitialized = false;
	CurrentSeed = 0;
	LevelVariantData = nullptr;
}

void UAnchorRuntimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	UE_LOG(LogTemp, Log, TEXT("[AnchorRuntimeSubsystem] Initialized"));
}

void UAnchorRuntimeSubsystem::Deinitialize()
{
	AllAnchorActors.Empty();
	AnchorsByTag.Empty();
	ActivationResults.Empty();
	ReplacementResults.Empty();
	
	Super::Deinitialize();
}

void UAnchorRuntimeSubsystem::InitializeAnchors(UDA_LevelVariant* InLevelVariantData, int32 Seed)
{
	if (!InLevelVariantData)
	{
		UE_LOG(LogTemp, Error, TEXT("[AnchorRuntimeSubsystem] LevelVariantData is null"));
		return;
	}

	this->LevelVariantData = InLevelVariantData;
	this->CurrentSeed = Seed;
	this->bInitialized = true;

	UE_LOG(LogTemp, Log, TEXT("[AnchorRuntimeSubsystem] Initialized with seed: %d"), Seed);

	// Scan and randomize anchors
	ScanAndRandomizeAnchors();
}

void UAnchorRuntimeSubsystem::ScanAndRandomizeAnchors()
{
	if (!bInitialized || !LevelVariantData)
	{
		UE_LOG(LogTemp, Error, TEXT("[AnchorRuntimeSubsystem] Not initialized or LevelVariantData is null"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[AnchorRuntimeSubsystem] World is null"));
		return;
	}

	// Only run on server
	if (!World->HasBegunPlay() || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AnchorRuntimeSubsystem] ScanAndRandomizeAnchors called on client or before BeginPlay"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[AnchorRuntimeSubsystem] Scanning and randomizing anchors..."));

	// Clear previous results
	AllAnchorActors.Empty();
	AnchorsByTag.Empty();
	ActivationResults.Empty();
	ReplacementResults.Empty();

	// Scan level for anchors
	ScanLevelForAnchors();

	// Perform randomization
	PerformRandomization();
	
	// Apply results
	ApplyRandomizationResults();

	UE_LOG(LogTemp, Log, TEXT("[AnchorRuntimeSubsystem] Anchor randomization complete. Found %d anchors, %d activated, %d replaced"), 
		AllAnchorActors.Num(), ActivationResults.Num(), ReplacementResults.Num());
}
void UAnchorRuntimeSubsystem::ScanLevelForAnchors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Find all actors that implement GameplayTagAssetInterface
	for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		AActor* Actor = *ActorItr;
		if (!Actor || !IsValid(Actor))
		{
			continue;
		}

		// Check if actor has gameplay tags (anchor tags)
		if (IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Actor))
		{
			FGameplayTagContainer ActorTags;
			TagInterface->GetOwnedGameplayTags(ActorTags);

			if (ActorTags.Num() > 0)
			{
				AllAnchorActors.Add(Actor);

				// Group by tags
				for (const FGameplayTag& Tag : ActorTags)
				{
					FTaggedActors& List = AnchorsByTag.FindOrAdd(Tag);
					List.Actors.Add(Actor);
				}

				UE_LOG(LogTemp, Verbose, TEXT("[AnchorRuntimeSubsystem] Found anchor actor: %s with tags: %s"), 
					*Actor->GetName(), *ActorTags.ToString());
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[AnchorRuntimeSubsystem] Scanned level: found %d anchor actors with %d unique tags"), 
		AllAnchorActors.Num(), AnchorsByTag.Num());
}

void UAnchorRuntimeSubsystem::PerformRandomization()
{
	if (!LevelVariantData)
	{
		return;
	}

	// Set random seed for deterministic results
	FMath::RandInit(CurrentSeed);

	// For now, just activate all anchors
	for (AActor* Anchor : AllAnchorActors)
	{
		if (Anchor && IsValid(Anchor))
		{
			ActivationResults.Add(Anchor, true);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[AnchorRuntimeSubsystem] Performed randomization with seed: %d"), CurrentSeed);
}

void UAnchorRuntimeSubsystem::ApplyRandomizationResults()
{
	// Apply activation results
	for (const auto& ActivationPair : ActivationResults)
	{
		AActor* Actor = ActivationPair.Key;
		bool bShouldActivate = ActivationPair.Value;

		if (Actor && IsValid(Actor))
		{
			Actor->SetActorHiddenInGame(!bShouldActivate);
			Actor->SetActorEnableCollision(bShouldActivate);
			Actor->SetActorTickEnabled(bShouldActivate);

			UE_LOG(LogTemp, Verbose, TEXT("[AnchorRuntimeSubsystem] %s actor: %s"), 
				bShouldActivate ? TEXT("Activated") : TEXT("Deactivated"), *Actor->GetName());
		}
	}

	// Apply replacement results
	for (const auto& ReplacementPair : ReplacementResults)
	{
		AActor* OriginalActor = ReplacementPair.Key;
		AActor* ReplacementActor = ReplacementPair.Value;

		if (OriginalActor && IsValid(OriginalActor) && ReplacementActor && IsValid(ReplacementActor))
		{
			// Hide original, show replacement
			OriginalActor->SetActorHiddenInGame(true);
			OriginalActor->SetActorEnableCollision(false);
			OriginalActor->SetActorTickEnabled(false);

			ReplacementActor->SetActorHiddenInGame(false);
			ReplacementActor->SetActorEnableCollision(true);
			ReplacementActor->SetActorTickEnabled(true);

			UE_LOG(LogTemp, Verbose, TEXT("[AnchorRuntimeSubsystem] Replaced actor: %s with: %s"), 
				*OriginalActor->GetName(), *ReplacementActor->GetName());
		}
	}
}

void UAnchorRuntimeSubsystem::GetAnchorsByTag(const FGameplayTag& AnchorTag, TArray<AActor*>& OutActors) const
{
	OutActors.Empty();

	if (const FTaggedActors* Found = AnchorsByTag.Find(AnchorTag))
	{
		for (AActor* Actor : Found->Actors)
		{
			OutActors.Add(Actor);
		}
	}
}
