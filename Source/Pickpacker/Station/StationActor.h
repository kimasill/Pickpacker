// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Blaster/DataAssets/DA_StationData.h"
#include "Blaster/DataAssets/DA_ParcelData.h"
#include "Blaster/DataAssets/DA_LabelRuleData.h"
#include "Blaster/Components/InteractionComponent.h"
#include "GameplayTagContainer.h"
#include "Blaster/Interfaces/InteractableInterface.h"
#include "StationActor.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;
class UBoxComponent;

/**
 * Station Actor - Base class for all warehouse stations
 * Handles interaction with parcels and label validation
 */
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API AStationActor : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AStationActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/**
	 * Initialize station with configuration data
	 */
	UFUNCTION(BlueprintCallable, Category = "Station")
	void InitializeStation(const FStationConfig& Config);

	/**
	 * Start processing a parcel
	 */
	UFUNCTION(BlueprintCallable, Category = "Station")
	bool StartProcessingParcel(class AParcelActor* Parcel);

	/**
	 * Complete processing and validate result
	 */
	UFUNCTION(BlueprintCallable, Category = "Station")
	bool CompleteProcessing(const FGameplayTagContainer& OutputTags);

	/**
	 * Cancel current processing
	 */
	UFUNCTION(BlueprintCallable, Category = "Station")
	void CancelProcessing();

	/**
	 * Check if station is available
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station")
	bool IsAvailable() const { return !bIsProcessing && !bIsDisabled; }

	/**
	 * Check if station is processing
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station")
	bool IsProcessing() const { return bIsProcessing; }

	/**
	 * Get current station type
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station")
	EStationType GetStationType() const { return StationType; }

	/**
	 * Get current parcel being processed
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station")
	AParcelActor* GetCurrentParcel() const { return CurrentParcel; }

	/**
	 * Get processing progress (0.0 - 1.0)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station")
	float GetProcessingProgress() const;

	/**
	 * Get remaining processing time
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station")
	float GetRemainingTime() const;

	/**
	 * Set station enabled/disabled
	 */
	UFUNCTION(BlueprintCallable, Category = "Station")
	void SetStationEnabled(bool bEnabled);

	/**
	 * Get station configuration
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Station")
	const FStationConfig& GetStationConfig() const { return StationConfig; }

	// InteractableInterface Implementation
	virtual void OnInteract_Implementation(ACharacter* Interactor);
	virtual bool CanInteract_Implementation(ACharacter* Interactor);
	virtual FText GetInteractText_Implementation();
	virtual void StartHighlight_Implementation();
	virtual void EndHighlight_Implementation();

public:
	/** Broadcast when processing starts */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProcessingStarted, AParcelActor*, Parcel);
	UPROPERTY(BlueprintAssignable, Category = "Station|Events")
	FOnProcessingStarted OnProcessingStarted;

	/** Broadcast when processing completes */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProcessingCompleted, AParcelActor*, Parcel, bool, bSuccess);
	UPROPERTY(BlueprintAssignable, Category = "Station|Events")
	FOnProcessingCompleted OnProcessingCompleted;

	/** Broadcast when processing is cancelled */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProcessingCancelled, AParcelActor*, Parcel);
	UPROPERTY(BlueprintAssignable, Category = "Station|Events")
	FOnProcessingCancelled OnProcessingCancelled;

	/** Broadcast when station state changes */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStationStateChanged, bool, bIsEnabled);
	UPROPERTY(BlueprintAssignable, Category = "Station|Events")
	FOnStationStateChanged OnStationStateChanged;

protected:
	/**
	 * Called when processing starts (Blueprint implementable)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Station")
	void OnProcessingStartedBP(AParcelActor* Parcel);

	/**
	 * Called when processing completes (Blueprint implementable)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Station")
	void OnProcessingCompletedBP(AParcelActor* Parcel, bool bSuccess);

	/**
	 * Called when processing is cancelled (Blueprint implementable)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Station")
	void OnProcessingCancelledBP(AParcelActor* Parcel);

	/**
	 * Called when station state changes (Blueprint implementable)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Station")
	void OnStationStateChangedBP(bool bIsEnabled);

private:
	/** Station configuration */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station Config", meta = (AllowPrivateAccess = "true"))
	FStationConfig StationConfig;

	/** Station type */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station Config", meta = (AllowPrivateAccess = "true"))
	EStationType StationType = EStationType::Unknown;

	/** Station mesh component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* StationMesh;

	/** Interaction box component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UBoxComponent* InteractionBox;

	/** Area sphere for overlap detection */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	class USphereComponent* AreaSphere;

	/** UI widget component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UWidgetComponent* StationWidget;

	/** Current parcel being processed */
	UPROPERTY()
	AParcelActor* CurrentParcel = nullptr;

	/** Whether station is currently processing */
	UPROPERTY()
	bool bIsProcessing = false;

	/** Whether station is disabled */
	UPROPERTY()
	bool bIsDisabled = false;

	/** Processing start time */
	UPROPERTY()
	float ProcessingStartTime = 0.0f;

	/** Processing duration */
	UPROPERTY()
	float ProcessingDuration = 0.0f;

	/** Processing progress */
	UPROPERTY()
	float ProcessingProgress = 0.0f;

	/**
	 * Internal function to update processing progress
	 */
	void UpdateProcessingProgress(float DeltaTime);

	/**
	 * Internal function to validate processing result
	 */
	bool ValidateProcessingResult(const FGameplayTagContainer& OutputTags) const;
private:
	/** Original materials for highlight */
	UPROPERTY()
	TArray<UMaterialInterface*> OriginalMaterials;

	/** Highlight material */
	UPROPERTY(EditAnywhere, Category = "Interaction")
	UMaterialInterface* HighlightMaterial = nullptr;
};
