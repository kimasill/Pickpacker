// Fill out your copyright notice in the Description page of Project Settings.


#include "PCG/PCGGenerator.h"

// Sets default values
APCGGenerator::APCGGenerator()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void APCGGenerator::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APCGGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void APCGGenerator::RequestBlueprintPCG_Implementation(int32 Seed)
{
	// Forward to BP event by default
	PCG_Generation(Seed);
}

void APCGGenerator::RequestBlueprintPCGWithOptions_Implementation(int32 Seed, bool bPreviewOnly)
{
	// Default: forward to standard generation. BP can override and branch on bPreviewOnly to disable actor spawners.
	PCG_Generation(Seed);
}

