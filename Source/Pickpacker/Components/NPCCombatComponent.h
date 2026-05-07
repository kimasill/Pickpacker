// NPCCombatComponent - Modular combat / AI module for NPC actors

#pragma once

#include "CoreMinimal.h"
#include "Components/NPCModuleComponent.h"
#include "PickpackerTypes/CoreLoopTypes.h"
#include "NPCCombatComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNPCHealthChanged, float, NewHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNPCDied);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCTargetAcquired, AActor*, Target);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNPCTargetLost);

/**
 * Modular combat state and attack capability for NPC actors.
 * Behavior Trees decide when targets are promoted, chased, and attacked.
 */
UCLASS(ClassGroup = (NPC), meta = (BlueprintSpawnableComponent))
class PICKPACKER_API UNPCCombatComponent : public UNPCModuleComponent
{
	GENERATED_BODY()

public:
	UNPCCombatComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- Configuration ---------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Health")
	float MaxHealth = 100.0f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Combat|Health")
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

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Combat|Policy")
	ENPCEngagementPolicy EngagementPolicy = ENPCEngagementPolicy::Defensive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Policy")
	bool bRetaliateWhenDamaged = true;

	/** Defensive NPCs can temporarily become aggressive after being damaged, then restore when the target is cleared. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Policy")
	bool bBecomeAggressiveWhenDamaged = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Aggro")
	bool bAutoClearTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Aggro", meta = (ClampMin = "0.0", EditCondition = "bAutoClearTarget"))
	float LoseSightAggroGraceTime = 3.0f;

	/** Clear target when the NPC has chased this far from its initial/home location. Set <= 0 to disable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Aggro", meta = (ClampMin = "0.0", EditCondition = "bAutoClearTarget"))
	float MaxChaseDistanceFromHome = 2500.0f;

	/** Clear target when the target is this far from the NPC. Set <= 0 to disable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Aggro", meta = (ClampMin = "0.0", EditCondition = "bAutoClearTarget"))
	float MaxTargetDistance = 3000.0f;

	/** Legacy escape hatch for NPCs without a Behavior Tree. Keep false for modular BT-driven NPCs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Fallback", meta = (AdvancedDisplay))
	bool bEnableDirectCombatFallback = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack")
	bool bUseActionRuleForDefaultAttack = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack", meta = (EditCondition = "bUseActionRuleForDefaultAttack"))
	FName DefaultAttackActionId = TEXT("Combat.Attack");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Movement")
	float ChaseSpeed = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Movement")
	float PatrolSpeed = 200.0f;

	// --- Runtime State ---------------------------------------------------

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Combat")
	bool bCombatActive = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<AActor> CurrentTarget = nullptr;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Combat")
	bool bIsDead = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Combat|Policy")
	bool bTemporaryAggressionActive = false;

	// --- API -------------------------------------------------------------

	/** Activate or deactivate combat mode */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetCombatActive(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetEngagementPolicy(ENPCEngagementPolicy NewPolicy);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ApplyCombatSettings(const FNPCCombatSettings& Settings);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
	bool CanInitiateEngagement() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
	bool CanRetaliate() const;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void BeginTemporaryAggression(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void RestoreTemporaryEngagementPolicy();

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
	ENPCEngagementPolicy PreviousEngagementPolicy = ENPCEngagementPolicy::Defensive;
	FVector AggroHomeLocation = FVector::ZeroVector;
	float LastTargetVisibleTime = 0.0f;

	/** Scan for player targets when combat is active */
	void ScanForTargets();

	void UpdateTargetRetention();

	/** Fallback chase/attack loop when no behavior tree is driving the owner */
	void UpdateDirectCombatMovement();

	bool IsBehaviorTreeRunning() const;
};
