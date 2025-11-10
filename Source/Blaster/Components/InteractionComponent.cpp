// Fill out your copyright notice in the Description page of Project Settings.

#include "InteractionComponent.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/Shelf/ShelfActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PrimitiveComponent.h"
#include "Net/UnrealNetwork.h"

UInteractionComponent::UInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    InteractionDistance = 300.0f;
    TraceStartOffset = FVector(0.0f, 0.0f, 50.0f);
    TraceChannel = ECC_Visibility;
    bRequireInteractableInterface = true;
    bDrawDebugTrace = false;

    SetIsReplicatedByDefault(true);
}

void UInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UInteractionComponent, CarriedParcel, COND_OwnerOnly);
}

void UInteractionComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 로컬 컨트롤된 캐릭터에서만 실행
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled())
    {
        return;
    }

    // 오버랩 목록 정리 (유효하지 않은 액터 제거)
    OverlappingActors.RemoveAll([](const TWeakObjectPtr<AActor>& ActorPtr)
    {
        return !ActorPtr.IsValid();
    });

    // 타겟 업데이트 (오버랩 목록 + Line Trace 우선순위)
    UpdateTarget();
}

void UInteractionComponent::PerformTrace(FHitResult& OutHitResult)
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter)
    {
        return;
    }

    APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
    if (!PC)
    {
        return;
    }

    // 카메라에서 Trace 시작
    UCameraComponent* Camera = OwnerCharacter->FindComponentByClass<UCameraComponent>();
    if (!Camera)
    {
        // Fallback: 캐릭터 위치에서
        FVector Start = OwnerCharacter->GetActorLocation() + OwnerCharacter->GetActorRotation().RotateVector(TraceStartOffset);
        FVector Forward = OwnerCharacter->GetActorForwardVector();
        FVector End = Start + Forward * InteractionDistance;

        GetWorld()->LineTraceSingleByChannel(
            OutHitResult,
            Start,
            End,
            TraceChannel
        );

        if (bDrawDebugTrace)
        {
            DrawDebugLine(GetWorld(), Start, End, OutHitResult.bBlockingHit ? FColor::Green : FColor::Red, false, 0.0f, 0, 2.0f);
        }
        return;
    }

    // 화면 중앙에서 Trace
    FVector CameraLocation = Camera->GetComponentLocation();
    FRotator CameraRotation = Camera->GetComponentRotation();
    FVector TraceStart = CameraLocation;
    FVector TraceDirection = CameraRotation.Vector();
    FVector TraceEnd = TraceStart + TraceDirection * InteractionDistance;

    // 액터가 가리는지 확인
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter);

    GetWorld()->LineTraceSingleByChannel(
        OutHitResult,
        TraceStart,
        TraceEnd,
        TraceChannel,
        QueryParams
    );

    if (bDrawDebugTrace)
    {
        DrawDebugLine(GetWorld(), TraceStart, TraceEnd, OutHitResult.bBlockingHit ? FColor::Green : FColor::Red, false, 0.0f, 0, 2.0f);
        if (OutHitResult.bBlockingHit)
        {
            DrawDebugSphere(GetWorld(), OutHitResult.ImpactPoint, 5.0f, 12, FColor::Yellow, false, 0.0f);
        }
    }
}

void UInteractionComponent::UpdateTarget()
{
    // 오버랩 목록에서 상호작용 가능한 액터만 필터링
    TArray<AActor*> ValidOverlappingActors;
    for (const TWeakObjectPtr<AActor>& ActorPtr : OverlappingActors)
    {
        if (ActorPtr.IsValid() && IsActorInteractable(ActorPtr.Get()))
        {
            ValidOverlappingActors.Add(ActorPtr.Get());
        }
    }

    // 타겟 선택
    AActor* NewTarget = nullptr;

    if (bUseLineTracePriority && ValidOverlappingActors.Num() > 0)
    {
        // Line Trace로 우선 타겟 선택
        NewTarget = SelectBestTargetFromOverlap(ValidOverlappingActors);
    }
    else if (ValidOverlappingActors.Num() > 0)
    {
        // Line Trace 비활성화 시 가장 가까운 액터 선택
        ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
        if (OwnerCharacter)
        {
            float ClosestDistance = MAX_FLT;
            FVector OwnerLocation = OwnerCharacter->GetActorLocation();

            for (AActor* Actor : ValidOverlappingActors)
            {
                float Distance = FVector::Dist(OwnerLocation, Actor->GetActorLocation());
                if (Distance < ClosestDistance)
                {
                    ClosestDistance = Distance;
                    NewTarget = Actor;
                }
            }
        }
    }

    // 타겟 변경 확인 및 하이라이트 업데이트
    PreviousTarget = CurrentTarget;

    // 새로운 타겟이 전혀 없으면, 기존 하이라이트를 해제
    if (NewTarget == nullptr)
    {
        if (CurrentTarget.IsValid())
        {
            if (CurrentTarget->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
            {
                IInteractableInterface::Execute_EndHighlight(CurrentTarget.Get());
            }

            OnTargetChanged.Broadcast(PreviousTarget.Get(), nullptr);
        }

        CurrentTarget = nullptr;
        return;
    }

    if (CurrentTarget.Get() != NewTarget)
    {
        // 이전 타겟 하이라이트 해제
        if (CurrentTarget.IsValid())
        {
            if (CurrentTarget->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
            {
                IInteractableInterface::Execute_EndHighlight(CurrentTarget.Get());
            }
        }

        // 새 타겟 설정
        CurrentTarget = NewTarget;

        // 새 타겟 하이라이트
        if (CurrentTarget.IsValid())
        {
            if (CurrentTarget->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
            {
                IInteractableInterface::Execute_StartHighlight(CurrentTarget.Get());
            }
        }

        // 이벤트 브로드캐스트
        OnTargetChanged.Broadcast(PreviousTarget.Get(), CurrentTarget.Get());
    }
}

AActor* UInteractionComponent::SelectBestTargetFromOverlap(const TArray<AActor*>& InActors) const
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter)
    {
        return InActors.Num() > 0 ? InActors[0] : nullptr;
    }

    // 카메라에서 Line Trace 수행
    UCameraComponent* Camera = OwnerCharacter->FindComponentByClass<UCameraComponent>();
    if (!Camera)
    {
        return InActors.Num() > 0 ? InActors[0] : nullptr;
    }

    FVector CameraLocation = Camera->GetComponentLocation();
    FRotator CameraRotation = Camera->GetComponentRotation();
    FVector TraceDirection = CameraRotation.Vector();

    // 각 오버랩 액터에 대해 Line Trace 수행
    AActor* BestTarget = nullptr;
    float ClosestHitDistance = MAX_FLT;

    for (AActor* Actor : InActors)
    {
        if (!Actor || !IsValid(Actor))
        {
            continue;
        }

        const float Distance = FVector::Dist(CameraLocation, Actor->GetActorLocation());
        if (Distance > InteractionDistance)
        {
            continue;
        }

        // 시야 각도 체크 (정면에 있는지)
        const FVector ToActorDir = (Actor->GetActorLocation() - CameraLocation).GetSafeNormal();
        const float DotProduct = FVector::DotProduct(TraceDirection, ToActorDir);
        
        if (DotProduct > 0.5f && Distance < ClosestHitDistance)
        {
            ClosestHitDistance = Distance;
            BestTarget = Actor;
        }
    }

    return BestTarget ? BestTarget : (InActors.Num() > 0 ? InActors[0] : nullptr);
}

void UInteractionComponent::AddOverlappingActor(AActor* Actor)
{
    if (!Actor || !IsValid(Actor))
    {
        return;
    }

    // 이미 목록에 있으면 무시
    if (OverlappingActors.Contains(Actor))
    {
        return;
    }

    OverlappingActors.Add(Actor);
}

void UInteractionComponent::RemoveOverlappingActor(AActor* Actor)
{
    OverlappingActors.RemoveAllSwap([Actor](const TWeakObjectPtr<AActor>& ActorPtr)
    {
        return !ActorPtr.IsValid() || ActorPtr.Get() == Actor;
    });
}

TArray<AActor*> UInteractionComponent::GetOverlappingActors() const
{
    TArray<AActor*> Result;
    for (const TWeakObjectPtr<AActor>& ActorPtr : OverlappingActors)
    {
        if (ActorPtr.IsValid())
        {
            Result.Add(ActorPtr.Get());
        }
    }
    return Result;
}

void UInteractionComponent::SetCarriedParcel(AParcelActor* NewParcel)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (CarriedParcel == NewParcel)
	{
		return;
	}

	AParcelActor* LastParcel = CarriedParcel;
	CarriedParcel = NewParcel;
	HandleCarriedParcelChanged(LastParcel);
}

void UInteractionComponent::OnRep_CarriedParcel(AParcelActor* LastParcel)
{
	HandleCarriedParcelChanged(LastParcel);
}

void UInteractionComponent::HandleCarriedParcelChanged(AParcelActor* LastParcel)
{
	const FString LastName = LastParcel ? LastParcel->GetName() : TEXT("None");
	const FString NewName = CarriedParcel ? CarriedParcel->GetName() : TEXT("None");
	UE_LOG(LogTemp, Log, TEXT("[InteractionComponent] Carried parcel changed %s -> %s"),
		*LastName, *NewName);
}

bool UInteractionComponent::IsActorInteractable(AActor* Actor) const
{
    if (!Actor)
    {
        return false;
    }

    // 액터 클래스 필터 확인
    if (AllowedActorClasses.Num() > 0)
    {
        bool bIsAllowed = false;
        for (TSubclassOf<AActor> AllowedClass : AllowedActorClasses)
        {
            if (Actor->IsA(AllowedClass))
            {
                bIsAllowed = true;
                break;
            }
        }
        if (!bIsAllowed)
        {
            return false;
        }
    }

    // 인터페이스 확인
    if (bRequireInteractableInterface)
    {
        if (!Actor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
        {
            return false;
        }
    }

    // CanInteract 확인
    if (IInteractableInterface* Interface = Cast<IInteractableInterface>(Actor))
    {
        ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
        if (OwnerCharacter)
        {
            return Interface->Execute_CanInteract(Actor, OwnerCharacter);
        }
    }

    return true;
}

void UInteractionComponent::Interact()
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter) return;

    // 들고 있는 Parcel이 있으면 Drop 로직 실행
    if (IsValid(CarriedParcel))
    {
        AParcelActor* Parcel = CarriedParcel;
        
        // ShelfActor가 타겟이면 선반에 배치
        if (CurrentTarget.IsValid())
        {
            if (AShelfActor* Shelf = Cast<AShelfActor>(CurrentTarget.Get()))
            {
                // 선반에 배치 시도
                if (OwnerCharacter->HasAuthority())
                {
                    if (Shelf->TryPlaceParcel(Parcel))
                    {
                        SetCarriedParcel(nullptr);
                        OnInteractSuccess.Broadcast(Shelf);
                        return;
                    }
                }
                else
                {
                    Server_Interact(Shelf);
                    return;
                }
            }
        }

        // 일반 Drop
        if (Parcel)
        {
            FVector Pulse = OwnerCharacter->GetActorForwardVector() * DropImpulse;
            Parcel->RequestDrop(Pulse);
			if (OwnerCharacter->HasAuthority())
			{
				SetCarriedParcel(nullptr);
			}
        }
        return;
    }

    // Parcel을 들고 있지 않으면 Pickup 로직 실행
    if (!CanInteract()) return;

    AActor* Target = CurrentTarget.Get();
    if (!Target) return;

    if (OwnerCharacter->HasAuthority())
    {
        const bool bSuccess = PerformInteract(Target, OwnerCharacter);
        if (bSuccess)
        {
            // Parcel을 집었으면 추적
        if (AParcelActor* Parcel = Cast<AParcelActor>(Target))
        {
            if (Parcel->IsAttached())
            {
                SetCarriedParcel(Parcel);
            }
        }
            OnInteractSuccess.Broadcast(Target);
        }
    }
    else
    {
        Server_Interact(Target);
    }
}

void UInteractionComponent::Server_Interact_Implementation(AActor* Target)
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter) return;

    // ShelfActor에 배치하는 경우
    if (IsValid(CarriedParcel))
    {
        if (AShelfActor* Shelf = Cast<AShelfActor>(Target))
        {
            AParcelActor* Parcel = CarriedParcel;
            if (Shelf->TryPlaceParcel(Parcel))
            {
                SetCarriedParcel(nullptr);
            }
            return;
        }
    }

    const bool bSuccess = PerformInteract(Target, OwnerCharacter);
    if (bSuccess)
    {
        // Parcel을 집었으면 추적
        if (AParcelActor* Parcel = Cast<AParcelActor>(Target))
        {
            if (Parcel->IsAttached() && OwnerCharacter->HasAuthority())
            {
                SetCarriedParcel(Parcel);
            }
        }
    }
}

bool UInteractionComponent::CanInteract() const
{
    if (!CurrentTarget.IsValid())
    {
        return false;
    }

    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter)
    {
        return false;
    }

    if (IInteractableInterface* Interface = Cast<IInteractableInterface>(CurrentTarget.Get()))
    {
        return Interface->Execute_CanInteract(CurrentTarget.Get(), OwnerCharacter);
    }

    return false;
}

bool UInteractionComponent::PerformInteract(AActor* Target, ACharacter* OwnerCharacter)
{
    if (!Target || !OwnerCharacter) return false;
    if (!Target->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass())) return false;
     
    return IInteractableInterface::Execute_OnInteract(Target, OwnerCharacter);
}

