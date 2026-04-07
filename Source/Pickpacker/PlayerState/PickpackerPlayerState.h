// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PickpackerPlayerState.generated.h"

/**
 * Pickpacker Player State - Handles player-specific PCG ready state
 */
UCLASS()
class PICKPACKER_API APickpackerPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	APickpackerPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Server RPC to report PCG ready state
	 */
	UFUNCTION(Server, Reliable, Category = "Pickpacker")
	void ServerReportPCGReady();

	/**
	 * Check if PCG is ready
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker")
	bool IsPCGReady() const { return bPCGReady; }

protected:
	/**
	 * PCG ready flag
	 */
	UPROPERTY(Replicated)
	bool bPCGReady = false;

	// Prevent duplicate counting from PlayerState path
	UPROPERTY(Transient)
	bool bHasReportedPCGReady = false;
};