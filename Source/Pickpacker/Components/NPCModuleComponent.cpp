#include "Components/NPCModuleComponent.h"

#include "NPC/ModularNPCActor.h"

AModularNPCActor* UNPCModuleComponent::GetModularNPCOwner() const
{
	return Cast<AModularNPCActor>(GetOwner());
}
