// Fill out your copyright notice in the Description page of Project Settings.


#include "PickpackerSuspicionLibrary.h"
#include "Character/BlasterCharacter.h"
#include "Components/PlayerInventoryComponent.h"
#include "Components/InteractionComponent.h"
#include "Parcel/ParcelActor.h"
#include "PickpackerTypes/PickpackerTypes.h"


ESuspiciousBehavior UPickpackerSuspicionLibrary::CheckSuspiciousBehavior(ACharacter* Player)
{
    if (!Player)
    {
        return ESuspiciousBehavior::None;
    }

    ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Player);
    if (!BlasterCharacter)
    {
        return ESuspiciousBehavior::None;
    }

    // Check if player is disassembling parcel (getting item from parcel)
    if (UInteractionComponent* InteractionComponent = BlasterCharacter->GetInteractionComponent())
    {
        if (AParcelActor* CarriedParcel = InteractionComponent->GetCarriedParcel())
        {
            // Check if parcel is being disassembled (포장된 택배를 해체)
            if (CarriedParcel->IsPackaged() && CarriedParcel->IsItem())
            {
                return ESuspiciousBehavior::DisassemblingParcel;
            }

            // Check if picking packaged parcel from shelf
            if (CarriedParcel->IsPackaged())
            {
                return ESuspiciousBehavior::PickingPackagedParcel;
            }
        }
    }

    // Check if player has suspicious items in inventory
    if (UPlayerInventoryComponent* InventoryComponent = BlasterCharacter->GetPlayerInventoryComponent())
    {
        if (InventoryComponent->GetInventoryCount() > 0)
        {
            // Check for specific suspicious item types
            // This would need to be expanded based on your item system
            return ESuspiciousBehavior::HoldingSuspiciousItem;
        }
    }

    // Check if player is holding weapon
    // Note: Access CombatComponent through public member if available
    // This may need to be adjusted based on your BlasterCharacter implementation
    // For now, we'll check through a different method or skip if not accessible

    // TODO: Check for restricted zone entry
    // TODO: Check for restricted system interaction

    return ESuspiciousBehavior::None;
}