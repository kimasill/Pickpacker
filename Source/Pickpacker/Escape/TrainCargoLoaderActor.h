#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractableInterface.h"
#include "Interaction/InteractionUIData.h"
#include "TrainCargoLoaderActor.generated.h"

class UBoxComponent;
class USceneComponent;

UCLASS()
class PICKPACKER_API ATrainCargoLoaderActor : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	ATrainCargoLoaderActor();

	virtual void OnInteract_Implementation(ACharacter* Interactor) override;
	virtual bool CanInteract_Implementation(ACharacter* Interactor) override;
	virtual FText GetInteractText_Implementation() override;
	virtual bool RequestShowInteractionUI_Implementation(ACharacter* Interactor) override;
	virtual void GetInteractionUIData_Implementation(FInteractionUIData& OutData) override;

protected:
	class AParcelActor* GetCarriedParcel(ACharacter* Interactor) const;
	class UTrainTravelComponent* GetTrainTravelComponent() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionVolume;
};
