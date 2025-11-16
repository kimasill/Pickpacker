// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Blaster/DataAssets/DA_ItemData.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "EscapeZoneActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UWidgetComponent;

/**
 * Escape Requirement - Defines what is needed to escape
 */
USTRUCT(BlueprintType)
struct BLASTER_API FEscapeRequirement
{
	GENERATED_BODY()

	/** Required item type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Escape Requirement")
	EItemType RequiredItemType = EItemType::Key;

	/** Required count */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Escape Requirement")
	int32 RequiredCount = 1;

	/** Whether item must be used */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Escape Requirement")
	bool bMustBeUsed = true;

	FEscapeRequirement()
	{
		RequiredItemType = EItemType::Key;
		RequiredCount = 1;
		bMustBeUsed = true;
	}
};

/**
 * Escape Zone Actor - Zone where players can escape if requirements are met
 */
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API AEscapeZoneActor : public AActor
{
	GENERATED_BODY()

public:
	AEscapeZoneActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/**
	 * Check if player can escape
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Escape")
	bool CanPlayerEscape(class ACharacter* Player) const;

	/**
	 * Attempt escape
	 */
	UFUNCTION(BlueprintCallable, Category = "Escape")
	bool AttemptEscape(class ACharacter* Player);

	/**
	 * Get escape progress (0.0 - 1.0)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Escape")
	float GetEscapeProgress(class ACharacter* Player) const;

	/**
	 * Get escape requirements
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Escape")
	TArray<FEscapeRequirement> GetEscapeRequirements() const { return EscapeRequirements; }

	/**
	 * Check if all players have escaped
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Escape")
	bool AreAllPlayersEscaped() const;

	/**
	 * Get escaped players count
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Escape")
	int32 GetEscapedPlayersCount() const { return EscapedPlayers.Num(); }

public:
	/** Escape requirements */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Escape")
	TArray<FEscapeRequirement> EscapeRequirements;

	/** Whether all players must escape to win */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Escape")
	bool bRequireAllPlayers = true;

	/** Escape zone name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Escape")
	FString ZoneName = TEXT("Escape Zone");

	/** Events */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerEscaped, class ACharacter*, Player);
	UPROPERTY(BlueprintAssignable, Category = "Escape|Events")
	FOnPlayerEscaped OnPlayerEscaped;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllPlayersEscaped);
	UPROPERTY(BlueprintAssignable, Category = "Escape|Events")
	FOnAllPlayersEscaped OnAllPlayersEscaped;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEscapeAttempted, class ACharacter*, Player, bool, bSuccess);
	UPROPERTY(BlueprintAssignable, Category = "Escape|Events")
	FOnEscapeAttempted OnEscapeAttempted;

protected:
	/**
	 * Handle player entering escape zone
	 */
	UFUNCTION()
	void OnPlayerEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/**
	 * Handle player exiting escape zone
	 */
	UFUNCTION()
	void OnPlayerExit(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/**
	 * Check escape requirements for player
	 */
	bool CheckEscapeRequirements(ACharacter* Player) const;

	/**
	 * Use required items
	 */
	bool UseRequiredItems(ACharacter* Player);

	/**
	 * Process player escape
	 */
	void ProcessPlayerEscape(ACharacter* Player);

private:
	/** Escaped players */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<class ACharacter>> EscapedPlayers;

	/** Players in zone */
	UPROPERTY()
	TArray<TWeakObjectPtr<class ACharacter>> PlayersInZone;

	/** Components */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* ZoneMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* TriggerBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UWidgetComponent* StatusWidget;
};

