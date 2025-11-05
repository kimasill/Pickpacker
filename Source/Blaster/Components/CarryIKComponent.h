// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "CarryIKComponent.generated.h"

/**
 * IK 컴포넌트 - 소켓에 부착된 오브젝트를 잡는 IK 처리
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BLASTER_API UCarryIKComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCarryIKComponent();
    
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /**
     * IK 활성화
     */
    UFUNCTION(BlueprintCallable, Category = "Carry IK")
    void EnableIK(AParcelActor* Parcel, const FName& SocketName);

    /**
     * IK 비활성화
     */
    UFUNCTION(BlueprintCallable, Category = "Carry IK")
    void DisableIK();

    /**
     * IK가 활성화되어 있는지 확인
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry IK")
    bool IsIKEnabled() const { return bIKEnabled; }

    /**
     * 왼손 IK 위치 가져오기
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry IK")
    FVector GetLeftHandIKLocation() const { return LeftHandIKLocation; }

    /**
     * 오른손 IK 위치 가져오기
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry IK")
    FVector GetRightHandIKLocation() const { return RightHandIKLocation; }

    /**
     * IK 타겟 위치 가져오기 (Parcel의 소켓 위치)
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry IK")
    FVector GetTargetIKLocation() const { return TargetIKLocation; }

public:
    /** IK 활성화 여부 */
    UPROPERTY(BlueprintReadOnly, Category = "Carry IK")
    bool bIKEnabled = false;

    /** 왼손 IK 위치 */
    UPROPERTY(BlueprintReadOnly, Category = "Carry IK")
    FVector LeftHandIKLocation = FVector::ZeroVector;

    /** 오른손 IK 위치 */
    UPROPERTY(BlueprintReadOnly, Category = "Carry IK")
    FVector RightHandIKLocation = FVector::ZeroVector;

    /** IK 타겟 위치 (Parcel 소켓) */
    UPROPERTY(BlueprintReadOnly, Category = "Carry IK")
    FVector TargetIKLocation = FVector::ZeroVector;

    /** 왼손 오프셋 (소켓로부터) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry IK")
    FVector LeftHandOffset = FVector(-20.0f, 0.0f, 0.0f);

    /** 오른손 오프셋 (소켓로부터) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry IK")
    FVector RightHandOffset = FVector(20.0f, 0.0f, 0.0f);

    /** IK 보간 속도 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry IK")
    float IKInterpSpeed = 10.0f;

protected:
    /**
     * IK 위치 업데이트
     */
    void UpdateIKLocations(float DeltaTime);

private:
    /** 현재 부착된 Parcel */
    UPROPERTY()
    TWeakObjectPtr<AParcelActor> AttachedParcel;

    /** 현재 소켓 이름 */
    UPROPERTY()
    FName CurrentSocketName;

    /** 목표 IK 위치 (보간용) */
    UPROPERTY()
    FVector TargetLeftHandLocation;

    /** 목표 IK 위치 (보간용) */
    UPROPERTY()
    FVector TargetRightHandLocation;
};


