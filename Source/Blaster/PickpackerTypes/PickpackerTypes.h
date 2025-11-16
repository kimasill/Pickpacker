// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PickpackerTypes.generated.h"

/**
 * Parcel state structure for replication
 */
USTRUCT(BlueprintType)
struct BLASTER_API FParcelState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Parcel State")
	float Durability = 100.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Parcel State")
	float Weight = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Parcel State")
	float Instability = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Parcel State")
	bool bIsAttached = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Parcel State")
	TArray<class APlayerState*> Carriers;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Parcel State")
	FName SocketId = NAME_None;

	FParcelState()
	{
		Durability = 100.0f;
		Weight = 1.0f;
		Instability = 0.0f;
		bIsAttached = false;
		SocketId = NAME_None;
	}
};

/**
 * Parcel types enum (renamed to avoid conflict with DA_ParcelData.h)
 */
UENUM(BlueprintType)
enum class EPickpackerParcelType : uint8
{
	None,
	Fragile,
	Heavy,
	Unstable
};

/**
 * Parcel configuration data (renamed to avoid conflict with DA_ParcelData.h)
 */
USTRUCT(BlueprintType)
struct BLASTER_API FPickpackerParcelConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Config")
	EPickpackerParcelType Type = EPickpackerParcelType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Config")
	float BaseDurability = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Config")
	float BaseWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Config")
	float BaseInstability = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Config")
	float ImpactThreshold = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Config")
	float InstabilityDecayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Config")
	bool bIsPrimaryObjective = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parcel Config")
	FString Description = TEXT("");


	FPickpackerParcelConfig()
	{
		Type = EPickpackerParcelType::None;
		BaseDurability = 100.0f;
		BaseWeight = 1.0f;
		BaseInstability = 0.0f;
		ImpactThreshold = 50.0f;
		InstabilityDecayRate = 1.0f;
		bIsPrimaryObjective = false;
		Description = TEXT("");
	}
};

/**
 * Carry socket data
 */
USTRUCT(BlueprintType)
struct BLASTER_API FCarrySocket
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry Socket")
	FName SocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry Socket")
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry Socket")
	FRotator RelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry Socket")
	bool bIsOccupied = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry Socket")
	class ACharacter* OccupyingCharacter = nullptr;

	FCarrySocket()
	{
		SocketName = NAME_None;
		RelativeLocation = FVector::ZeroVector;
		RelativeRotation = FRotator::ZeroRotator;
		bIsOccupied = false;
		OccupyingCharacter = nullptr;
	}
};

/**
 * Objective row for PCG anchor spawning
 */
USTRUCT(BlueprintType)
struct BLASTER_API FObjectiveRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FName Tag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	TSubclassOf<AActor> Class = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	bool bIsRequired = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FString Description = TEXT("");

	FObjectiveRow()
	{
		Tag = NAME_None;
		Class = nullptr;
		Count = 1;
		bIsRequired = true;
		Description = TEXT("");
	}
};

/**
 * Spawner row for PCG anchor spawning
 */
USTRUCT(BlueprintType)
struct BLASTER_API FSpawnerRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	FName Tag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	TSubclassOf<AActor> Class = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	float SpawnChance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	FString Description = TEXT("");

	FSpawnerRow()
	{
		Tag = NAME_None;
		Class = nullptr;
		Count = 1;
		SpawnChance = 1.0f;
		Description = TEXT("");
	}
};

/**
 * PCG Anchor types for tagging
 */
UENUM(BlueprintType)
enum class EPCGAnchorType : uint8
{
	None,
	Objective,
	Extract,
	EnemySpawn,
	HazardSpawn,
	ItemSpawn
};

/**
 * PCG Anchor data structure
 */
USTRUCT(BlueprintType)
struct BLASTER_API FPCGAnchorData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG Anchor")
	EPCGAnchorType AnchorType = EPCGAnchorType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG Anchor")
	FName Tag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG Anchor")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG Anchor")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG Anchor")
	TMap<FName, FString> Metadata;

	FPCGAnchorData()
	{
		AnchorType = EPCGAnchorType::None;
		Tag = NAME_None;
		Location = FVector::ZeroVector;
		Rotation = FRotator::ZeroRotator;
	}
};

/**
 * Mission/seed configuration for dungeon generation
 */
USTRUCT(BlueprintType)
struct BLASTER_API FSeedSet
{
	GENERATED_BODY()

	/** Random seed used for deterministic generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker")
	int32 Seed = 0;

	/** Mission identifier or name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker")
	FString MissionId = TEXT("");

	FSeedSet()
	{
		Seed = 0;
		MissionId = TEXT("");
	}
};

/**
 * Suspicious Behavior Types
 */
UENUM(BlueprintType)
enum class ESuspiciousBehavior : uint8
{
	None					UMETA(DisplayName = "None"),
	DisassemblingParcel		UMETA(DisplayName = "Disassembling Parcel"),
	EnteringRestrictedZone	UMETA(DisplayName = "Entering Restricted Zone"),
	PickingPackagedParcel	UMETA(DisplayName = "Picking Packaged Parcel"),
	HoldingSuspiciousItem	UMETA(DisplayName = "Holding Suspicious Item"),
	HoldingWeapon			UMETA(DisplayName = "Holding Weapon"),
	InteractingRestrictedSystem UMETA(DisplayName = "Interacting Restricted System"),
	OtherViolation			UMETA(DisplayName = "Other Violation")
};



