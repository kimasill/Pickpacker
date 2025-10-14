// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Blaster/PickpackerTypes/PickpackerTypes.h"
#include "CarryPointsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCarrySocketOccupied, FName, SocketName, class ACharacter*, Character);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCarrySocketFreed, FName, SocketName, class ACharacter*, Character);

/**
 * Carry Points Component - Manages carry sockets for parcel attachment
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BLASTER_API UCarryPointsComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UCarryPointsComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/**
	 * Initialize carry sockets
	 */
	UFUNCTION(BlueprintCallable, Category = "Carry Points")
	void InitializeCarrySockets();

	/**
	 * Try to attach character to socket
	 */
	UFUNCTION(BlueprintCallable, Category = "Carry Points")
	bool TryAttachToSocket(class ACharacter* Character, const FName& SocketName);

	/**
	 * Detach character from socket
	 */
	UFUNCTION(BlueprintCallable, Category = "Carry Points")
	bool DetachFromSocket(class ACharacter* Character, const FName& SocketName);

	/**
	 * Detach all characters
	 */
	UFUNCTION(BlueprintCallable, Category = "Carry Points")
	void DetachAllCharacters();

	/**
	 * Get socket location in world space
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry Points")
	FVector GetSocketWorldLocation(const FName& SocketName) const;

	/**
	 * Get socket rotation in world space
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry Points")
	FRotator GetSocketWorldRotation(const FName& SocketName) const;

	/**
	 * Check if socket is available
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry Points")
	bool IsSocketAvailable(const FName& SocketName) const;

	/**
	 * Get available socket count
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry Points")
	int32 GetAvailableSocketCount() const;

	/**
	 * Get occupied socket count
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry Points")
	int32 GetOccupiedSocketCount() const;

	/**
	 * Check if both sockets are occupied (2-person carry)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry Points")
	bool IsTwoPersonCarry() const;

	/**
	 * Get stability bonus for two-person carry
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry Points")
	float GetStabilityBonus() const;

	/**
	 * Get movement speed bonus for two-person carry
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry Points")
	float GetMovementSpeedBonus() const;

	/**
	 * Get all carry sockets
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry Points")
	const TArray<FCarrySocket>& GetCarrySockets() const { return CarrySockets; }

	/**
	 * Get carry socket by name
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry Points")
	FCarrySocket GetCarrySocket(const FName& SocketName) const;

protected:
	/**
	 * Called when socket is occupied
	 */
	UFUNCTION()
	void HandleSocketOccupied(const FName& SocketName, class ACharacter* Character);

	/**
	 * Called when socket is freed
	 */
	UFUNCTION()
	void HandleSocketFreed(const FName& SocketName, class ACharacter* Character);

	/**
	 * Find socket by name
	 */
	int32 FindSocketIndex(const FName& SocketName) const;

	/**
	 * Update socket transforms
	 */
	void UpdateSocketTransforms();

public:
	// Delegates
	UPROPERTY(BlueprintAssignable, Category = "Carry Points")
	FOnCarrySocketOccupied OnSocketOccupied;

	UPROPERTY(BlueprintAssignable, Category = "Carry Points")
	FOnCarrySocketFreed OnSocketFreed;

protected:
	// Carry sockets
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Carry Sockets")
	TArray<FCarrySocket> CarrySockets;

	// Socket configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry Sockets")
	FName LeftHandleSocketName = FName(TEXT("LeftHandle"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry Sockets")
	FName RightHandleSocketName = FName(TEXT("RightHandle"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry Sockets")
	FVector LeftHandleOffset = FVector(-50.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry Sockets")
	FVector RightHandleOffset = FVector(50.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry Sockets")
	float SocketRadius = 30.0f;

	// Two-person carry settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Two-Person Carry")
	float TwoPersonStabilityBonus = 0.3f; // 30% stability bonus

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Two-Person Carry")
	float TwoPersonMovementBonus = 0.15f; // 15% movement speed bonus

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Two-Person Carry")
	bool bEnableTwoPersonCarry = true;

	// Debug settings
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bEnableDebugLogging = true;

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebugSockets = false;
};
