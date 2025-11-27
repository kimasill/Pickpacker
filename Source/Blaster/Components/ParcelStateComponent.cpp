// Fill out your copyright notice in the Description page of Project Settings.

#include "ParcelStateComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Blaster/DataAssets/DA_ParcelData.h"
#include "GameplayTagsManager.h"

namespace ParcelClassification
{
	static FGameplayTag Standard()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Parcel-Classification.Standard"), false);
		return Tag;
	}

	static FGameplayTag Fragile()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Parcel-Classification.Fragile"), false);
		return Tag;
	}

	static FGameplayTag Contraband()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Parcel-Classification.Contraband"), false);
		return Tag;
	}
}

UParcelStateComponent::UParcelStateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	// Initialize default state
	CurrentState = FParcelState();
	ParcelConfig = FParcelConfig();
	LastDamageTime = 0.0f;
	LastDamageSource = TEXT("");
	InstabilityTimer = 0.0f;
	
	// Two-person carry settings
	TwoPersonStabilityBonus = 0.3f;
	TwoPersonMovementBonus = 0.15f;
	bIsTwoPersonCarry = false;
	
	bEnableDebugLogging = true;
}

void UParcelStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UParcelStateComponent, CurrentState);
	DOREPLIFETIME(UParcelStateComponent, InternalItemData);
	DOREPLIFETIME(UParcelStateComponent, InternalItemData);
}

void UParcelStateComponent::BeginPlay()
{
	Super::BeginPlay();

	// Ensure parcel classification has a default
	if (!ParcelConfig.ClassificationTag.IsValid())
	{
		ParcelConfig.ClassificationTag = FGameplayTag::RequestGameplayTag(TEXT("Parcel-Classification.Standard"), false);
	}

	// Set initial state
	CurrentState.Durability = ParcelConfig.BaseDurability;
	CurrentState.Weight = ParcelConfig.BaseWeight;
	CurrentState.Instability = ParcelConfig.InstabilityFactor;

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelStateComponent] Initialized - Classification: %s, Durability: %.2f, Weight: %.2f, Instability: %.2f"),
			*ParcelConfig.ClassificationTag.ToString(),
			CurrentState.Durability, CurrentState.Weight, CurrentState.Instability);
	}
}

void UParcelStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Update instability for contraband parcels
	if (IsContrabandClassification())
	{
		UpdateInstability(DeltaTime);		
	}
}

void UParcelStateComponent::InitializeParcel(const FParcelConfig& Config)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ParcelStateComponent] Only server can initialize parcel"));
		return;
	}

	ParcelConfig = Config;
	CurrentState.Durability = Config.BaseDurability;
	CurrentState.Weight = Config.BaseWeight;
	CurrentState.Instability = Config.InstabilityFactor;

	if (!ParcelConfig.ClassificationTag.IsValid())
	{
		ParcelConfig.ClassificationTag = ParcelClassification::Standard();
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelStateComponent] Parcel initialized - Classification: %s, Durability: %.2f, Weight: %.2f, Instability: %.2f"),
			*ParcelConfig.ClassificationTag.ToString(),
			Config.BaseDurability, Config.BaseWeight, Config.InstabilityFactor);
	}
}

void UParcelStateComponent::ApplyDamage(float DamageAmount, const FString& DamageSource)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ParcelStateComponent] Only server can apply damage"));
		return;
	}

	if (IsBroken())
	{
		return; // Already broken
	}

	float CalculatedDamage = CalculateDamage(DamageAmount, DamageSource);
	float OldDurability = CurrentState.Durability;
	
	CurrentState.Durability = FMath::Max(0.0f, CurrentState.Durability - CalculatedDamage);
	LastDamageTime = GetWorld()->GetTimeSeconds();
	LastDamageSource = DamageSource;

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelStateComponent] Damage applied - Classification: %s, Source: %s, Amount: %.2f, New Durability: %.2f"),
			*ParcelConfig.ClassificationTag.ToString(), *DamageSource, CalculatedDamage, CurrentState.Durability);
	}

	// Broadcast durability change
	OnDurabilityChanged.Broadcast(OldDurability, CurrentState.Durability);
	OnParcelStateChanged.Broadcast(CurrentState);

	if (IsBroken())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ParcelStateComponent] Parcel broken! Classification: %s, Source: %s"),
			*ParcelConfig.ClassificationTag.ToString(), *DamageSource);
	}
}

void UParcelStateComponent::ApplyImpactDamage(float ImpactForce, const FString& ImpactSource)
{
	if (!IsFragileClassification())
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Log, TEXT("[ParcelStateComponent] Impact damage ignored - Classification: %s"),
				*ParcelConfig.ClassificationTag.ToString());
		}
		return; // Only fragile parcels take impact damage
	}

	if (ImpactForce > ImpactDamageThreshold)
	{
		float DamageAmount = (ImpactForce - ImpactDamageThreshold) * 0.1f * FragileImpactMultiplier;
		ApplyDamage(DamageAmount, FString::Printf(TEXT("Impact_%s"), *ImpactSource));
		
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Log, TEXT("[ParcelStateComponent] Impact damage applied - Force: %.2f, Threshold: %.2f, Damage: %.2f, Source: %s"),
				ImpactForce, ImpactDamageThreshold, DamageAmount, *ImpactSource);
		}
	}
	else
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, VeryVerbose, TEXT("[ParcelStateComponent] Impact damage below threshold - Force: %.2f, Threshold: %.2f"),
				ImpactForce, ImpactDamageThreshold);
		}
	}
}

void UParcelStateComponent::UpdateInstability(float DeltaTime)
{
	if (!IsContrabandClassification())
	{
		return;
	}

	InstabilityTimer += DeltaTime;
	
	// Increase instability over time
	if (InstabilityTimer >= 1.0f) // Update every second
	{
		float OldInstability = CurrentState.Instability;
		CurrentState.Instability += UnstableDecayRate;
		InstabilityTimer = 0.0f;

		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Log, TEXT("[ParcelStateComponent] Instability updated - Old: %.2f, New: %.2f, DecayRate: %.2f"),
				OldInstability, CurrentState.Instability, UnstableDecayRate);
		}

		OnInstabilityChanged.Broadcast(OldInstability, CurrentState.Instability);
		OnParcelStateChanged.Broadcast(CurrentState);

		// Check if instability has reached failure threshold
		if (CurrentState.Instability >= InstabilityFailureThreshold)
		{
			if (bEnableDebugLogging)
			{
				UE_LOG(LogTemp, Warning, TEXT("[ParcelStateComponent] Instability failure threshold reached! Value: %.2f, Threshold: %.2f"),
					CurrentState.Instability, InstabilityFailureThreshold);
			}
			// TODO: Handle instability failure (parcel breaks, mission fails, etc.)
		}
	}
}

void UParcelStateComponent::SetAttachedState(bool bAttached, const FName& SocketId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ParcelStateComponent] Only server can set attached state"));
		return;
	}

	CurrentState.bIsAttached = bAttached;
	CurrentState.SocketId = SocketId;

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelStateComponent] Attached state changed - Attached: %s, Socket: %s"),
			bAttached ? TEXT("True") : TEXT("False"), *SocketId.ToString());
	}

	OnParcelStateChanged.Broadcast(CurrentState);
}

void UParcelStateComponent::OnRep_ParcelState()
{
	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelStateComponent] Parcel state replicated - Durability: %.2f, Weight: %.2f, Instability: %.2f, Attached: %s"),
			CurrentState.Durability, CurrentState.Weight, CurrentState.Instability,
			CurrentState.bIsAttached ? TEXT("True") : TEXT("False"));
	}

	OnParcelStateChanged.Broadcast(CurrentState);
}

void UParcelStateComponent::OnRep_Durability()
{
	OnDurabilityChanged.Broadcast(0.0f, CurrentState.Durability); // We don't have old value in replication
}

void UParcelStateComponent::OnRep_Instability()
{
	OnInstabilityChanged.Broadcast(0.0f, CurrentState.Instability); // We don't have old value in replication
}

void UParcelStateComponent::OnRep_InternalItemData()
{
	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[ParcelStateComponent] Internal item data replicated - %s"),
			*InternalItemData.ItemName);
	}
}

float UParcelStateComponent::CalculateDamage(float BaseDamage, const FString& DamageSource)
{
	float Multiplier = 1.0f;

	if (IsFragileClassification())
	{
		Multiplier = 2.0f;
	}
	else if (IsContrabandClassification())
	{
		Multiplier = 1.5f;
	}

	return BaseDamage * Multiplier;
}

float UParcelStateComponent::GetMovementSpeedMultiplier() const
{
	return 1.0f;
}

bool UParcelStateComponent::CanJump() const
{
	return true;
}

float UParcelStateComponent::GetInstabilityDecayRate() const
{
	if (IsContrabandClassification())
	{
		return UnstableDecayRate;
	}
	return 0.0f;
}

void UParcelStateComponent::ApplyTwoPersonCarryBonuses(bool bTwoPersonCarry)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ParcelStateComponent] Only server can apply two-person carry bonuses"));
		return;
	}

	this->bIsTwoPersonCarry = bTwoPersonCarry;

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelStateComponent] Two-person carry state changed - IsTwoPerson: %s"),
			bTwoPersonCarry ? TEXT("True") : TEXT("False"));
	}

	// Broadcast state change
	OnParcelStateChanged.Broadcast(CurrentState);
}

float UParcelStateComponent::GetEffectiveMovementSpeedMultiplier() const
{
	float BaseMultiplier = GetMovementSpeedMultiplier();
	
	if (bIsTwoPersonCarry)
	{
		// Apply two-person carry bonus to movement speed
		// For heavy parcels: 0.75 * (1 + 0.15) = 0.8625 (13.75% penalty instead of 25%)
		BaseMultiplier = BaseMultiplier * (1.0f + TwoPersonMovementBonus);
	}

	return BaseMultiplier;
}

void UParcelStateComponent::SetParcelState(const FParcelState& NewState)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ParcelStateComponent] Only server can set parcel state"));
		return;
	}

	CurrentState = NewState;

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[ParcelStateComponent] Parcel state set - Durability: %.2f, Weight: %.2f, Instability: %.2f"),
			CurrentState.Durability, CurrentState.Weight, CurrentState.Instability);
	}

	// Broadcast state change
	OnParcelStateChanged.Broadcast(CurrentState);
}

bool UParcelStateComponent::MatchesClassificationTag(const FGameplayTag& Tag) const
{
	return ParcelConfig.ClassificationTag.IsValid() && Tag.IsValid() && ParcelConfig.ClassificationTag.MatchesTag(Tag);
}

bool UParcelStateComponent::IsFragileClassification() const
{
	return MatchesClassificationTag(ParcelClassification::Fragile());
}

bool UParcelStateComponent::IsContrabandClassification() const
{
	return MatchesClassificationTag(ParcelClassification::Contraband());
}

void UParcelStateComponent::SetInternalItemData(const FItemData& ItemData)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		InternalItemData = ItemData;
		return;
	}

	InternalItemData = ItemData;
	OnRep_InternalItemData();
}
