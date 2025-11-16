// Fill out your copyright notice in the Description page of Project Settings.

#include "PackagingSwitchActor.h"
#include "Blaster/Station/PackagingStationActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/Components/InteractionComponent.h"

APackagingSwitchActor::APackagingSwitchActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// Create components
	SwitchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwitchMesh"));
	RootComponent = SwitchMesh;

	InteractionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget"));
	InteractionWidget->SetupAttachment(RootComponent);
	InteractionWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionWidget->SetDrawAtDesiredSize(true);

	// 기본 설정
	bAutoFindStation = false;
	AutoFindDistance = 1000.0f;
	bEnableDebugLogging = true;
}

void APackagingSwitchActor::BeginPlay()
{
	Super::BeginPlay();

	// 자동으로 스테이션 찾기
	if (bAutoFindStation && !PackagingStation)
	{
		FindNearestPackagingStation();
	}

	if (bEnableDebugLogging)
	{
		if (PackagingStation)
		{
			UE_LOG(LogTemp, Log, TEXT("[PackagingSwitchActor] Switch initialized: %s -> Station: %s"),
				*GetName(), *PackagingStation->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[PackagingSwitchActor] Switch initialized without station: %s"), *GetName());
		}
	}

	if (InteractionWidget)
	{
		InteractionWidget->SetVisibility(false); // Hidden by default
	}
	if (SwitchMesh)
	{
		SwitchMesh->SetRenderCustomDepth(false);
	}
}

void APackagingSwitchActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APackagingSwitchActor::ToggleSwitch()
{
	if (!PackagingStation)
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PackagingSwitchActor] No packaging station connected"));
		}
		return;
	}

	// PackagingStationActor의 모드 전환
	PackagingStation->ToggleMode();
	EPackagingMode NewMode = PackagingStation->GetMode();

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PackagingSwitchActor] Switch toggled: Mode changed to %s"),
			NewMode == EPackagingMode::Pack ? TEXT("Pack") : TEXT("Unpack"));
	}

	OnSwitchToggled.Broadcast(NewMode);
}

void APackagingSwitchActor::SetPackagingStation(APackagingStationActor* Station)
{
	PackagingStation = Station;

	if (bEnableDebugLogging)
	{
		if (Station)
		{
			UE_LOG(LogTemp, Log, TEXT("[PackagingSwitchActor] Station set: %s"), *Station->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[PackagingSwitchActor] Station cleared"));
		}
	}
}

bool APackagingSwitchActor::OnInteract_Implementation(ACharacter* Interactor)
{
	if (!Interactor)
	{
		return false;
	}

	ToggleSwitch();

	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Interactor);


	if (PackagingStation->CurrentMode == EPackagingMode::Unpack && BlasterCharacter)
	{
		BlasterCharacter->ReportSuspiciousBehavior(ESuspiciousBehavior::DisassemblingParcel);
	}

	return true;
}

bool APackagingSwitchActor::CanInteract_Implementation(ACharacter* Interactor) const
{
	return PackagingStation != nullptr;
}

FText APackagingSwitchActor::GetInteractText_Implementation() const
{
	if (!PackagingStation)
	{
		return FText::FromString(TEXT("No Station Connected"));
	}

	EPackagingMode CurrentMode = PackagingStation->GetMode();
	FString ModeText = CurrentMode == EPackagingMode::Pack ? TEXT("Pack") : TEXT("Unpack");
	return FText::FromString(FString::Printf(TEXT("Press E to Switch Mode (Current: %s)"), *ModeText));
}

void APackagingSwitchActor::StartHighlight_Implementation()
{
	if (!SwitchMesh)
	{
		return;
	}

	SwitchMesh->SetRenderCustomDepth(true);
	SwitchMesh->SetCustomDepthStencilValue(252);
	ShowInteractionWidget(true);
}

void APackagingSwitchActor::EndHighlight_Implementation()
{
	if (!SwitchMesh)
	{
		return;
	}

	SwitchMesh->SetRenderCustomDepth(false);
	ShowInteractionWidget(false);
}

void APackagingSwitchActor::ShowInteractionWidget(bool bShow)
{
	if (InteractionWidget)
	{
		InteractionWidget->SetVisibility(bShow);
	}
}

void APackagingSwitchActor::FindNearestPackagingStation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APackagingStationActor* NearestStation = nullptr;
	float NearestDistance = AutoFindDistance;

	FVector SwitchLocation = GetActorLocation();

	// 레벨의 모든 PackagingStationActor 찾기
	for (TActorIterator<APackagingStationActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		APackagingStationActor* Station = *ActorItr;
		if (!Station || !IsValid(Station))
		{
			continue;
		}

		float Distance = FVector::Dist(SwitchLocation, Station->GetActorLocation());
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			NearestStation = Station;
		}
	}

	if (NearestStation)
	{
		PackagingStation = NearestStation;
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Log, TEXT("[PackagingSwitchActor] Found nearest station: %s (Distance: %.2f)"),
				*NearestStation->GetName(), NearestDistance);
		}
	}
	else
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PackagingSwitchActor] No packaging station found within distance %.2f"), AutoFindDistance);
		}
	}
}

