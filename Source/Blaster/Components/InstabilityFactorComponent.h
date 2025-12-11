#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InstabilityFactorComponent.generated.h"

/**
 * Instability factor provider - attach to parcels or special items to add instability
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BLASTER_API UInstabilityFactorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInstabilityFactorComponent();

	/** Instability factor contributed by this component */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instability")
	float InstabilityFactor = 0.0f;
};


