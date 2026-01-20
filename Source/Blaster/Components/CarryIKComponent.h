// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blaster/DataAssets/DA_ItemData.h"
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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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
     IK 타겟 위치 (캐릭터 스켈레탈 메시 기준 Component Space) */

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry IK")
	FTransform GetLeftHandIKTransform() const { return LeftHandIKTransform; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry IK")
	FTransform GetRightHandIKTransform() const { return RightHandIKTransform; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry IK")
	FTransform GetTargetCenterTransform() const { return TargetCenterTransform; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry IK")
	bool HasLeftHandTarget() const { return bHasLeftHandTarget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry IK")
	bool HasRightHandTarget() const { return bHasRightHandTarget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry IK|Grip")
	EGripType GetCurrentGripType() const { return CurrentGripType; }

public:
    
	UPROPERTY(BlueprintReadOnly, Category = "Carry IK")
	FTransform LeftHandIKTransform;

	UPROPERTY(BlueprintReadOnly, Category = "Carry IK")
	FTransform RightHandIKTransform;

    /** IK 타겟 위치 (캐릭터 스켈레탈 메시 기준 Component Space) */

    /** 왼손 오프셋 (소켓로부터) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry IK")
    FVector LeftHandOffset = FVector(-20.0f, 0.0f, 0.0f);

    /** 오른손 오프셋 (소켓로부터) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry IK")
    FVector RightHandOffset = FVector(20.0f, 0.0f, 0.0f);

	/** 타겟 중심 위치 오프셋 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry IK")
    FTransform TargetCenterTransform;

    /** IK 보간 속도 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry IK")
    float IKInterpSpeed = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry IK")
    bool bUseParcelCarryPoints = true;

    /** 핸들 소켓 
    */

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry IK")
    FName LeftHandleName = FName("CarryPoint_Left");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry IK")
    FName RightHandleName = FName("CarryPoint_Right");

    UPROPERTY(BlueprintReadOnly, Category = "Carry IK")
    bool bHasLeftHandTarget = false;

    UPROPERTY(BlueprintReadOnly, Category = "Carry IK")
    bool bHasRightHandTarget = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Carry IK|Grip")
    EGripType CurrentGripType = EGripType::None;

    UFUNCTION()
    void OnRep_IKState();

protected:
    /**
     * IK 위치 업데이트
     */
    void UpdateIKLocations(float DeltaTime);

    void RefreshGripType();

private:
    /** 현재 부착된 Parcel */
    UPROPERTY(ReplicatedUsing = OnRep_IKState)
    TObjectPtr<AParcelActor> AttachedParcel;

        /** IK 활성화 여부 */
    UPROPERTY(ReplicatedUsing = OnRep_IKState)
    bool bIKEnabled = false;

    /** 현재 소켓 이름 */
    UPROPERTY(ReplicatedUsing=OnRep_IKState)
    FName CurrentSocketName;
};


