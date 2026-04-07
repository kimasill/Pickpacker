// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "PickpackerTypes.generated.h"

/**
 * Parcel state structure for replication
 */
USTRUCT(BlueprintType)
struct PICKPACKER_API FParcelState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Parcel State")
	float Durability = 100.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Parcel State")
	float Weight = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Parcel State")
	int32 Unit = 1;

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
		Unit = 1;
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
struct PICKPACKER_API FPickpackerParcelConfig
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
struct PICKPACKER_API FCarrySocket
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
struct PICKPACKER_API FObjectiveRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FName Tag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	TSubclassOf<AActor> Class;

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
struct PICKPACKER_API FSpawnerRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	FName Tag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	TSubclassOf<AActor> Class;

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
struct PICKPACKER_API FPCGAnchorData
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
struct PICKPACKER_API FSeedSet
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
 * Definition of an order that players must fulfill
 */
USTRUCT(BlueprintType)
struct PICKPACKER_API FParcelOrderDefinition
{
	GENERATED_BODY()

	/** Display name for UI/LED billboard */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Orders")
	FName OrderName = NAME_None;

	/** Description shown on UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Orders")
	FText OrderDescription;

	/** Parcel tag requirement for this order */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Orders")
	FGameplayTag RequiredParcelTag;

	/** Item tag requirement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Orders")
	FGameplayTag RequiredItemTag;

	/** Number of parcels required */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Orders", meta = (ClampMin = "1"))
	int32 RequiredQuantity = 1;

	/** Should the parcel arrive packaged */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Orders")
	bool bRequirePackaged = true;

	/** Time limit in seconds (<=0 means no limit) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Orders")
	float TimeLimitSeconds = 60.0f;

	/** Credit reward on success */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Orders")
	int32 CreditReward = 10;

	/** Credit penalty on failure */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Orders")
	int32 CreditPenalty = 5;

	/** Suspicion penalty applied on failure */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Orders")
	float SuspicionPenalty = 2.0f;

	FParcelOrderDefinition()
	{
		RequiredQuantity = 1;
		TimeLimitSeconds = 60.0f;
		CreditReward = 10;
		CreditPenalty = 5;
		SuspicionPenalty = 2.0f;
	}
};

/**
 * Wave definition grouping multiple orders
 */
USTRUCT(BlueprintType)
struct PICKPACKER_API FParcelOrderWave
{
	GENERATED_BODY()

	/** 각 repeat마다 한 번에 생성할 오더 개수 (1=1개씩, 2=2개씩 생성, 0=템플릿 전체) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Orders", meta = (ClampMin = "0"))
	int32 OrdersPerWave = 1;

	/** 이 웨이브를 반복할 횟수 (repeat마다 OrdersPerWave개씩 생성) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Orders", meta = (ClampMin = "1"))
	int32 RepeatCount = 1;

	/** 각 repeat 사이 딜레이 (초). repeat 직후 이 시간 뒤에 다음 오더 배치 생성 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Orders", meta = (ClampMin = "0.0"))
	float RepeatDelay = 0.0f;

	/** 다음 웨이브로 넘어가기 전 딜레이 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Orders", meta = (ClampMin = "0.0"))
	float NextWaveDelay = 10.0f;

	/** 이 웨이브에서 사용할 오더 템플릿 목록 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Orders")
	TArray<FParcelOrderDefinition> Orders;
};

/**
 * Runtime state of an order replicated to clients
 */
USTRUCT(BlueprintType)
struct PICKPACKER_API FActiveOrderState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Orders")
	FGuid OrderId = FGuid();

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Orders")
	FName OrderName = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Orders")
	FText OrderDescription;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Orders")
	FGameplayTag RequiredParcelTag;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Orders")
	FGameplayTag RequiredItemTag;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Orders")
	int32 RequiredQuantity = 1;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Orders")
	int32 SubmittedQuantity = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Orders")
	bool bRequirePackaged = true;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Orders")
	float ExpireTime = -1.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Orders")
	bool bCompleted = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Orders")
	bool bFailed = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Orders")
	int32 CreditReward = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Orders")
	int32 CreditPenalty = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Orders")
	float SuspicionPenalty = 0.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Orders")
	float ResolutionTime = -1.0f;
};

/**
 * Credit transaction log entry
 */
USTRUCT(BlueprintType)
struct PICKPACKER_API FCreditTransaction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Credits")
	FGuid TransactionId = FGuid();

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Credits")
	int32 Delta = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Credits")
	int32 BalanceAfter = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Credits")
	float Timestamp = 0.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pickpacker|Credits")
	FText Reason = FText::GetEmpty();
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
	DroppingParcel			UMETA(DisplayName = "Dropping Parcel"),
	HoldingSuspiciousItem	UMETA(DisplayName = "Holding Suspicious Item"),
	HoldingWeapon			UMETA(DisplayName = "Holding Weapon"),
	InteractingRestrictedSystem UMETA(DisplayName = "Interacting Restricted System"),
	OtherViolation			UMETA(DisplayName = "Other Violation")
};




