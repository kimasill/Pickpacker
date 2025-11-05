// Fill out your copyright notice in the Description page of Project Settings.

#include "CarryIKComponent.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/Components/CarryPointsComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"

UCarryIKComponent::UCarryIKComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
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

void UCarryIKComponent::UpdateIKLocations(float DeltaTime)
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter)
    {
        DisableIK();
        return;
    }

    if (!AttachedParcel.IsValid())
    {
        DisableIK();
        return;
    }

    AParcelActor* Parcel = AttachedParcel.Get();
    if (!Parcel)
    {
        DisableIK();
        return;
    }

    // Parcel 소켓 위치
    USkeletalMeshComponent* CharacterMesh = OwnerCharacter->GetMesh();
    if (!CharacterMesh)
    {
        return;
    }

    FTransform SocketTransform = CharacterMesh->GetSocketTransform(CurrentSocketName, RTS_World);
    FVector SocketLocation = SocketTransform.GetLocation();
    FRotator SocketRotation = SocketTransform.Rotator();

    // 타겟 위치 계산 (소켓 위치에서 오프셋 적용)
    FVector WorldLeftOffset = SocketRotation.RotateVector(LeftHandOffset);
    FVector WorldRightOffset = SocketRotation.RotateVector(RightHandOffset);

    TargetLeftHandLocation = SocketLocation + WorldLeftOffset;
    TargetRightHandLocation = SocketLocation + WorldRightOffset;

    // 보간
    float InterpAlpha = FMath::Clamp(IKInterpSpeed * DeltaTime, 0.0f, 1.0f);
    LeftHandIKLocation = UKismetMathLibrary::VInterpTo(LeftHandIKLocation, TargetLeftHandLocation, DeltaTime, IKInterpSpeed);
    RightHandIKLocation = UKismetMathLibrary::VInterpTo(RightHandIKLocation, TargetRightHandLocation, DeltaTime, IKInterpSpeed);

    TargetIKLocation = SocketLocation;
}


