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
        // 1) 정적 메시 소켓 우선 - 소켓의 Transform을 그대로 사용
        if (UStaticMeshComponent* ParcelMesh = Parcel->GetParcelMesh())
        {
            bool bLeftOk = false, bRightOk = false;

            if (ParcelMesh->DoesSocketExist(LeftHandleName))
            {
                // 소켓의 Transform을 그대로 사용 (위치 + 회전)
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
                // 소켓의 Transform을 그대로 사용 (위치 + 회전)
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
                // 회전 계산 제거 - 소켓의 회전을 그대로 사용
                // FABRIK IK는 위치만 조정하고, 회전은 소켓의 회전을 그대로 사용
                
                if (!bLeftOk) 
                {
					LTransform = FTransform::Identity;
                }
                if (!bRightOk) 
                {
					RTransform = FTransform::Identity;
                }
                
                // 중심 위치 계산
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

            // 오프셋을 소켓의 로컬 공간에서 월드 공간으로 변환
            const FVector WorldLeftOffset = SocketRotation.RotateVector(LeftHandOffset);
            const FVector WorldRightOffset = SocketRotation.RotateVector(RightHandOffset);

            const FVector LeftHandLocation = SocketLocation + WorldLeftOffset;
            const FVector RightHandLocation = SocketLocation + WorldRightOffset;
            
            // 소켓의 회전을 그대로 사용 (회전 계산 제거)
            // 위치만 오프셋 적용하고, 회전은 소켓의 회전 유지
            LTransform = FTransform(SocketRotation, LeftHandLocation);
            RTransform = FTransform(SocketRotation, RightHandLocation);
            CenterTransform = SocketTransform;

            bGotTargets = true;
        }
    }
    if (!bGotTargets) return;

    // 물체 크기 계산 및 작은 물체 감지
    CalculateObjectSizeAndHandPose(Parcel);

    LeftHandIKTransform = LTransform;
	RightHandIKTransform = RTransform;
	TargetCenterTransform = CenterTransform;
}

// 물체 크기를 계산하고 손 포즈 사용 여부/블렌드 설정
void UCarryIKComponent::CalculateObjectSizeAndHandPose(AParcelActor* Parcel)
{
    bUseHandPoseAnimation = false;
    HandPoseBlendWeight = 0.0f;
    bIsSmallObject = false;

    if (!Parcel)
    {
        return;
    }

    const UStaticMeshComponent* ParcelMesh = Parcel->GetParcelMesh();
    if (!ParcelMesh)
    {
        return;
    }

    // StaticMeshComponent의 bounds를 사용하여 물체의 대략적인 크기를 계산
    const FBoxSphereBounds Bounds = ParcelMesh->CalcBounds(ParcelMesh->GetComponentTransform());
    const FVector Extents = Bounds.BoxExtent * 2.0f; // BoxExtent는 half-size, 전체 크기로 변환

    // 가장 큰 변 길이를 기준으로 작은 물체 여부 판정
    const float MaxDimension = FMath::Max3(Extents.X, Extents.Y, Extents.Z);
    bIsSmallObject = MaxDimension <= SmallObjectThreshold;

    // 작은 물체면 손 포즈 애니메이션 활성화, 블렌드 가중치 상향
    if (bIsSmallObject)
    {
        bUseHandPoseAnimation = true;
        HandPoseBlendWeight = 1.0f; // 완전한 손 포즈
    }
    else
    {
        // 큰 물체는 손 포즈를 끄거나 낮은 블렌드로 설정
        bUseHandPoseAnimation = false;
        HandPoseBlendWeight = 0.0f;
    }
}


