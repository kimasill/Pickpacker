// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_MoveToPatrolPoint.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "AI/DroneActor.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Engine/Engine.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"

UBTTask_MoveToPatrolPoint::UBTTask_MoveToPatrolPoint()
{
	NodeName = TEXT("Move To Patrol Point");
	bCreateNodeInstance = true;
	bNotifyTick = true;
	// Default blackboard key
	PatrolPointLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MoveToPatrolPoint, PatrolPointLocationKey));
}

EBTNodeResult::Type UBTTask_MoveToPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FMoveToPatrolPointMemory* MyMemory = reinterpret_cast<FMoveToPatrolPointMemory*>(NodeMemory);
	if (!MyMemory)
	{
		return EBTNodeResult::Failed;
	}

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	ADroneActor* Drone = Cast<ADroneActor>(Pawn);
	if (!Drone)
	{
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	// Get target location from blackboard
	MyMemory->TargetLocation = BlackboardComp->GetValueAsVector(PatrolPointLocationKey.SelectedKeyName);
	
	if (MyMemory->TargetLocation.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BTTask_MoveToPatrolPoint] Invalid target location"));
		return EBTNodeResult::Failed;
	}

	MyMemory->bIsActive = true;
	
	UE_LOG(LogTemp, Log, TEXT("[BTTask_MoveToPatrolPoint] Started moving to location: %s"), *MyMemory->TargetLocation.ToString());
	
	return EBTNodeResult::InProgress;
}

void UBTTask_MoveToPatrolPoint::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FMoveToPatrolPointMemory* MyMemory = reinterpret_cast<FMoveToPatrolPointMemory*>(NodeMemory);
	if (!MyMemory)
	{
		UE_LOG(LogTemp, Error, TEXT("[BTTask_MoveToPatrolPoint] TickTask: NodeMemory is null!"));
		return;
	}

	if (!MyMemory->bIsActive)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("[BTTask_MoveToPatrolPoint] TickTask: Task is not active"));
		return;
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("[BTTask_MoveToPatrolPoint] TickTask called - DeltaSeconds: %.3f"), DeltaSeconds);

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		MyMemory->bIsActive = false;
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	APawn* Pawn = AIController->GetPawn();
	if (!Pawn)
	{
		MyMemory->bIsActive = false;
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	ADroneActor* Drone = Cast<ADroneActor>(Pawn);
	if (!Drone || !Drone->MovementComponent)
	{
		MyMemory->bIsActive = false;
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	FVector CurrentLocation = Pawn->GetActorLocation();
	FVector ToTarget = MyMemory->TargetLocation - CurrentLocation;
	float Distance = ToTarget.Size();
	FVector Direction = ToTarget.GetSafeNormal();

	// Check if reached
	if (Distance <= AcceptableRadius)
	{
		UE_LOG(LogTemp, Log, TEXT("[BTTask_MoveToPatrolPoint] Reached patrol point. Distance: %.2f"), Distance);
		MyMemory->bIsActive = false;
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	// 고급 회피 시스템: 샘플링 기반 회피
	UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Pawn->GetRootComponent());
	if (RootPrimitive)
	{
		// 충돌 형태 가져오기
		FCollisionShape CollisionShape;
		if (USphereComponent* SphereComp = Cast<USphereComponent>(RootPrimitive))
		{
			CollisionShape = FCollisionShape::MakeSphere(SphereComp->GetScaledSphereRadius());
		}
		else if (UCapsuleComponent* CapsuleComp = Cast<UCapsuleComponent>(RootPrimitive))
		{
			CollisionShape = FCollisionShape::MakeCapsule(CapsuleComp->GetScaledCapsuleRadius(), CapsuleComp->GetScaledCapsuleHalfHeight());
		}
		else
		{
			FVector BoxExtent = RootPrimitive->Bounds.BoxExtent;
			CollisionShape = FCollisionShape::MakeBox(BoxExtent);
		}

		// 전방 충돌 체크
		FHitResult ForwardHit;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(Pawn);
		QueryParams.bTraceComplex = true;

		FVector ForwardTestLocation = CurrentLocation + Direction * AvoidanceScanDistance;
		bool bForwardHit = GetWorld()->SweepSingleByChannel(
			ForwardHit,
			CurrentLocation,
			ForwardTestLocation,
			Pawn->GetActorQuat(),
			RootPrimitive->GetCollisionObjectType(),
			CollisionShape,
			QueryParams
		);

		if (bForwardHit && ForwardHit.bBlockingHit)
		{
			float ClosestObstacleDistance = ForwardHit.Distance;
			FVector ObstacleNormal = ForwardHit.Normal;

			// 가중치 함수: 장애물과 가까울수록 더 큰 상승값
			float DistanceRatio = FMath::Clamp(ClosestObstacleDistance / AvoidanceScanDistance, 0.0f, 1.0f);
			float LiftWeight = 1.0f - FMath::Pow(DistanceRatio, LiftWeightExponent);
			float LiftValue = FMath::Lerp(BaseLiftValue, MaxLiftValue, LiftWeight);

			// 여러 각도로 샘플링
			FVector BestDirection = Direction;
			float BestScore = -1.0f;

			// 현재 방향을 기준으로 샘플링
			FVector ForwardVector = Direction;
			FVector RightVector = FVector::CrossProduct(ForwardVector, FVector::UpVector).GetSafeNormal();
			FVector UpVector = FVector::UpVector;

			for (int32 i = 0; i < AvoidanceSampleCount; ++i)
			{
				// 샘플링 각도 계산 (-범위/2 ~ +범위/2)
				float Angle = FMath::Lerp(-AvoidanceSampleAngleRange * 0.5f, AvoidanceSampleAngleRange * 0.5f, 
					static_cast<float>(i) / static_cast<float>(AvoidanceSampleCount - 1));
				float AngleRad = FMath::DegreesToRadians(Angle);

				// 수평 회전 (Yaw)
				FVector RotatedDirection = ForwardVector.RotateAngleAxis(Angle, UpVector);

				// 수직 각도 샘플링 (상승 각도)
				for (int32 j = 0; j < 3; ++j)
				{
					float VerticalAngle = FMath::Lerp(0.0f, 45.0f, static_cast<float>(j) / 2.0f);
					float VerticalAngleRad = FMath::DegreesToRadians(VerticalAngle);
					
					FVector SampleDirection = RotatedDirection;
					SampleDirection.Z += FMath::Sin(VerticalAngleRad) * LiftValue;
					SampleDirection.Normalize();

					// 이 방향으로 스캔
					FVector SampleEndLocation = CurrentLocation + SampleDirection * AvoidanceScanDistance;
					FHitResult SampleHit;
					bool bSampleHit = GetWorld()->SweepSingleByChannel(
						SampleHit,
						CurrentLocation,
						SampleEndLocation,
						Pawn->GetActorQuat(),
						RootPrimitive->GetCollisionObjectType(),
						CollisionShape,
						QueryParams
					);

					// 점수 계산: 목표 방향과의 유사도 + 안전 거리
					float Score = 0.0f;
					if (!bSampleHit || SampleHit.Distance > AvoidanceMinSafeDistance)
					{
						// 목표 방향과의 유사도 (Dot Product)
						float DirectionSimilarity = FVector::DotProduct(SampleDirection, Direction);
						
						// 안전 거리 (충돌이 없으면 최대 거리 사용)
						float SafeDistance = bSampleHit ? SampleHit.Distance : AvoidanceScanDistance;
						
						// 점수 = 방향 유사도 * 안전 거리 비율
						Score = DirectionSimilarity * (SafeDistance / AvoidanceScanDistance);
						
						// 상승 방향에 보너스 (계단/틈 통과)
						if (SampleDirection.Z > 0.0f)
						{
							Score += 0.2f * (SampleDirection.Z * LiftValue);
						}
					}

					// 최고 점수 방향 선택
					if (Score > BestScore)
					{
						BestScore = Score;
						BestDirection = SampleDirection;
					}
				}
			}

			// 최적 방향이 찾아졌으면 사용
			if (BestScore > 0.0f)
			{
				Direction = BestDirection;
				UE_LOG(LogTemp, VeryVerbose, TEXT("[BTTask_MoveToPatrolPoint] Avoidance: Found safe direction with score %.2f"), BestScore);
			}
			else
			{
				// 안전한 방향을 찾지 못한 경우: 반사 + 상승
				FVector ReflectedDirection = Direction - 2.0f * FVector::DotProduct(Direction, ObstacleNormal) * ObstacleNormal;
				ReflectedDirection.Z = FMath::Max(ReflectedDirection.Z, LiftValue);
				ReflectedDirection.Normalize();
				Direction = ReflectedDirection;
				UE_LOG(LogTemp, Warning, TEXT("[BTTask_MoveToPatrolPoint] Avoidance: No safe direction found, using reflection"));
			}
		}
	}

	// PID/Steering 기반 smoothing: 급격한 방향 변경 방지
	FVector DesiredDirection = Direction;
	
	// 이전 방향이 없으면 초기화
	if (MyMemory->PreviousDirection.IsNearlyZero())
	{
		MyMemory->PreviousDirection = DesiredDirection;
		MyMemory->SmoothedDirection = DesiredDirection;
	}

	// 방향 변경량 계산 (각도 기반)
	float DirectionAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
		FVector::DotProduct(MyMemory->SmoothedDirection, DesiredDirection), -1.0f, 1.0f)));
	
	// 최대 방향 변경 속도 제한 (각도/초)
	float MaxAngleChange = MaxDirectionChangeRate * DeltaSeconds;
	if (DirectionAngle > MaxAngleChange)
	{
		// 최대 변경량으로 제한된 방향 계산
		FVector RotationAxis = FVector::CrossProduct(MyMemory->SmoothedDirection, DesiredDirection).GetSafeNormal();
		if (RotationAxis.IsNearlyZero())
		{
			// 거의 같은 방향이면 수직 벡터 사용
			RotationAxis = FVector::CrossProduct(MyMemory->SmoothedDirection, FVector::UpVector).GetSafeNormal();
			if (RotationAxis.IsNearlyZero())
			{
				RotationAxis = FVector::CrossProduct(MyMemory->SmoothedDirection, FVector::RightVector).GetSafeNormal();
			}
		}
		
		FQuat RotationQuat = FQuat(RotationAxis, FMath::DegreesToRadians(MaxAngleChange));
		MyMemory->SmoothedDirection = RotationQuat.RotateVector(MyMemory->SmoothedDirection);
		MyMemory->SmoothedDirection.Normalize();
	}
	else
	{
		// 제한 없이 원하는 방향으로 이동 가능
		MyMemory->SmoothedDirection = DesiredDirection;
	}

	// Interpolation을 통한 추가 smoothing
	MyMemory->SmoothedDirection = FMath::VInterpTo(
		MyMemory->SmoothedDirection,
		DesiredDirection,
		DeltaSeconds,
		SteeringSmoothingRate
	);
	MyMemory->SmoothedDirection.Normalize();

	// 이전 방향 업데이트
	MyMemory->PreviousDirection = MyMemory->SmoothedDirection;

	// Smoothing된 방향 사용
	FVector FinalDirection = MyMemory->SmoothedDirection;
	FVector MovementInput = FinalDirection * SpeedMultiplier;
	
	// Directly use FloatingPawnMovement's AddInputVector (more reliable)
	Drone->MovementComponent->AddInputVector(MovementInput);
	
	// Also try AddMovementInput as fallback
	Pawn->AddMovementInput(MovementInput, 1.0f);
	
	UE_LOG(LogTemp, VeryVerbose, TEXT("[BTTask_MoveToPatrolPoint] TickTask - Distance: %.2f, Direction: %s, Smoothed: %s, Input: %s"), 
		Distance, *Direction.ToString(), *FinalDirection.ToString(), *MovementInput.ToString());

	// Rotate towards target - FindLookAtRotation을 사용하여 정확한 회전 계산
	FRotator TargetRotation = UKismetMathLibrary::FindLookAtRotation(CurrentLocation, MyMemory->TargetLocation);
	FRotator CurrentRotation = Pawn->GetActorRotation();
	
	
	// Yaw만 회전 (Pitch와 Roll은 유지)
	FRotator TargetYawOnly = FRotator(CurrentRotation.Pitch, TargetRotation.Yaw, CurrentRotation.Roll);
	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetYawOnly, DeltaSeconds, 5.0f);
	
	Pawn->SetActorRotation(NewRotation);
}

EBTNodeResult::Type UBTTask_MoveToPatrolPoint::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FMoveToPatrolPointMemory* MyMemory = reinterpret_cast<FMoveToPatrolPointMemory*>(NodeMemory);
	if (MyMemory)
	{
		MyMemory->bIsActive = false;
	}
	return EBTNodeResult::Aborted;
}

