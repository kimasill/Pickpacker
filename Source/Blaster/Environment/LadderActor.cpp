// Copyright

#include "Blaster/Environment/LadderActor.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Blaster/Character/BlasterCharacter.h"

ALadderActor::ALadderActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	LadderSpline = CreateDefaultSubobject<USplineComponent>(TEXT("LadderSpline"));
	LadderSpline->SetupAttachment(Root);

	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	TriggerVolume->SetupAttachment(Root);
	TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerVolume->SetGenerateOverlapEvents(true);
	TriggerVolume->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);

	BottomPoint = CreateDefaultSubobject<USceneComponent>(TEXT("BottomPoint"));
	BottomPoint->SetupAttachment(Root);

	TopPoint = CreateDefaultSubobject<USceneComponent>(TEXT("TopPoint"));
	TopPoint->SetupAttachment(Root);

	bCanAccess = true;
}

void ALadderActor::BeginPlay()
{
	Super::BeginPlay();

	// Trigger Overlap 바인딩 (bCanClimb, CurrentLadder 설정)
	if (TriggerVolume)
	{
		TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &ALadderActor::OnLadderTriggerBeginOverlap);
		TriggerVolume->OnComponentEndOverlap.AddDynamic(this, &ALadderActor::OnLadderTriggerEndOverlap);
	}

	// 스플라인 포인트가 없으면 Bottom/Top 기반으로 초기화
	if (LadderSpline && LadderSpline->GetNumberOfSplinePoints() < 2)
	{
		const FVector Bottom = BottomPoint ? BottomPoint->GetComponentLocation() : GetActorLocation();
		FVector Top = TopPoint ? TopPoint->GetComponentLocation() : GetActorLocation() + FVector(0.f, 0.f, 300.f);
		LadderSpline->ClearSplinePoints();
		LadderSpline->AddSplinePoint(Bottom, ESplineCoordinateSpace::World);
		LadderSpline->AddSplinePoint(Top, ESplineCoordinateSpace::World);
		LadderSpline->UpdateSpline();
	}

	if (!bIgnorePawnCollision)
	{
		return;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* Component : PrimitiveComponents)
	{
		if (!Component || Component == TriggerVolume)
		{
			continue;
		}

		Component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		if (Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			continue;
		}
	}
}

void ALadderActor::OnLadderTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ABlasterCharacter* BlasterChar = Cast<ABlasterCharacter>(OtherActor))
	{
		BlasterChar->SetCanClimbLadder(true, this);
	}
}

void ALadderActor::OnLadderTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (ABlasterCharacter* BlasterChar = Cast<ABlasterCharacter>(OtherActor))
	{
		BlasterChar->SetCanClimbLadder(false, nullptr);
	}
}

bool ALadderActor::TryStartClimb(ACharacter* Interactor)
{
	// 부착/해제는 BlasterCharacter의 Jump 키(OnJumpAction)로만 처리.
	// Interact 등 다른 경로에서의 자동 부착 비활성화.
	(void)Interactor;
	return false;
}

void ALadderActor::ForceStopClimb(ACharacter* Interactor, bool bPlaceAtTop)
{
	if (ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Interactor))
	{
		BlasterCharacter->StopLadder(bPlaceAtTop);
	}
}

FVector ALadderActor::GetBottomLocation() const
{
	return BottomPoint ? BottomPoint->GetComponentLocation() : GetActorLocation();
}

FVector ALadderActor::GetTopLocation() const
{
	return TopPoint ? TopPoint->GetComponentLocation() : GetActorLocation();
}

FVector ALadderActor::GetLadderUpVector() const
{
	const FVector Bottom = GetBottomLocation();
	const FVector Top = GetTopLocation();
	return (Top - Bottom).GetSafeNormal();
}

FVector ALadderActor::GetLadderForwardVector() const
{
	// 사다리 액터의 Forward = 벽을 향한 방향 (배치 시 설정)
	FVector Forward = GetActorForwardVector();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector(1.f, 0.f, 0.f);
	}
	return Forward.GetSafeNormal();
}

float ALadderActor::GetLadderLength() const
{
	if (LadderSpline && LadderSpline->GetNumberOfSplinePoints() >= 2)
	{
		return LadderSpline->GetSplineLength();
	}
	return FVector::Dist(GetBottomLocation(), GetTopLocation());
}

float ALadderActor::FindDistanceAlongSplineForWorldLocation(const FVector& WorldLocation) const
{
	if (!HasValidSpline())
	{
		return 0.f;
	}
	const float InputKey = LadderSpline->FindInputKeyClosestToWorldLocation(WorldLocation);
	return LadderSpline->GetDistanceAlongSplineAtSplineInputKey(InputKey);
}

FVector ALadderActor::GetLocationAtDistanceAlongSpline(float Distance) const
{
	if (!HasValidSpline())
	{
		return GetActorLocation();
	}
	return LadderSpline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
}

FVector ALadderActor::GetDirectionAtDistanceAlongSpline(float Distance) const
{
	// nullptr 선행 검사 (BP 등에서 LadderSpline 미설정 시 크래시 방지)
	if (LadderSpline == nullptr)
	{
		return GetLadderUpVector();
	}
	if (!HasValidSpline())
	{
		return GetLadderUpVector();
	}
	// 경계 구간에서 UE 스플라인 API 크래시 방지 (EXCEPTION_ACCESS_VIOLATION)
	const float SplineLength = LadderSpline->GetSplineLength();
	constexpr float BoundaryEpsilon = 1.f;
	if (SplineLength <= BoundaryEpsilon || Distance < BoundaryEpsilon || Distance >= SplineLength - BoundaryEpsilon)
	{
		return GetLadderUpVector();
	}
	const FVector Dir = LadderSpline->GetDirectionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
	return Dir.GetSafeNormal(1e-4f);
}

float ALadderActor::GetSplineLength() const
{
	if (!HasValidSpline())
	{
		return GetLadderLength();
	}
	return LadderSpline->GetSplineLength();
}

bool ALadderActor::HasValidSpline() const
{
	return LadderSpline != nullptr && IsValid(LadderSpline) && LadderSpline->GetNumberOfSplinePoints() >= 2;
}
