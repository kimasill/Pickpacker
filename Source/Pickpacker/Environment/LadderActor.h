// Copyright

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "LadderActor.generated.h"

class UBoxComponent;
class USceneComponent;
class USplineComponent;

UCLASS()
class BLASTER_API ALadderActor : public AActor
{
	GENERATED_BODY()

public:
	ALadderActor();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnLadderTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnLadderTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION(BlueprintCallable, Category = "Ladder")
	bool TryStartClimb(ACharacter* Interactor);

	UFUNCTION(BlueprintCallable, Category = "Ladder")
	void ForceStopClimb(ACharacter* Interactor, bool bPlaceAtTop);

public:
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	FVector GetBottomLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Ladder")
	FVector GetTopLocation() const;

	UFUNCTION(BlueprintCallable, Category = "Ladder")
	FVector GetLadderUpVector() const;

	/** 사다리가 바라보는 방향 (벽 쪽, Yaw 계산용) */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	FVector GetLadderForwardVector() const;

	UFUNCTION(BlueprintCallable, Category = "Ladder")
	float GetLadderLength() const;

	/** 스플라인 거리 기준으로 월드 위치에 가장 가까운 거리 반환 */
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	float FindDistanceAlongSplineForWorldLocation(const FVector& WorldLocation) const;
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	FVector GetLocationAtDistanceAlongSpline(float Distance) const;
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	FVector GetDirectionAtDistanceAlongSpline(float Distance) const;
	UFUNCTION(BlueprintCallable, Category = "Ladder")
	float GetSplineLength() const;

	/** 스플라인 사용 가능 여부 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ladder")
	bool HasValidSpline() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	USplineComponent* LadderSpline;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	UBoxComponent* TriggerVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	USceneComponent* BottomPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder")
	USceneComponent* TopPoint;

	// Set in BP based on trigger/angle checks
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder")
	bool bCanAccess = false;

	// If true, ladder mesh/components won't block pawn movement
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ladder")
	bool bIgnorePawnCollision = true;
};
