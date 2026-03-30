#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ConveyorBeltActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
class AActor;
class ABlasterCharacter;

USTRUCT()
struct FConveyedPrimitiveEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UPrimitiveComponent> Primitive;

	UPROPERTY()
	bool bRestoreGravity = false;
};

/**
 * Simple conveyor belt actor that keeps overlapping physics objects moving in a direction
 * and releases them when they reach the drop volume so gravity can make them fall.
 */
UCLASS()
class BLASTER_API AConveyorBeltActor : public AActor
{
	GENERATED_BODY()

public:
	AConveyorBeltActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* BeltMesh;

	/** Volume that keeps actors moving along the belt */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* ConveyorVolume;

	/** Overlap volume placed at the end of the belt; when actors touch it they are released so they can fall */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* DropVolume;

	/** Units per second applied along BeltDirection */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conveyor", meta = (ClampMin = "0.0"))
	float BeltSpeed = 200.0f;

	/** Normalized direction the belt pushes objects toward */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conveyor")
	FVector BeltDirection = FVector(1.f, 0.f, 0.f);

	/** Disable gravity while objects ride on the belt */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conveyor")
	bool bDisableGravityWhileConveyed = true;

	/** 플레이어 이동 시작 위치 오프셋 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conveyor")
	FVector EntryOffset = FVector::ZeroVector;

	/** Automatically release an actor if it leaves the belt volume for any reason */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conveyor")
	bool bReleaseOnVolumeExit = true;

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Conveyor")
	FVector GetConveyorEntryLocation() const;

	/** 컨베이어에서 제출되는 파슬을 컨베이어 목록에서 해제 (파괴 전 호출) */
	UFUNCTION(BlueprintCallable, Category = "Conveyor")
	void ReleaseParcelIfConveyed(class AParcelActor* Parcel);

protected:
	UFUNCTION()
	void HandleConveyorBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void HandleConveyorEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
	);

	UFUNCTION()
	void HandleDropOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	void AddPrimitiveToConveyor(UPrimitiveComponent* Primitive);
	void ReleasePrimitive(UPrimitiveComponent* Primitive, bool bRestorePhysics);
	int32 FindPrimitiveIndex(UPrimitiveComponent* Primitive) const;
	void UpdateConveyedPrimitives(float DeltaSeconds);

	bool CanConveyActor(AActor* Actor) const;
	void AddActorToConveyor(AActor* Actor);
	void ReleaseActor(AActor* Actor);
	int32 FindActorIndex(AActor* Actor) const;

protected:
	UPROPERTY()
	TArray<FConveyedPrimitiveEntry> ConveyedPrimitives;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> ConveyedActors;
};

