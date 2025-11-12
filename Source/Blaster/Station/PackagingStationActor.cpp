// Fill out your copyright notice in the Description page of Project Settings.

#include "PackagingStationActor.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/Components/InteractionComponent.h"

APackagingStationActor::APackagingStationActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// Create components
	StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationMesh"));
	RootComponent = StationMesh;

	// 입력 영역
	InputArea = CreateDefaultSubobject<UBoxComponent>(TEXT("InputArea"));
	InputArea->SetupAttachment(RootComponent);
	InputArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InputArea->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	InputArea->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	InputArea->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Overlap);
	InputArea->SetCollisionResponseToChannel(ECollisionChannel::ECC_PhysicsBody, ECollisionResponse::ECR_Overlap);

	// 출력 위치
	OutputLocation = CreateDefaultSubobject<USceneComponent>(TEXT("OutputLocation"));
	OutputLocation->SetupAttachment(RootComponent);

	// 기본 설정
	CurrentMode = EPackagingMode::Pack;
	ProcessingTime = 2.0f;
	bAutoProcess = true;
	ProcessingInterval = 1.0f;
	bIsProcessing = false;
	bEnableDebugLogging = true;
}

void APackagingStationActor::BeginPlay()
{
	Super::BeginPlay();

	// 오버랩 이벤트 설정
	if (InputArea)
	{
		InputArea->OnComponentBeginOverlap.AddDynamic(this, &APackagingStationActor::OnInputAreaOverlap);
	}

	// 자동 처리 타이머 설정
	if (bAutoProcess && HasAuthority())
	{
		GetWorldTimerManager().SetTimer(
			AutoProcessTimerHandle,
			this,
			&APackagingStationActor::ProcessInputArea,
			ProcessingInterval,
			true
		);
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PackagingStationActor] Packaging station initialized: %s (Mode: %s)"), 
			*GetName(), 
			CurrentMode == EPackagingMode::Pack ? TEXT("Pack") : TEXT("Unpack"));
	}
}

void APackagingStationActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APackagingStationActor::ToggleMode()
{
	SetMode(CurrentMode == EPackagingMode::Pack ? EPackagingMode::Unpack : EPackagingMode::Pack);
}

void APackagingStationActor::SetMode(EPackagingMode NewMode)
{
	if (CurrentMode == NewMode)
	{
		return;
	}

	CurrentMode = NewMode;

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PackagingStationActor] Mode changed to: %s"),
			CurrentMode == EPackagingMode::Pack ? TEXT("Pack") : TEXT("Unpack"));
	}

	OnModeChanged.Broadcast(CurrentMode);
}

void APackagingStationActor::ProcessInputArea()
{
	if (!HasAuthority() || bIsProcessing)
	{
		return;
	}

	AParcelActor* Parcel = FindParcelInArea();
	if (!Parcel || Parcel->IsAttached())
	{
		return;
	}

	// 모드에 따라 처리
	if (CurrentMode == EPackagingMode::Pack)
	{
		// 포장 모드: 포장되지 않은 Parcel만 처리
		if (!Parcel->IsPackaged())
		{
			AddToProcessingQueue(Parcel);
		}
	}
	else
	{
		// 포장 해제 모드: 포장된 Parcel만 처리
		if (Parcel->IsPackaged())
		{
			AddToProcessingQueue(Parcel);
		}
	}
}

void APackagingStationActor::PackageParcel(AParcelActor* Parcel)
{
	if (!Parcel || !HasAuthority())
	{
		return;
	}

	if (Parcel->IsPackaged())
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PackagingStationActor] Parcel already packaged: %s"), *Parcel->GetName());
		}
		return;
	}

	// 포장 처리
	Parcel->SetPackaged(true);

	// 출력 위치로 이동
	if (OutputLocation)
	{
		FTransform OutputTransform = OutputLocation->GetComponentTransform();
		Parcel->SetActorTransform(OutputTransform);
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PackagingStationActor] Parcel packaged: %s"), *Parcel->GetName());
	}

	OnParcelPackaged.Broadcast(Parcel);
}

void APackagingStationActor::UnpackageParcel(AParcelActor* Parcel)
{
	if (!Parcel || !HasAuthority())
	{
		return;
	}

	if (!Parcel->IsPackaged())
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PackagingStationActor] Parcel not packaged: %s"), *Parcel->GetName());
		}
		return;
	}

	// 포장 해제 처리
	Parcel->SetPackaged(false);

	// 출력 위치로 이동
	if (OutputLocation)
	{
		FTransform OutputTransform = OutputLocation->GetComponentTransform();
		Parcel->SetActorTransform(OutputTransform);
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PackagingStationActor] Parcel unpackaged: %s"), *Parcel->GetName());
	}

	OnParcelUnpackaged.Broadcast(Parcel);
}

bool APackagingStationActor::OnInteract_Implementation(ACharacter* Interactor)
{
	if (!Interactor)
	{
		return false;
	}

	// 모드 전환
	ToggleMode();
	return true;
}

bool APackagingStationActor::CanInteract_Implementation(ACharacter* Interactor) const
{
	return true;
}

FText APackagingStationActor::GetInteractText_Implementation() const
{
	FString ModeText = CurrentMode == EPackagingMode::Pack ? TEXT("Pack") : TEXT("Unpack");
	return FText::FromString(FString::Printf(TEXT("Press E to Switch Mode (Current: %s)"), *ModeText));
}

void APackagingStationActor::StartHighlight_Implementation()
{
	if (!StationMesh)
	{
		return;
	}

	StationMesh->SetRenderCustomDepth(true);
	StationMesh->SetCustomDepthStencilValue(252);
}

void APackagingStationActor::EndHighlight_Implementation()
{
	if (!StationMesh)
	{
		return;
	}

	StationMesh->SetRenderCustomDepth(false);
}

void APackagingStationActor::OnInputAreaOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	// 오버랩 이벤트는 자동 처리 타이머에서 처리
	// 필요시 여기서 즉시 처리할 수도 있음
}

AParcelActor* APackagingStationActor::FindParcelInArea() const
{
	if (!InputArea)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// 오버랩된 액터 찾기
	TArray<AActor*> OverlappingActors;
	InputArea->GetOverlappingActors(OverlappingActors, AParcelActor::StaticClass());

	for (AActor* Actor : OverlappingActors)
	{
		AParcelActor* Parcel = Cast<AParcelActor>(Actor);
		if (Parcel && !Parcel->IsAttached())
		{
			return Parcel;
		}
	}

	return nullptr;
}

void APackagingStationActor::AddToProcessingQueue(AParcelActor* Parcel)
{
	if (!Parcel || bIsProcessing)
	{
		return;
	}

	// 이미 큐에 있는지 확인
	for (const TWeakObjectPtr<AParcelActor>& QueuedParcel : ProcessingQueue)
	{
		if (QueuedParcel.Get() == Parcel)
		{
			return; // 이미 큐에 있음
		}
	}

	ProcessingQueue.Add(Parcel);

	// 처리 중이 아니면 즉시 처리 시작
	if (!bIsProcessing)
	{
		ProcessNextInQueue();
	}
}

void APackagingStationActor::ProcessNextInQueue()
{
	if (bIsProcessing || ProcessingQueue.Num() == 0)
	{
		return;
	}

	// 유효하지 않은 Parcel 제거
	ProcessingQueue.RemoveAll([](const TWeakObjectPtr<AParcelActor>& Parcel)
	{
		return !Parcel.IsValid() || Parcel->IsAttached();
	});

	if (ProcessingQueue.Num() == 0)
	{
		return;
	}

	// 첫 번째 Parcel 처리
	CurrentProcessingParcel = ProcessingQueue[0];
	ProcessingQueue.RemoveAt(0);

	if (!CurrentProcessingParcel.IsValid())
	{
		// 유효하지 않으면 다음 처리
		ProcessNextInQueue();
		return;
	}

	bIsProcessing = true;

	// 처리 시간 후 완료
	if (GetWorld())
	{
		GetWorldTimerManager().SetTimer(
			ProcessingTimerHandle,
			this,
			&APackagingStationActor::OnProcessingComplete,
			ProcessingTime,
			false
		);
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PackagingStationActor] Processing started: %s (Mode: %s)"),
			*CurrentProcessingParcel->GetName(),
			CurrentMode == EPackagingMode::Pack ? TEXT("Pack") : TEXT("Unpack"));
	}
}

void APackagingStationActor::OnProcessingComplete()
{
	if (!CurrentProcessingParcel.IsValid())
	{
		bIsProcessing = false;
		ProcessNextInQueue();
		return;
	}

	AParcelActor* Parcel = CurrentProcessingParcel.Get();

	// 모드에 따라 처리
	if (CurrentMode == EPackagingMode::Pack)
	{
		PackageParcel(Parcel);
	}
	else
	{
		UnpackageParcel(Parcel);
	}

	bIsProcessing = false;
	CurrentProcessingParcel = nullptr;

	// 다음 처리
	ProcessNextInQueue();
}
