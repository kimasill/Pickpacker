// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PlayerController/BlasterPlayerController.h"
#include "PickpackerPlayerController.generated.h"

class APickpackerGameMode;
class APickpackerGameState;

UCLASS()
class PICKPACKER_API APickpackerPlayerController : public ABlasterPlayerController
{
	GENERATED_BODY()

public:
	APickpackerPlayerController();

	/**
	 * Report to server that client PCG generation is complete
	 */
	UFUNCTION(Server, Reliable)
	void ServerReportPCGReady();

	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Helpers to manage readiness from client side
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|PCG")
	void SetClientPCGReady(bool bReady);

	UFUNCTION(BlueprintCallable, Category = "Pickpacker|PCG")
	void OnClientPCGReady();

protected:
	/**
	 * Replicated flag indicating this client finished local PCG
	 */
	UPROPERTY(Replicated)
	bool bClientPCGReady = false;

	/**
	 * Local guard to avoid double reporting
	 */
	UPROPERTY()
	bool bHasReportedPCGReady = false;
};
