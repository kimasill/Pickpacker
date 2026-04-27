#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractableInterface.h"
#include "Interaction/InteractionUIData.h"
#include "ReturnTrainPlatformActor.generated.h"

class UBoxComponent;
class USceneComponent;
class UTrainTravelComponent;

UCLASS()
class PICKPACKER_API AReturnTrainPlatformActor : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AReturnTrainPlatformActor();

	virtual void BeginPlay() override;

	virtual void OnInteract_Implementation(ACharacter* Interactor) override;
	virtual bool CanInteract_Implementation(ACharacter* Interactor) override;
	virtual FText GetInteractText_Implementation() override;
	virtual bool RequestShowInteractionUI_Implementation(ACharacter* Interactor) override;
	virtual void GetInteractionUIData_Implementation(FInteractionUIData& OutData) override;

protected:
	UFUNCTION()
	void OnBoardingVolumeBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnBoardingVolumeEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	bool IsCharacterBoarded(const ACharacter* Character) const;
	bool CanOperateDeparture(const ACharacter* Character) const;
	UTrainTravelComponent* GetTrainTravelComponent() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> BoardingVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionVolume;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Return Train")
	bool bAutoStartReturnBoardingOnEntry = true;

private:
	UPROPERTY()
	TSet<TWeakObjectPtr<AController>> BoardedControllers;
};
