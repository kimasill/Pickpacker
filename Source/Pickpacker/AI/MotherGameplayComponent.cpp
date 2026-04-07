// Fill out your copyright notice in the Description page of Project Settings.

#include "MotherGameplayComponent.h"
#include "AI/MotherAIActor.h"
#include "AI/DroneActor.h"
#include "GameState/PickpackerGameState.h"
#include "Engine/World.h"
#include "TimerManager.h"

UMotherGameplayComponent::UMotherGameplayComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

AMotherAIActor* UMotherGameplayComponent::GetMother() const
{
	return Cast<AMotherAIActor>(GetOwner());
}

ADroneActor* UMotherGameplayComponent::SpawnDroneAt(const FVector& Location)
{
	AMotherAIActor* Mother = GetMother();
	if (!Mother || !Mother->DroneClass || Mother->ActiveDrones.Num() >= Mother->MaxDrones)
	{
		return nullptr;
	}

	if (!Mother->HasAuthority())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ADroneActor* NewDrone = Mother->GetWorld()->SpawnActor<ADroneActor>(
		Mother->DroneClass, Location, FRotator::ZeroRotator, SpawnParams);
	if (NewDrone)
	{
		Mother->ActiveDrones.Add(NewDrone);
	}

	return NewDrone;
}

void UMotherGameplayComponent::PunishPlayers(float SuspicionPoints)
{
	AMotherAIActor* Mother = GetMother();
	if (!Mother)
	{
		return;
	}

	if (Mother->GameState)
	{
		Mother->GameState->AddTeamSuspicion(SuspicionPoints);
	}

	if (Mother->ActiveDrones.Num() < Mother->MaxDrones && Mother->DroneSpawnLocations.Num() > 0)
	{
		const int32 DronesToSpawn = FMath::Min(Mother->MaxDrones - Mother->ActiveDrones.Num(), 2);
		for (int32 i = 0; i < DronesToSpawn; ++i)
		{
			const int32 RandomIndex = FMath::RandRange(0, Mother->DroneSpawnLocations.Num() - 1);
			SpawnDroneAt(Mother->DroneSpawnLocations[RandomIndex]);
		}
	}
}

void UMotherGameplayComponent::ManageDrones()
{
	AMotherAIActor* Mother = GetMother();
	if (!Mother)
	{
		return;
	}

	if (Mother->CurrentState == EMotherAIState::RestPeriod)
	{
		return;
	}

	const float Suspicion = Mother->GetSuspicionLevel();

	const int32 DesiredDrones = FMath::Clamp(
		FMath::RoundToInt(Suspicion * Mother->MaxDrones),
		1,
		Mother->MaxDrones);

	while (Mother->ActiveDrones.Num() > DesiredDrones)
	{
		ADroneActor* Drone = Mother->ActiveDrones.Last();
		if (Drone)
		{
			Drone->Destroy();
		}
		Mother->ActiveDrones.RemoveAt(Mother->ActiveDrones.Num() - 1);
	}

	while (Mother->ActiveDrones.Num() < DesiredDrones && Mother->DroneSpawnLocations.Num() > 0)
	{
		const int32 RandomIndex = FMath::RandRange(0, Mother->DroneSpawnLocations.Num() - 1);
		SpawnDroneAt(Mother->DroneSpawnLocations[RandomIndex]);
	}
}

void UMotherGameplayComponent::ScheduleNextInspection()
{
	AMotherAIActor* Mother = GetMother();
	if (!Mother || !Mother->HasAuthority() || Mother->InspectionTimes.Num() == 0)
	{
		return;
	}

	APickpackerGameState* PickpackerGameState = Mother->GetWorld()->GetGameState<APickpackerGameState>();
	if (!PickpackerGameState)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MotherGameplayComponent] PickpackerGameState not found, cannot schedule inspection"));
		return;
	}

	const float CurrentGameHour = PickpackerGameState->GetCurrentGameHour();

	float NextInspectionHour = MAX_FLT;
	for (float InspectionTime : Mother->InspectionTimes)
	{
		float TimeUntilInspection = InspectionTime - CurrentGameHour;
		if (TimeUntilInspection < 0.0f)
		{
			TimeUntilInspection += 24.0f;
		}

		if (TimeUntilInspection < NextInspectionHour)
		{
			NextInspectionHour = TimeUntilInspection;
		}
	}

	if (NextInspectionHour < MAX_FLT)
	{
		const float RealTimeUntilInspection = PickpackerGameState->ConvertGameHoursToRealSeconds(NextInspectionHour);

		Mother->GetWorld()->GetTimerManager().SetTimer(
			Mother->InspectionTimerHandle,
			Mother,
			&AMotherAIActor::TriggerInspection,
			RealTimeUntilInspection,
			false);

		UE_LOG(LogTemp, Log, TEXT("[MotherGameplayComponent] Scheduled next inspection in %.2f game hours (%.2f real seconds)"),
			NextInspectionHour, RealTimeUntilInspection);
	}
}
