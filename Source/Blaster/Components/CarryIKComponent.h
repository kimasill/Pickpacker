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
     IK 타겟 위치 (Parcel의 소켓 위치)*/

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry IK")
	FTransform GetLeftHandIKTransform() const { return LeftHandIKTransform; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry IK")
	FTransform GetRightHandIKTransform() const { return RightHandIKTransform; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry IK")
	FTransform GetTargetCenterTransform() const { return TargetCenterTransform; }

public:
    
	UPROPERTY(BlueprintReadOnly, Category = "Carry IK")
	FTransform LeftHandIKTransform;

	UPROPERTY(BlueprintReadOnly, Category = "Carry IK")
	FTransform RightHandIKTransform;

    /** IK 타겟 위치 (Parcel 소켓) */

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

    /** 작은 물체 감지 임계값 (cm 단위, 이 크기보다 작으면 작은 물체로 간주) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry IK|Hand Pose")
    float SmallObjectThreshold = 30.0f;

    /** 손 모양 애니메이션 포즈 사용 여부 */
    UPROPERTY(BlueprintReadOnly, Category = "Carry IK|Hand Pose")
    bool bUseHandPoseAnimation = false;

    /** 손 모양 포즈 블렌드 가중치 (0.0 ~ 1.0) */
    UPROPERTY(BlueprintReadOnly, Category = "Carry IK|Hand Pose")
    float HandPoseBlendWeight = 0.0f;

    /** 작은 물체 여부 */
    UPROPERTY(BlueprintReadOnly, Category = "Carry IK|Hand Pose")
    bool bIsSmallObject = false;

    /**
     * 손 모양 포즈 사용 여부 가져오기
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry IK|Hand Pose")
    bool ShouldUseHandPoseAnimation() const { return bUseHandPoseAnimation; }

    /**
     * 손 모양 포즈 블렌드 가중치 가져오기
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry IK|Hand Pose")
    float GetHandPoseBlendWeight() const { return HandPoseBlendWeight; }

    /**
     * 작은 물체 여부 가져오기
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Carry IK|Hand Pose")
    bool IsSmallObject() const { return bIsSmallObject; }

	UFUNCTION()
    void OnRep_IKState();

protected:
    /**
     * IK 위치 업데이트
     */
    void UpdateIKLocations(float DeltaTime);

    /**
     * 물체 크기 계산 및 손 모양 포즈 결정
     */
    void CalculateObjectSizeAndHandPose(AParcelActor* Parcel);

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


