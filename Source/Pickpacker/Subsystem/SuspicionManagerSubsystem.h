// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "PickpackerTypes/PickpackerTypes.h"
#include "SuspicionManagerSubsystem.generated.h"

class ABlasterCharacter;

/**
 * Suspicion Event Data
 */
USTRUCT(BlueprintType)
struct FSuspicionEventData
{
	GENERATED_BODY()

	/** 플레이어가 의심 행동을 한 캐릭터 */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ABlasterCharacter> Player;

	/** 의심 행동 타입 */
	UPROPERTY(BlueprintReadOnly)
	ESuspiciousBehavior Behavior;

	/** 이벤트 발생 시간 */
	UPROPERTY(BlueprintReadOnly)
	float EventTime;

	FSuspicionEventData()
		: Player(nullptr)
		, Behavior(ESuspiciousBehavior::None)
		, EventTime(0.0f)
	{
	}

	FSuspicionEventData(ABlasterCharacter* InPlayer, ESuspiciousBehavior InBehavior, float InEventTime)
		: Player(InPlayer)
		, Behavior(InBehavior)
		, EventTime(InEventTime)
	{
	}
};

USTRUCT(BlueprintType)
struct FPlayerSuspicionState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESuspiciousBehavior Behavior = ESuspiciousBehavior::None;

	// 만료 시각(월드 시간, 초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ExpireTime = 0.f;
};

/**
 * Suspicion Manager Subsystem - 중앙 집중식 의심 행동 이벤트 관리 시스템
 * 플레이어의 의심 행동을 이벤트로 브로드캐스트하고, 드론 등이 구독하여 처리
 */
UCLASS()
class PICKPACKER_API USuspicionManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * 의심 행동 이벤트 브로드캐스트
	 * @param Player - 의심 행동을 한 플레이어
	 * @param Behavior - 의심 행동 타입
	 */
	UFUNCTION(BlueprintCallable, Category = "Suspicion Manager")
	void BroadcastSuspicionEvent(ABlasterCharacter* Player, ESuspiciousBehavior Behavior);

	/**
	 * 드론 등이 이벤트를 구독 (C++용 - 자동으로 함수 바인딩)
	 * @param Subscriber - 구독자 (드론 등)
	 * @param CallbackFunctionName - Blueprint 함수명 (C++에서는 사용 안 함)
	 */
	UFUNCTION(BlueprintCallable, Category = "Suspicion Manager")
	void SubscribeToSuspicionEvents(UObject* Subscriber, FName CallbackFunctionName = NAME_None);

	/**
	 * 구독 해제
	 * @param Subscriber - 구독자
	 */
	UFUNCTION(BlueprintCallable, Category = "Suspicion Manager")
	void UnsubscribeFromSuspicionEvents(UObject* Subscriber);

	/**
	 * 플레이어의 현재 의심 행동 상태 가져오기
	 * @param Player - 플레이어
	 * @return 의심 행동 타입 (없으면 None)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Suspicion Manager")
	ESuspiciousBehavior GetPlayerSuspiciousBehavior(ABlasterCharacter* Player) const;

	/**
	 * 플레이어의 의심 행동 상태 설정 (짧은 시간 유지)
	 * @param Player - 플레이어
	 * @param Behavior - 의심 행동 타입
	 * @param Duration - 유지 시간 (초, 기본값 0.25초)
	 */
	UFUNCTION(BlueprintCallable, Category = "Suspicion Manager")
	void SetPlayerSuspiciousBehavior(ABlasterCharacter* Player, ESuspiciousBehavior Behavior, float Duration = 0.25f);

public:
	/** 이벤트 브로드캐스트 델리게이트 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSuspicionEvent, const FSuspicionEventData&, EventData);
	UPROPERTY(BlueprintAssignable, Category = "Suspicion Manager|Events")
	FOnSuspicionEvent OnSuspicionEvent;

private:
	/** 플레이어별 의심 행동 상태 및 만료 시간 */
	UPROPERTY()
	TMap<TObjectPtr<ABlasterCharacter>, FPlayerSuspicionState> PlayerSuspiciousStates;

	/** 구독자 목록 */
	UPROPERTY()
	TArray<TWeakObjectPtr<UObject>> Subscribers;

	/** 의심 행동 상태 업데이트 (타이머에서 호출) */
	void UpdateSuspiciousStates();

	/** Tick 핸들 */
	FTimerHandle UpdateTimerHandle;

	/** 플레이어별 마지막 이벤트 발생 시간 (중복 방지용) */
	UPROPERTY()
	TMap<TObjectPtr<ABlasterCharacter>, float> LastEventTimeByPlayer;

	/** 같은 행동의 중복 이벤트 방지 시간 (초) */
	UPROPERTY(EditAnywhere, Category = "Suspicion Manager")
	float DuplicateEventCooldown = 2.0f;
};

