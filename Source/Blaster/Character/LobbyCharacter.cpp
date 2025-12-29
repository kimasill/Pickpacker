// Fill out your copyright notice in the Description page of Project Settings.

#include "LobbyCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraActor.h"

ALobbyCharacter::ALobbyCharacter()
	: Super()
{
	// 카메라는 부모 생성자에서 생성되지만, BeginPlay에서 제거할 예정
	// 여기서는 입력과 이동만 비활성화

	// 입력 비활성화 (로비에서는 이동 불필요)
	bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->MaxWalkSpeed = 0.f; // 이동 비활성화
	}

	// OverHeadWidget은 유지 (이름과 준비 상태 표시)
	// 부모 클래스에서 이미 생성됨
}

void ALobbyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 카메라가 있다면 비활성화 (제거하지 않고 비활성화만)
	if (UCameraComponent* Camera = GetFollowCamera())
	{
		Camera->SetActive(false);
		Camera->SetVisibility(false);
	}

	// 입력 비활성화
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetIgnoreLookInput(true);
		PC->SetIgnoreMoveInput(true);
		
		// 로비 맵에 고정 카메라가 있다면 그것으로 전환
		// 없으면 기본 3인칭 카메라 사용
		UWorld* World = GetWorld();
		if (World)
		{
			// "LobbyCamera" 태그를 가진 Camera Actor 찾기
			TArray<AActor*> FoundCameras;
			UGameplayStatics::GetAllActorsOfClass(World, ACameraActor::StaticClass(), FoundCameras);
			
			for (AActor* Actor : FoundCameras)
			{
				if (ACameraActor* CameraActor = Cast<ACameraActor>(Actor))
				{
					// 태그로 확인하거나, 이름으로 확인
					if (CameraActor->ActorHasTag(FName("LobbyCamera")) || 
						CameraActor->GetName().Contains(TEXT("Lobby")))
					{
						PC->SetViewTarget(CameraActor);
						break;
					}
				}
			}
		}
	}

	// OverHeadWidget 업데이트
	UpdateOverheadWidget();
}

