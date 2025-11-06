// Fill out your copyright notice in the Description page of Project Settings.

#include "CarryIKComponent.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/Components/CarryPointsComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMeshSocket.h" // 선택: 포인터 미사용이면 생략 가능
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"

UCarryIKComponent::UCarryIKComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
    bIKEnabled = false;
    LeftHandIKLocation = FVector::ZeroVector;
    RightHandIKLocation = FVector::ZeroVector;
    TargetIKLocation = FVector::ZeroVector;
    LeftHandOffset = FVector(-20.0f, 0.0f, 0.0f);
    RightHandOffset = FVector(20.0f, 0.0f, 0.0f);
    IKInterpSpeed = 10.0f;
}

void UCarryIKComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UCarryIKComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCarryIKComponent, bIKEnabled);
    DOREPLIFETIME(UCarryIKComponent, AttachedParcel);
	DOREPLIFETIME(UCarryIKComponent, CurrentSocketName);
}

void UCarryIKComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIKEnabled)
    {
        UpdateIKLocations(DeltaTime);
    }
}

void UCarryIKComponent::EnableIK(AParcelActor* Parcel, const FName& SocketName)
{
    if (!Parcel)
    {
        return;
    }

    AttachedParcel = Parcel;
    CurrentSocketName = SocketName;
    bIKEnabled = true;

    // 초기 위치 설정
    UpdateIKLocations(0.0f);
}

void UCarryIKComponent::DisableIK()
{
    bIKEnabled = false;
    AttachedParcel = nullptr;
    CurrentSocketName = NAME_None;
    LeftHandIKLocation = FVector::ZeroVector;
    RightHandIKLocation = FVector::ZeroVector;
    TargetIKLocation = FVector::ZeroVector;
}

FVector UCarryIKComponent::GetLeftHandIKLocationInBoneSpace() const
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter || !OwnerCharacter->GetMesh()) { return FVector(); }

	USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
    FVector OutPosition;
	FRotator OutRotation;
	Mesh->TransformToBoneSpace(FName("hand_l"), LeftHandIKLocation, FRotator::ZeroRotator, OutPosition, OutRotation);
	return OutPosition;
}

FVector UCarryIKComponent::GetRightHandIKLocationInBoneSpace() const
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter || !OwnerCharacter->GetMesh()) { return FVector(); }
    USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
    FVector OutPosition;
    FRotator OutRotation;
    Mesh->TransformToBoneSpace(FName("hand_r"), RightHandIKLocation, FRotator::ZeroRotator, OutPosition, OutRotation);
    return OutPosition;
}

void UCarryIKComponent::OnRep_IKState()
{
    if (bIKEnabled && AttachedParcel)
    {
        UpdateIKLocations(0.0f);

        UE_LOG(LogTemp, Log, TEXT("[CarryIK] OnRep_IKState: IK Enabled (Client) for Parcel %s | Socket %s"),
            *AttachedParcel->GetName(), *CurrentSocketName.ToString());
    }
    else
    {
        LeftHandIKLocation = FVector::ZeroVector;
        RightHandIKLocation = FVector::ZeroVector;
        TargetIKLocation = FVector::ZeroVector;

        UE_LOG(LogTemp, Log, TEXT("[CarryIKComponent] IK Disabled (Client)"));
    }
}

void UCarryIKComponent::UpdateIKLocations(float DeltaTime)
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter) { return; }

    if (!AttachedParcel) { return; }
    AParcelActor* Parcel = AttachedParcel.Get();
    if (!Parcel) { return; }

    FVector LTarget = FVector::ZeroVector;
    FVector RTarget = FVector::ZeroVector;
    FVector CenterTarget = FVector::ZeroVector;
    bool bGotTargets = false;

    if (bUseParcelCarryPoints)
    {
        // 1) 정적 메시 소켓 우선
        if (UStaticMeshComponent* ParcelMesh = Cast<UStaticMeshComponent>(Parcel->GetRootComponent()))
        {
            bool bLeftOk = false, bRightOk = false;

            if (ParcelMesh->DoesSocketExist(LeftHandleName))
            {
                LTarget = ParcelMesh->GetSocketLocation(LeftHandleName);
                bLeftOk = true;
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[CarryIK] Left handle socket '%s' not found on Parcel '%s'"),
                    *LeftHandleName.ToString(), *Parcel->GetName());
            }

            if (ParcelMesh->DoesSocketExist(RightHandleName))
            {
                RTarget = ParcelMesh->GetSocketLocation(RightHandleName);
                bRightOk = true;
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[CarryIK] Right handle socket '%s' not found on Parcel '%s'"),
                    *RightHandleName.ToString(), *Parcel->GetName());
            }

            if (bLeftOk || bRightOk)
            {
                if (!bLeftOk) LTarget = RTarget;
                if (!bRightOk) RTarget = LTarget;
                CenterTarget = (LTarget + RTarget) * 0.5f;
                bGotTargets = true;
            }
        }
        // 2) 폴백: CarryPointsComponent 사용
        if (!bGotTargets)
        {
            if (UCarryPointsComponent* CarryPoints = Parcel->GetCarryPointsComponent())
            {
                LTarget = CarryPoints->GetSocketWorldLocation(LeftHandleName);
                RTarget = CarryPoints->GetSocketWorldLocation(RightHandleName);
                CenterTarget = (LTarget + RTarget) * 0.5f;
                bGotTargets = true;
            }
        }
    }

    // 3) 최종 폴백: 캐릭터 소켓 + 오프셋
    if (!bGotTargets)
    {
        if (USkeletalMeshComponent* CharacterMesh = OwnerCharacter->GetMesh())
        {
            const FTransform SocketTransform = CharacterMesh->GetSocketTransform(CurrentSocketName, RTS_World);
            const FVector SocketLocation = SocketTransform.GetLocation();
            const FRotator SocketRotation = SocketTransform.Rotator();

            const FVector WorldLeftOffset = SocketRotation.RotateVector(LeftHandOffset);
            const FVector WorldRightOffset = SocketRotation.RotateVector(RightHandOffset);

            LTarget = SocketLocation + WorldLeftOffset;
            RTarget = SocketLocation + WorldRightOffset;
            CenterTarget = SocketLocation;
            bGotTargets = true;
        }
    }

    if (!bGotTargets) return;

    // 보간
    LeftHandIKLocation = UKismetMathLibrary::VInterpTo(LeftHandIKLocation, LTarget, DeltaTime, IKInterpSpeed);
    RightHandIKLocation = UKismetMathLibrary::VInterpTo(RightHandIKLocation, RTarget, DeltaTime, IKInterpSpeed);
    TargetIKLocation = CenterTarget;
}


