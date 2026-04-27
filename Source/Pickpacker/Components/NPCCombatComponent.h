// NPCCombatComponent - Modular combat / AI module for NPC actors

#pragma once

#include "CoreMinimal.h"
#include "Components/NPCModuleComponent.h"
#include "NPCCombatComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNPCHealthChanged, float, NewHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNPCDied);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCTargetAcquired, AActor*, Target);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNPCTargetLost);

/**
 * Modular combat & AI component for NPC actors.
 * Provides health, damage, sight detection, attack logic, and pathfinding hooks.
 * Only active when the owning NPC is in Hostile disposition.
 */
UCLASS(ClassGroup = (NPC), meta = (BlueprintSpawnableComponent))
class PICKPACKER_API UNPCCombatComponent : public UNPCModuleComponent
{
	GENERATED_BODY()

public:
	UNPCCombatComponent();

	// --- Configuration ---------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Health")
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Health")
	float CurrentHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Detection")
	float SightRange = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Detection")
	float SightAngle = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack")
	float AttackRange = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack")
	float AttackDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack")
	float AttackCooldown = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Movement")
	float ChaseSpeed = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Movement")
	float PatrolSpeed = 200.0f;

	// --- Runtime State ---------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bCombatActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TWeakObjectPtr<AActor> CurrentTarget;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsDead = false;

	// --- API -------------------------------------------------------------

	/** Activate or deactivate combat mode */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetCombatActive(bool bActive);

	/** Apply damage to this NPC */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ApplyDamage(float Damage, AActor* DamageCauser = nullptr);

	/** Check if a target actor is within sight */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
	bool IsActorInSight(AActor* Target) const;

	/** Check if a target is within attack range */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
	bool IsActorInAttackRange(AActor* Target) const;

	/** Attempt to attack the current target */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool TryAttack();

	/** Manually set the chase target */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetTarget(AActor* NewTarget);

	/** Clear the chase target */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ClearTarget();

	/** Reset health to max */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ResetHealth();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
	float GetHealthPercent() const { return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f; }

	// --- Events ----------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnNPCHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnNPCDied OnDied;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnNPCTargetAcquired OnTargetAcquired;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnNPCTargetLost OnTargetLost;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float LastAttackTime = 0.0f;

	/** Scan for player targets when combat is active */
	void ScanForTargets();

	/** Fallback chase/attack loop when no behavior tree is driving the owner */
	void UpdateDirectCombatMovement();
};
