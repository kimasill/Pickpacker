// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 공통 Blackboard 키 이름 (문자열 오타 방지).
 * 에디터 BT 에셋은 여전히 동일 문자열을 사용해야 함.
 */
namespace PPBlackboardKeys
{
	inline const FName BatteryLevel(TEXT("BatteryLevel"));
	inline const FName BatteryLow(TEXT("BatteryLow"));
	inline const FName IsCharging(TEXT("IsCharging"));
	inline const FName ShouldUseWeapon(TEXT("ShouldUseWeapon"));
	inline const FName DetectedPlayer(TEXT("DetectedPlayer"));
	inline const FName CurrentPatrolPoint(TEXT("CurrentPatrolPoint"));
	inline const FName PatrolPointLocation(TEXT("PatrolPointLocation"));
	inline const FName ChargingStation(TEXT("ChargingStation"));
}
