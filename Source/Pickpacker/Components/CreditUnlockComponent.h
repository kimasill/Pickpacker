#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CreditUnlockComponent.generated.h"

class APickpackerGameMode;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCreditUnlockEvent);

/**
 * Simple helper component that unlocks an actor by spending team credits.
 * Blueprint actors can attach this component and forward interaction events to RequestUnlock().
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PICKPACKER_API UCreditUnlockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCreditUnlockComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Returns whether the actor is already unlocked */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Credits")
	bool IsUnlocked() const { return bUnlocked; }

	/** Attempts to unlock the actor by paying the cost */
	UFUNCTION(BlueprintCallable, Category = "Credits")
	bool RequestUnlock(AActor* RequestingActor);

	/** Cost in credits to unlock this actor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits")
	int32 UnlockCost = 10;

	/** Optional reason string used in credit logs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits")
	FString UnlockReason;

	/** Optional message displayed while locked */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits")
	FText LockedMessage;

	/** Optional message displayed once unlocked */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits")
	FText UnlockedMessage;

	/** Event raised when the actor becomes unlocked */
	UPROPERTY(BlueprintAssignable, Category = "Credits")
	FCreditUnlockEvent OnUnlocked;

protected:
	UFUNCTION()
	void OnRep_Unlocked();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Unlocked, BlueprintReadOnly, Category = "Credits")
	bool bUnlocked = false;
};


