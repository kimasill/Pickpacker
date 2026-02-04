// Copyright

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "LadderActor.generated.h"

class UBoxComponent;
class USceneComponent;

UCLASS()
class BLASTER_API ALadderActor : public AActor
{
	GENERATED_BODY()

public:
	ALadderActor();

protected:
	virtual void BeginPlay() override;

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

	UFUNCTION(BlueprintCallable, Category = "Ladder")
	float GetLadderLength() const;

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
