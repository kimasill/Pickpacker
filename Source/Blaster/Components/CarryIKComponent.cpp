// Fill out your copyright notice in the Description page of Project Settings.

#include "CarryIKComponent.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/Components/CarryPointsComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Engine/StaticMeshSocket.h" // 선택: 포인터 미사용이면 생략 가능
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"

UCarryIKComponent::UCarryIKComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(true);
    bIKEnabled = false;
    LeftHandIKTransform = FTransform::Identity;
    RightHandIKTransform = FTransform::Identity;
	TargetCenterTransform = FTransform::Identity;
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
    DOREPLIFETIME(UCarryIKComponent, CurrentGripType);
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
    RefreshGripType();

    UpdateIKLocations(0.0f);
}

void UCarryIKComponent::DisableIK()
{
    bIKEnabled = false;
    AttachedParcel = nullptr;
    CurrentSocketName = NAME_None;
    LeftHandIKTransform = FTransform::Identity;
    RightHandIKTransform = FTransform::Identity;
    TargetCenterTransform = FTransform::Identity;
    bHasLeftHandTarget = false;
    bHasRightHandTarget = false;
    CurrentGripType = EGripType::None;
}

void UCarryIKComponent::OnRep_IKState()
{
    RefreshGripType();

    if (bIKEnabled && AttachedParcel)
    {
        UpdateIKLocations(0.0f);

        UE_LOG(LogTemp, Log, TEXT("[CarryIK] OnRep_IKState: IK Enabled (Client) for Parcel %s | Socket %s"),
            *AttachedParcel->GetName(), *CurrentSocketName.ToString());
    }
    else
    {
        LeftHandIKTransform = FTransform::Identity;
        RightHandIKTransform = FTransform::Identity;
        TargetCenterTransform = FTransform::Identity;
        bHasLeftHandTarget = false;
        bHasRightHandTarget = false;

        UE_LOG(LogTemp, Log, TEXT("[CarryIKComponent] IK Disabled (Client)"));
    }
}

void UCarryIKComponent::UpdateIKLocations(float DeltaTime)
{
    bHasLeftHandTarget = false;
    bHasRightHandTarget = false;

    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter) { DisableIK();  return; }

    if (!AttachedParcel) { DisableIK(); return; }
    AParcelActor* Parcel = AttachedParcel.Get();
    if (!Parcel) { DisableIK(); return; }

    FTransform CenterTransform = FTransform::Identity;
    FTransform LTransform = FTransform::Identity;
    FTransform RTransform = FTransform::Identity;
    bool bLeftOk = false;
    bool bRightOk = false;
    bool bGotTargets = false;

    if (bUseParcelCarryPoints)
    {
        if (UStaticMeshComponent* ParcelMesh = Parcel->GetParcelMesh())
        {
            if (ParcelMesh->DoesSocketExist(LeftHandleName))
            {
                LTransform = ParcelMesh->GetSocketTransform(LeftHandleName, ERelativeTransformSpace::RTS_World);
                bLeftOk = true;
            }
            else
            {
                UE_LOG(LogTemp, Verbose, TEXT("[CarryIK] Left handle socket '%s' not found on Parcel '%s'"),
                    *LeftHandleName.ToString(), *Parcel->GetName());
            }

            if (ParcelMesh->DoesSocketExist(RightHandleName))
            {
                RTransform = ParcelMesh->GetSocketTransform(RightHandleName, ERelativeTransformSpace::RTS_World);
                bRightOk = true;
            }
            else
            {
                UE_LOG(LogTemp, Verbose, TEXT("[CarryIK] Right handle socket '%s' not found on Parcel '%s'"),
                    *RightHandleName.ToString(), *Parcel->GetName());
            }

            if (bLeftOk || bRightOk)
            {
                if (bLeftOk && bRightOk)
                {
                    const FVector CenterLocation = (LTransform.GetLocation() + RTransform.GetLocation()) * 0.5f;
                    CenterTransform.SetLocation(CenterLocation);
                    CenterTransform.SetRotation(FQuat::Identity);
                }
                else if (bLeftOk)
                {
                    CenterTransform = LTransform;
                }
                else if (bRightOk)
                {
                    CenterTransform = RTransform;
                }

                bGotTargets = true;
            }
        }
    }

    if (!bGotTargets)
    {
        if (USkeletalMeshComponent* CharacterMesh = OwnerCharacter->GetMesh())
        {
            const ABlasterCharacter* BlasterOwner = Cast<ABlasterCharacter>(OwnerCharacter);
            if (!BlasterOwner)
            {
                DisableIK();
                return;
            }

            const FName TargetSocket = CurrentSocketName.IsNone()
                ? BlasterOwner->GetCarrySocketName()
                : CurrentSocketName;

            const FTransform SocketTransform = CharacterMesh->GetSocketTransform(TargetSocket, RTS_World);

            const FVector SocketLocation = SocketTransform.GetLocation();
            const FRotator SocketRotation = SocketTransform.Rotator();

            const FVector WorldLeftOffset = SocketRotation.RotateVector(LeftHandOffset);
            const FVector WorldRightOffset = SocketRotation.RotateVector(RightHandOffset);

            const FVector LeftHandLocation = SocketLocation + WorldLeftOffset;
            const FVector RightHandLocation = SocketLocation + WorldRightOffset;

            LTransform = FTransform(SocketRotation, LeftHandLocation);
            RTransform = FTransform(SocketRotation, RightHandLocation);
            CenterTransform = SocketTransform;

            bLeftOk = true;
            bRightOk = true;
            bGotTargets = true;
        }
    }

    if (!bGotTargets)
    {
        LeftHandIKTransform = FTransform::Identity;
        RightHandIKTransform = FTransform::Identity;
        TargetCenterTransform = FTransform::Identity;
        return;
    }

    LeftHandIKTransform = bLeftOk ? LTransform : FTransform::Identity;
    RightHandIKTransform = bRightOk ? RTransform : FTransform::Identity;
    TargetCenterTransform = CenterTransform;
    bHasLeftHandTarget = bLeftOk;
    bHasRightHandTarget = bRightOk;
}

void UCarryIKComponent::RefreshGripType()
{
    if (!AttachedParcel)
    {
        CurrentGripType = EGripType::None;
        return;
    }

    const AActor* OwnerActor = GetOwner();
    if (!OwnerActor || !OwnerActor->HasAuthority())
    {
        return;
    }

    if (AttachedParcel->IsItem())
    {
        CurrentGripType = AttachedParcel->GetItemData().GripType;
        return;
    }

    CurrentGripType = AttachedParcel->RequiresTwoHandCarry() ? EGripType::Box : EGripType::Handle;
}


