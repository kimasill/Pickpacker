// Fill out your copyright notice in the Description page of Project Settings.

#include "CarryIKComponent.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/Components/CarryPointsComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Blaster\Character\BlasterCharacter.h"
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
	LeftHandIKTransform = FTransform::Identity;
	RightHandIKTransform = FTransform::Identity;
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
		LeftHandIKTransform = FTransform::Identity;
		RightHandIKTransform = FTransform::Identity;

        UE_LOG(LogTemp, Log, TEXT("[CarryIKComponent] IK Disabled (Client)"));
    }
}

void UCarryIKComponent::UpdateIKLocations(float DeltaTime)
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter) { DisableIK();  return; }

    if (!AttachedParcel) { DisableIK(); return; }
    AParcelActor* Parcel = AttachedParcel.Get();
    if (!Parcel) { DisableIK(); return; }

    FTransform CenterTransform = FTransform::Identity;
	FTransform LTransform = FTransform::Identity;
	FTransform RTransform = FTransform::Identity;
    

    bool bGotTargets = false;

    if (bUseParcelCarryPoints)
    {
        // 1) 정적 메시 소켓 우선
        if (UStaticMeshComponent* ParcelMesh = Parcel->GetParcelMesh())
        {
            bool bLeftOk = false, bRightOk = false;

            if (ParcelMesh->DoesSocketExist(LeftHandleName))
            {
                LTransform = ParcelMesh->GetSocketTransform(LeftHandleName, ERelativeTransformSpace::RTS_World);
                bLeftOk = true;
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[CarryIK] Left handle socket '%s' not found on Parcel '%s'"),
                    *LeftHandleName.ToString(), *Parcel->GetName());
            }

            if (ParcelMesh->DoesSocketExist(RightHandleName))
            {
				RTransform = ParcelMesh->GetSocketTransform(RightHandleName, ERelativeTransformSpace::RTS_World);
                bRightOk = true;
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[CarryIK] Right handle socket '%s' not found on Parcel '%s'"),
                    *RightHandleName.ToString(), *Parcel->GetName());
            }

            if (bLeftOk || bRightOk)
            {
                if (!bLeftOk) 
                {
					LTransform = FTransform::Identity;
                }
                if (!bRightOk) 
                {
					RTransform = FTransform::Identity;
                }				
                bGotTargets = true;
            }
        }
    }

    // 2) 폴백: 캐릭터 소켓 + 오프셋
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

            // 손바닥이 중앙을 향하도록 회전 계산
			const FVector ToCenter = (SocketLocation - (SocketLocation + WorldLeftOffset)).GetSafeNormal();
			const FRotator LeftHandRotation = UKismetMathLibrary::MakeRotFromX(ToCenter);
			const FRotator RightHandRotation = UKismetMathLibrary::MakeRotFromX(-ToCenter);
			LTransform = FTransform(LeftHandRotation, SocketLocation + WorldLeftOffset);
			RTransform = FTransform(RightHandRotation, SocketLocation + WorldRightOffset);
			CenterTransform = SocketTransform;

            bGotTargets = true;
        }
    }
    if (!bGotTargets) return;

    LeftHandIKTransform = LTransform;
	RightHandIKTransform = RTransform;
	TargetCenterTransform = CenterTransform;
}


