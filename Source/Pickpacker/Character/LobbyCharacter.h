// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BlasterCharacter.h"
#include "LobbyCharacter.generated.h"

/**
 * 로비 전용 캐릭터 클래스
 * - 카메라 없음 (3인칭으로 표시)
 * - 입력 비활성화
 * - OverHeadWidget으로 이름과 준비 상태 표시
 */
UCLASS()
class PICKPACKER_API ALobbyCharacter : public ABlasterCharacter
{
	GENERATED_BODY()

public:
	ALobbyCharacter();

protected:
	virtual void BeginPlay() override;
};

