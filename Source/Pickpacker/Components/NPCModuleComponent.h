// NPCModuleComponent - Base class for pluggable modules used by ModularNPCActor.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PickpackerTypes/CoreLoopTypes.h"
#include "NPCModuleComponent.generated.h"

class AModularNPCActor;

/**
 * Base class for modular NPC behaviour pieces.
 * ModularNPCActor notifies every attached module about lifecycle/state changes,
 * allowing enemy/NPC variants to be composed from components instead of actor subclasses.
 */
UCLASS(Abstract, Blueprintable, ClassGroup = (NPC), meta = (BlueprintSpawnableComponent))
class PICKPACKER_API UNPCModuleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC|Module")
	AModularNPCActor* GetModularNPCOwner() const;

	UFUNCTION(BlueprintNativeEvent, Category = "NPC|Module")
	void HandleOwnerReady(AModularNPCActor* OwnerNPC);
	virtual void HandleOwnerReady_Implementation(AModularNPCActor* OwnerNPC) {}

	UFUNCTION(BlueprintNativeEvent, Category = "NPC|Module")
	void HandleDispositionChanged(ENPCDisposition OldDisposition, ENPCDisposition NewDisposition);
	virtual void HandleDispositionChanged_Implementation(ENPCDisposition OldDisposition, ENPCDisposition NewDisposition) {}

	UFUNCTION(BlueprintNativeEvent, Category = "NPC|Module")
	void HandleRoleChanged(ENPCRole OldRole, ENPCRole NewRole);
	virtual void HandleRoleChanged_Implementation(ENPCRole OldRole, ENPCRole NewRole) {}

	UFUNCTION(BlueprintNativeEvent, Category = "NPC|Module")
	void HandleNarrativeStateEvaluated();
	virtual void HandleNarrativeStateEvaluated_Implementation() {}

	UFUNCTION(BlueprintNativeEvent, Category = "NPC|Module")
	void HandleModulesUpdated();
	virtual void HandleModulesUpdated_Implementation() {}
};
