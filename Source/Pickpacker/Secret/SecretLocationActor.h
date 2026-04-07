// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Parcel/ParcelActor.h"
#include "DataAssets/DA_ItemData.h"
#include "SecretLocationActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UWidgetComponent;

/**
 * Secret Location Actor - Hidden location where players can hide and find items
 */
UCLASS(BlueprintType, Blueprintable)
class PICKPACKER_API ASecretLocationActor : public AActor
{
	GENERATED_BODY()

public:
	ASecretLocationActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/**
	 * Check if location is discovered
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Secret Location")
	bool IsDiscovered() const { return bIsDiscovered; }

	/**
	 * Discover this location
	 */
	UFUNCTION(BlueprintCallable, Category = "Secret Location")
	void Discover();

	/**
	 * Check if player can access this location
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Secret Location")
	bool CanAccess(class ACharacter* Player) const;

	/**
	 * Check if player is inside this location
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Secret Location")
	bool IsPlayerInside(class ACharacter* Player) const;

	/**
	 * Spawn item at this location
	 */
	UFUNCTION(BlueprintCallable, Category = "Secret Location")
	AParcelActor* SpawnItem(EItemType ItemType, const FItemData& ItemData);

	/**
	 * Get all items at this location
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Secret Location")
	TArray<AParcelActor*> GetItems() const { return SpawnedItems; }

public:
	/** Location name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secret Location")
	FString LocationName = TEXT("Secret Location");

	/** Location description */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secret Location")
	FText LocationDescription;

	/** Whether location requires specific item to access */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secret Location")
	bool bRequiresItem = false;

	/** Required item type to access */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secret Location", meta = (EditCondition = "bRequiresItem"))
	EItemType RequiredItemType = EItemType::Key;

	/** Items to spawn at this location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secret Location")
	TArray<FItemData> ItemsToSpawn;

	/** Item spawn locations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secret Location")
	TArray<FVector> ItemSpawnLocations;

	/** Parcel class to spawn for items */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secret Location")
	TSubclassOf<AParcelActor> ItemParcelClass;

	/** Whether location is hidden from surveillance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secret Location")
	bool bHiddenFromSurveillance = true;

	/** Events */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLocationDiscovered, ASecretLocationActor*, Location);
	UPROPERTY(BlueprintAssignable, Category = "Secret Location|Events")
	FOnLocationDiscovered OnLocationDiscovered;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerEntered, ASecretLocationActor*, Location, class ACharacter*, Player);
	UPROPERTY(BlueprintAssignable, Category = "Secret Location|Events")
	FOnPlayerEntered OnPlayerEntered;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerExited, ASecretLocationActor*, Location, class ACharacter*, Player);
	UPROPERTY(BlueprintAssignable, Category = "Secret Location|Events")
	FOnPlayerExited OnPlayerExited;

protected:
	/**
	 * Handle player entering location
	 */
	UFUNCTION()
	void OnPlayerEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/**
	 * Handle player exiting location
	 */
	UFUNCTION()
	void OnPlayerExit(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/**
	 * Spawn initial items
	 */
	UFUNCTION()
	void SpawnInitialItems();

private:
	/** Whether location is discovered */
	UPROPERTY(Replicated)
	bool bIsDiscovered = false;

	/** Players currently inside */
	UPROPERTY()
	TArray<TWeakObjectPtr<class ACharacter>> PlayersInside;

	/** Spawned items */
	UPROPERTY()
	TArray<TObjectPtr<AParcelActor>> SpawnedItems;

	/** Components */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* LocationMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* TriggerBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UWidgetComponent* DiscoveryWidget;
};

