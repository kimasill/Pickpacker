// Fill out your copyright notice in the Description page of Project Settings.

#include "StationActor.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/DataAssets/DA_LabelRuleData.h"
#include "Blaster/GameState/PickpackerGameState.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

AStationActor::AStationActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create components
	StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationMesh"));
	RootComponent = StationMesh;

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(RootComponent);
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	AreaSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AreaSphere"));
	AreaSphere->SetupAttachment(RootComponent);
	AreaSphere->SetSphereRadius(200.0f);
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AreaSphere->SetCollisionObjectType(ECC_WorldDynamic);
	AreaSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	AreaSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	StationWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("StationWidget"));
	StationWidget->SetupAttachment(RootComponent);
	StationWidget->SetWidgetSpace(EWidgetSpace::Screen);
	StationWidget->SetDrawAtDesiredSize(true);

	// Initialize variables
	StationType = EStationType::Unknown;
	CurrentParcel = nullptr;
	bIsProcessing = false;
	bIsDisabled = false;
	ProcessingStartTime = 0.0f;
	ProcessingDuration = 0.0f;
	ProcessingProgress = 0.0f;
}

void AStationActor::BeginPlay()
{
	Super::BeginPlay();

	// Store original materials
	if (StationMesh)
	{
		int32 NumMaterials = StationMesh->GetNumMaterials();
		OriginalMaterials.Empty();
		for (int32 i = 0; i < NumMaterials; ++i)
		{
			OriginalMaterials.Add(StationMesh->GetMaterial(i));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[StationActor] Station initialized: %s"), *GetName());
}

void AStationActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsProcessing)
	{
		UpdateProcessingProgress(DeltaTime);
	}
}

void AStationActor::InitializeStation(const FStationConfig& Config)
{
	StationConfig = Config;
	StationType = Config.StationType;

	UE_LOG(LogTemp, Log, TEXT("[StationActor] Station initialized: %s (Type: %d)"), 
		*Config.StationName, (int32)StationType);
}

bool AStationActor::StartProcessingParcel(AParcelActor* Parcel)
{
	if (!Parcel || !IsAvailable())
	{
		UE_LOG(LogTemp, Warning, TEXT("[StationActor] Cannot start processing - Parcel: %s, Available: %s"), 
			Parcel ? TEXT("Valid") : TEXT("Null"), IsAvailable() ? TEXT("Yes") : TEXT("No"));
		return false;
	}

	// Check if parcel is compatible with this station
	if (!StationConfig.RequiredInputTags.HasAll(Parcel->GetParcelTags()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[StationActor] Parcel not compatible with station requirements"));
		return false;
	}

	CurrentParcel = Parcel;
	bIsProcessing = true;
	ProcessingStartTime = GetWorld()->GetTimeSeconds();
	ProcessingDuration = StationConfig.BaseProcessingTime * StationConfig.DifficultyMultiplier;
	ProcessingProgress = 0.0f;

	UE_LOG(LogTemp, Log, TEXT("[StationActor] Started processing parcel: %s (Duration: %.2f)"), 
		*Parcel->GetName(), ProcessingDuration);

	// Broadcast events
	OnProcessingStarted.Broadcast(Parcel);
	OnProcessingStartedBP(Parcel);

	return true;
}

bool AStationActor::CompleteProcessing(const FGameplayTagContainer& OutputTags)
{
	if (!bIsProcessing || !CurrentParcel)
	{
		UE_LOG(LogTemp, Warning, TEXT("[StationActor] Cannot complete processing - Not processing or no parcel"));
		return false;
	}

	// Validate processing result
	bool bSuccess = ValidateProcessingResult(OutputTags);

	// Calculate suspicion points
	float SuspicionPoints = 0.0f;
	if (!bSuccess)
	{
		SuspicionPoints = StationConfig.SuspicionPerError;
	}

	// Add suspicion to game state
	if (UWorld* World = GetWorld())
	{
		if (APickpackerGameState* GameState = World->GetGameState<APickpackerGameState>())
		{
			GameState->AddTeamSuspicion(SuspicionPoints);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[StationActor] Processing completed: %s (Success: %s, Suspicion: %.2f)"), 
		*CurrentParcel->GetName(), bSuccess ? TEXT("Yes") : TEXT("No"), SuspicionPoints);

	// Broadcast events
	OnProcessingCompleted.Broadcast(CurrentParcel, bSuccess);
	OnProcessingCompletedBP(CurrentParcel, bSuccess);

	// Reset processing state
	CurrentParcel = nullptr;
	bIsProcessing = false;
	ProcessingProgress = 0.0f;

	return bSuccess;
}

void AStationActor::CancelProcessing()
{
	if (!bIsProcessing || !CurrentParcel)
	{
		UE_LOG(LogTemp, Warning, TEXT("[StationActor] Cannot cancel processing - Not processing or no parcel"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[StationActor] Processing cancelled: %s"), *CurrentParcel->GetName());

	// Broadcast events
	OnProcessingCancelled.Broadcast(CurrentParcel);
	OnProcessingCancelledBP(CurrentParcel);

	// Reset processing state
	CurrentParcel = nullptr;
	bIsProcessing = false;
	ProcessingProgress = 0.0f;
}

float AStationActor::GetProcessingProgress() const
{
	if (!bIsProcessing)
	{
		return 0.0f;
	}

	return FMath::Clamp(ProcessingProgress, 0.0f, 1.0f);
}

float AStationActor::GetRemainingTime() const
{
	if (!bIsProcessing)
	{
		return 0.0f;
	}

	float ElapsedTime = GetWorld()->GetTimeSeconds() - ProcessingStartTime;
	return FMath::Max(0.0f, ProcessingDuration - ElapsedTime);
}

void AStationActor::SetStationEnabled(bool bEnabled)
{
	if (bIsDisabled == !bEnabled)
	{
		return; // No change
	}

	bIsDisabled = !bEnabled;

	UE_LOG(LogTemp, Log, TEXT("[StationActor] Station %s: %s"), 
		*GetName(), bEnabled ? TEXT("Enabled") : TEXT("Disabled"));

	// Broadcast events
	OnStationStateChanged.Broadcast(bEnabled);
	OnStationStateChangedBP(bEnabled);
}

void AStationActor::UpdateProcessingProgress(float DeltaTime)
{
	if (!bIsProcessing)
	{
		return;
	}

	float ElapsedTime = GetWorld()->GetTimeSeconds() - ProcessingStartTime;
	ProcessingProgress = FMath::Clamp(ElapsedTime / ProcessingDuration, 0.0f, 1.0f);

	// Check if processing is complete
	if (ProcessingProgress >= 1.0f)
	{
		// Auto-complete with default output tags
		FGameplayTagContainer DefaultOutputTags = StationConfig.OutputTags;
		CompleteProcessing(DefaultOutputTags);
	}
}

bool AStationActor::ValidateProcessingResult(const FGameplayTagContainer& OutputTags) const
{
	// Get label rule data from game state
	if (UWorld* World = GetWorld())
	{
		if (APickpackerGameState* GameState = World->GetGameState<APickpackerGameState>())
		{
			if (UDA_LevelVariant* LevelVariant = GameState->GetLevelVariant())
			{
				if (UDA_LabelRuleData* LabelRuleData = LevelVariant->LabelRuleData)
				{
					// Find matching label rule
					for (const FLabelRule& Rule : LabelRuleData->LabelRules)
					{
						if (Rule.RequiredInputTags.HasAll(CurrentParcel->GetParcelTags()))
						{
							// Validate based on rule type
							switch (Rule.RuleType)
							{
							case ELabelRuleType::ExactMatch:
								return Rule.ExpectedOutputTags.HasAll(OutputTags) && 
									   OutputTags.HasAll(Rule.ExpectedOutputTags);

							case ELabelRuleType::PartialMatch:
								return Rule.ExpectedOutputTags.HasAny(OutputTags);

							case ELabelRuleType::RangeMatch:
								// For range match, we'd need custom logic
								return true; // Placeholder

							case ELabelRuleType::CustomRule:
								// Custom validation logic would go here
								return true; // Placeholder

							default:
								return false;
							}
						}
					}
				}
			}
		}
	}

	// Default validation: check if output tags match expected tags
	return StationConfig.OutputTags.HasAll(OutputTags) && 
		   OutputTags.HasAll(StationConfig.OutputTags);
}

// InteractableInterface Implementation
bool AStationActor::OnInteract_Implementation(ACharacter* Interactor)
{
	if (!Interactor || !CanInteract_Implementation(Interactor))
	{
		return false;
	}

	// Check if player is carrying a parcel
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Interactor);
	if (!BlasterCharacter)
	{
		return false;
	}

	UInteractionComponent* InteractionComponent = BlasterCharacter->GetInteractionComponent();
	if (!InteractionComponent)
	{
		return false;
	}

	AParcelActor* CarriedParcel = InteractionComponent->GetCarriedParcel();
	if (CarriedParcel && IsAvailable())
	{
		// Start processing the parcel
		return StartProcessingParcel(CarriedParcel);
	}

	return false;
}

bool AStationActor::CanInteract_Implementation(ACharacter* Interactor) const
{
	if (!Interactor)
	{
		return false;
	}

	// Check if station is available
	if (!IsAvailable())
	{
		return false;
	}

	// Check if player is carrying a parcel
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Interactor);
	if (!BlasterCharacter)
	{
		return false;
	}

	UInteractionComponent* InteractionComponent = BlasterCharacter->GetInteractionComponent();
	if (!InteractionComponent)
	{
		return false;
	}

	AParcelActor* CarriedParcel = InteractionComponent->GetCarriedParcel();
	if (!CarriedParcel)
	{
		return false;
	}

	// Check if parcel is compatible
	return StationConfig.RequiredInputTags.HasAll(CarriedParcel->GetParcelTags());
}

FText AStationActor::GetInteractText_Implementation() const
{
	if (bIsProcessing)
	{
		return FText::FromString(TEXT("Processing..."));
	}

	if (bIsDisabled)
	{
		return FText::FromString(TEXT("Station Disabled"));
	}

	return FText::FromString(FString::Printf(TEXT("Press E to Use %s"), *StationConfig.StationName));
}

void AStationActor::StartHighlight_Implementation()
{
	if (!StationMesh)
	{
		return;
	}

	// Use custom depth for highlight
	StationMesh->SetRenderCustomDepth(true);
	StationMesh->SetCustomDepthStencilValue(252);
}

void AStationActor::EndHighlight_Implementation()
{
	if (!StationMesh)
	{
		return;
	}

	// Disable custom depth
	StationMesh->SetRenderCustomDepth(false);
}
