// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PCGGenerator.generated.h"

/**
 * Base actor for Blueprint-driven PCG generation.
 * Blueprint can inherit from this class and implement PCG_Generation to run PCG logic.
 */
UCLASS(Blueprintable)
class BLASTER_API APCGGenerator : public AActor
{
	GENERATED_BODY()
public:
	APCGGenerator();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Entry point called by Subsystem. Default implementation forwards to PCG_Generation implementable event. */
	UFUNCTION(BlueprintNativeEvent, Category = "PCG")
	void RequestBlueprintPCG(int32 Seed);
	virtual void RequestBlueprintPCG_Implementation(int32 Seed);

	/** Entry point with options (e.g. preview-only on clients) */
	UFUNCTION(BlueprintNativeEvent, Category = "PCG")
	void RequestBlueprintPCGWithOptions(int32 Seed, bool bPreviewOnly);
	virtual void RequestBlueprintPCGWithOptions_Implementation(int32 Seed, bool bPreviewOnly);

	/** Implement in Blueprint to execute PCG with the provided seed. Call Subsystem.NotifyPCGGenerationComplete() when done. */
	UFUNCTION(BlueprintImplementableEvent, Category = "PCG")
	void PCG_Generation(int32 Seed);
};
