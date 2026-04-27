// Fill out your copyright notice in the Description page of Project Settings.

#include "SuspicionManagerSubsystem.h"
#include "Character/BlasterCharacter.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "AI/DroneActor.h"

namespace
{
	static float GetSuspicionWeightForBehavior(const ESuspiciousBehavior Behavior)
	{
		switch (Behavior)
		{
		case ESuspiciousBehavior::DisassemblingParcel:
			return 1.25f;
		case ESuspiciousBehavior::EnteringRestrictedZone:
			return 2.0f;
		case ESuspiciousBehavior::PickingPackagedParcel:
			return 1.0f;
		case ESuspiciousBehavior::DroppingParcel:
			return 1.5f;
		case ESuspiciousBehavior::HoldingSuspiciousItem:
			return 1.75f;
		case ESuspiciousBehavior::HoldingWeapon:
			return 2.5f;
		case ESuspiciousBehavior::InteractingRestrictedSystem:
			return 2.25f;
		case ESuspiciousBehavior::OtherViolation:
			return 1.0f;
		default:
			return 0.0f;
		}
	}
}

void USuspicionManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("[SuspicionManagerSubsystem] Initialized"));

	// Tick 대신 타이머 사용 (0.1초 간격)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			UpdateTimerHandle,
			this,
			&USuspicionManagerSubsystem::UpdateSuspiciousStates,
			0.1f,
			true
		);
	}
}

void USuspicionManagerSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UpdateTimerHandle.IsValid())
		{
			World->GetTimerManager().ClearTimer(UpdateTimerHandle);
		}
	}

	PlayerSuspiciousStates.Empty();
	Subscribers.Empty();
	LastEventTimeByPlayer.Empty();

	Super::Deinitialize();
}

void USuspicionManagerSubsystem::BroadcastSuspicionEvent(ABlasterCharacter* Player, ESuspiciousBehavior Behavior)
{
	if (!Player || Behavior == ESuspiciousBehavior::None)
	{
		return;
	}

	// 서버에서만 브로드캐스트
	if (!GetWorld() || !GetWorld()->GetAuthGameMode())
	{
		return;
	}

	float CurrentTime = GetWorld()->GetTimeSeconds();
	
	// 중복 이벤트 방지: 같은 플레이어가 짧은 시간 내에 같은 행동을 반복하면 무시
	float* LastEventTime = LastEventTimeByPlayer.Find(Player);
	if (LastEventTime && (CurrentTime - *LastEventTime) < DuplicateEventCooldown)
	{
		// 같은 행동이 짧은 시간 내에 반복됨 - 이벤트는 브로드캐스트하지 않지만 상태는 갱신
		SetPlayerSuspiciousBehavior(Player, Behavior, 0.25f);
		UE_LOG(LogTemp, VeryVerbose, TEXT("[SuspicionManagerSubsystem] Duplicate event ignored - Player: %s, Behavior: %s (cooldown: %.2f)"), 
			*Player->GetName(), *UEnum::GetValueAsString(Behavior), DuplicateEventCooldown);
		return;
	}

	// 마지막 이벤트 시간 업데이트
	LastEventTimeByPlayer.Add(Player, CurrentTime);

	const float EventWeight = GetSuspicionWeightForBehavior(Behavior);
	const float ExpireTime = CurrentTime + FMath::Max(0.1f, DefaultEventLifetime);
	FSuspicionEventData EventData(
		Player,
		Behavior,
		CurrentTime,
		Player->GetFName(),
		Player->GetActorLocation(),
		EventWeight,
		ExpireTime);

	UE_LOG(LogTemp, Log, TEXT("[SuspicionManagerSubsystem] Broadcasting suspicion event - Player: %s, Behavior: %s, Weight: %.2f"),
		*Player->GetName(), *UEnum::GetValueAsString(Behavior), EventWeight);

	// 델리게이트 브로드캐스트
	OnSuspicionEvent.Broadcast(EventData);	

	// 플레이어 상태 설정 (짧은 시간 유지)
	SetPlayerSuspiciousBehavior(Player, Behavior, 0.25f);
}

void USuspicionManagerSubsystem::SubscribeToSuspicionEvents(UObject* Subscriber, FName CallbackFunctionName)
{
	if (!Subscriber)
	{
		return;
	}

	// 이미 구독 중인지 확인
	if (Subscribers.Contains(Subscriber))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SuspicionManagerSubsystem] Subscriber %s already subscribed"), *Subscriber->GetName());
		return;
	}

	Subscribers.Add(Subscriber);

	// 기본 콜백 이름 지정: C++ 핸들러 `OnSuspicionEventReceived`
	if (CallbackFunctionName.IsNone())
	{
		CallbackFunctionName = FName(TEXT("OnSuspicionEventReceived"));
	}

	// 유효한 바인드 가능한 함수인지 확인
	const UFunction* Func = Subscriber->FindFunction(CallbackFunctionName);
	if (!Func)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SuspicionManagerSubsystem] Subscriber %s does not implement function %s. Suspicion events will not be received."),
			*Subscriber->GetName(), *CallbackFunctionName.ToString());
		return;
	}

	// 델리게이트에 바인딩
	FScriptDelegate Delegate;
	Delegate.BindUFunction(Subscriber, CallbackFunctionName);
	OnSuspicionEvent.Add(Delegate);

	UE_LOG(LogTemp, Log, TEXT("[SuspicionManagerSubsystem] Subscriber %s subscribed to suspicion events via %s"), *Subscriber->GetName(), *CallbackFunctionName.ToString());
}

void USuspicionManagerSubsystem::UnsubscribeFromSuspicionEvents(UObject* Subscriber)
{
	if (!Subscriber)
	{
		return;
	}

	Subscribers.Remove(Subscriber);
	// 델리게이트에서 제거는 자동으로 처리됨 (UObject가 파괴되면)

	UE_LOG(LogTemp, Log, TEXT("[SuspicionManagerSubsystem] Subscriber %s unsubscribed from suspicion events"), *Subscriber->GetName());
}

ESuspiciousBehavior USuspicionManagerSubsystem::GetPlayerSuspiciousBehavior(ABlasterCharacter* Player) const
{
	if (!Player)
	{
		return ESuspiciousBehavior::None;
	}

	if (!PlayerSuspiciousStates.Contains(Player)) 
	{
		return ESuspiciousBehavior::None;
	}		

	const FPlayerSuspicionState& SuspicionState = PlayerSuspiciousStates[Player];

	// 만료 시간 확인
	float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (CurrentTime > SuspicionState.ExpireTime)
	{
		// 만료됨
		return ESuspiciousBehavior::None;
	}

	return SuspicionState.Behavior;
}

void USuspicionManagerSubsystem::SetPlayerSuspiciousBehavior(ABlasterCharacter* Player, ESuspiciousBehavior Behavior, float Duration)
{
	if (!Player || !GetWorld())
	{
		return;
	}

	float CurrentTime = GetWorld()->GetTimeSeconds();
	float ExpiryTime = CurrentTime + Duration;

	PlayerSuspiciousStates.Add(Player, FPlayerSuspicionState{ Behavior, ExpiryTime});

	UE_LOG(LogTemp, VeryVerbose, TEXT("[SuspicionManagerSubsystem] Set player %s suspicious behavior: %s (expires in %.2f seconds)"), 
		*Player->GetName(), *UEnum::GetValueAsString(Behavior), Duration);
}

void USuspicionManagerSubsystem::UpdateSuspiciousStates()
{
	if (!GetWorld())
	{
		return;
	}

	float CurrentTime = GetWorld()->GetTimeSeconds();

	// 만료된 상태 제거
	TArray<ABlasterCharacter*> ExpiredPlayers;
	for (auto& State : PlayerSuspiciousStates)
	{
		if (State.Key && CurrentTime > State.Value.ExpireTime)
		{
			ExpiredPlayers.Add(State.Key);
		}
	}

	for (ABlasterCharacter* ExpiredPlayer : ExpiredPlayers)
	{
		PlayerSuspiciousStates.Remove(ExpiredPlayer);
		UE_LOG(LogTemp, VeryVerbose, TEXT("[SuspicionManagerSubsystem] Player %s suspicious behavior expired"), 
			ExpiredPlayer ? *ExpiredPlayer->GetName() : TEXT("Invalid"));
	}

	// 유효하지 않은 플레이어 제거
	TArray<ABlasterCharacter*> InvalidPlayers;
	for (auto& Pair : PlayerSuspiciousStates)
	{
		if (!IsValid(Pair.Key))
		{
			InvalidPlayers.Add(Pair.Key);
		}
	}

	for (ABlasterCharacter* InvalidPlayer : InvalidPlayers)
	{
		PlayerSuspiciousStates.Remove(InvalidPlayer);
		LastEventTimeByPlayer.Remove(InvalidPlayer);
	}
}
