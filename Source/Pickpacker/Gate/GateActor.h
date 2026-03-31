// Base gate actor requiring item use actions to unlock devices/doors
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Blaster/Gate/GateTypes.h"
#include "Blaster/Interfaces/InteractableInterface.h"
#include "Blaster/Interaction/InteractionUIData.h"
#include "GateActor.generated.h"

class ACharacter;
class AParcelActor;
class UPlayerInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGateUnlocked, AActor*, GateActor);

UCLASS(Blueprintable)
class BLASTER_API AGateActor : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AGateActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IInteractableInterface
	virtual void OnInteract_Implementation(ACharacter* Interactor) override;
	virtual bool CanInteract_Implementation(ACharacter* Interactor) override;
	virtual FText GetInteractText_Implementation() override;
	virtual bool RequestShowInteractionUI_Implementation(ACharacter* Interactor) override;
	virtual void GetInteractionUIData_Implementation(FInteractionUIData& OutData) override;
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnGateUnlocked(ACharacter* InstigatorCharacter);
	virtual void Multicast_OnGateUnlocked_Implementation(ACharacter* InstigatorCharacter);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnGateIneracted(ACharacter* Interactor);
	virtual void Multicast_OnGateIneracted_Implementation(ACharacter* Interactor);

	/** 아이템을 사용하여 게이트 해제 시도 (서버) */
	UFUNCTION(BlueprintCallable, Category = "Gate")
	bool TryUseItemWithGate(AParcelActor* Item, ACharacter* User);

	/** 주어진 플레이어가 조건을 만족하는지 노출 (클라 UI용) */
	UFUNCTION(BlueprintCallable, Category = "Gate")
	bool DoesPlayerMeetConditions(ACharacter* User) const;

	/** 요구 조건 조회 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Gate")
	const TArray<FGateCondition>& GetRequiredConditions() const { return RequiredConditions; }

	/** 게이트가 해제되었을 때 브로드캐스트 */
	UPROPERTY(BlueprintAssignable, Category = "Gate|Events")
	FOnGateUnlocked OnGateUnlocked;

protected:
	/** 조건 검사 (서버 우선) */
	bool CheckGateConditions(ACharacter* User) const;

	/** 특정 플레이어가 조건 세트를 만족하는지 */
	bool PlayerSatisfiesConditions(ACharacter* User, const FGateCondition& Condition) const;

	/** UseAction/ItemId 확인용 인벤토리 조회 */
	UPlayerInventoryComponent* FindInventory(const ACharacter* User) const;

	/** 게이트 해제 실행 (서버) */
	void UnlockGate(ACharacter* InstigatorCharacter);

	UFUNCTION()
	void OnRep_IsUnlocked();

protected:
	/** 게이트 식별자 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	FName GateId;

	/** 해제에 필요한 조건 (AND) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	TArray<FGateCondition> RequiredConditions;

	/** 해제 시 수행할 결과 정보 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	FGateUnlockResult UnlockResult;

	/** 모든 플레이어가 인증해야 하는지 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	bool bRequireAllPlayersAuthorized = false;

	/** 상호작용 텍스트(잠김/해제) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate|UI")
	FText LockedInteractText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate|UI")
	FText UnlockedInteractText;

	/** 현재 해제 상태 */
	UPROPERTY(ReplicatedUsing = OnRep_IsUnlocked, BlueprintReadOnly, Category = "Gate")
	bool bIsUnlocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate")
	bool bGateInteractable = true;

	/** 로컬 월드 플래그 상태 (간단한 태그 기반) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	TMap<FGameplayTag, bool> WorldFlagState;

	UFUNCTION(BlueprintImplementableEvent, Category = "Gate")
	void BP_OnGateUnlocked(ACharacter* InstigatorCharacter);

	UFUNCTION(BlueprintImplementableEvent, Category = "Gate")
	void BP_OnGateInteracted(ACharacter* Interactor);
};
