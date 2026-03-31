#include "InstabilityFactorComponent.h"

UInstabilityFactorComponent::UInstabilityFactorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}


