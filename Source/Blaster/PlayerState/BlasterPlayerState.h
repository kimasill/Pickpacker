// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Blaster/BlasterTypes/Team.h"
#include "BlasterPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class BLASTER_API ABlasterPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	* Replication notification for Score variable
	*/
	virtual void OnRep_Score() override;

	UFUNCTION()
	virtual void OnRep_Defeats();
	void AddToScore(float ScoreAmount);
	void AddToDefeats(int32 DefeatAmount);
private:
	UPROPERTY()
	class ABlasterCharacter* Character;
	UPROPERTY()
	class ABlasterPlayerController* Controller;

	UPROPERTY(ReplicatedUsing = OnRep_Score)
	int32 Defeats;

	UPROPERTY(ReplicatedUsing = OnRep_Team)
	ETeam Team = ETeam::ET_NoTeam;

	/** 개인별 의심 수치 */
	UPROPERTY(ReplicatedUsing = OnRep_PersonalSuspicion, VisibleAnywhere, Category = "Suspicion")
	float PersonalSuspicion = 0.0f;

	/** 목숨 카운트 (최대 3개) */
	UPROPERTY(ReplicatedUsing = OnRep_Lives, VisibleAnywhere, Category = "Lives")
	int32 Lives = 3;

	UFUNCTION()
	void OnRep_Team();

	UFUNCTION()
	void OnRep_PersonalSuspicion(float OldSuspicion);

	UFUNCTION()
	void OnRep_Lives(int32 OldLives);

public:
	FORCEINLINE ETeam GetTeam() const { return Team; }
	void SetTeam(ETeam TeamToSet);

	/** 개인별 의심 수치 추가 */
	UFUNCTION(BlueprintCallable, Category = "Suspicion")
	void AddPersonalSuspicion(float Points);

	/** 개인별 의심 수치 가져오기 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Suspicion")
	float GetPersonalSuspicion() const { return PersonalSuspicion; }

	/** 목숨 카운트 가져오기 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lives")
	int32 GetLives() const { return Lives; }

	/** 목숨 감소 */
	UFUNCTION(BlueprintCallable, Category = "Lives")
	void LoseLife();

	/** 목숨이 모두 소진되었는지 확인 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lives")
	bool IsOutOfLives() const { return Lives <= 0; }

	/** 의심 수치 100 이상인지 확인 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Suspicion")
	bool IsSuspicionMaxed() const { return PersonalSuspicion >= 100.0f; }

	/** 이벤트: 의심 수치 변경 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPersonalSuspicionChanged, float, NewSuspicion, float, OldSuspicion);
	UPROPERTY(BlueprintAssignable, Category = "Suspicion|Events")
	FOnPersonalSuspicionChanged OnPersonalSuspicionChanged;

	/** 이벤트: 목숨 변경 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLivesChanged, int32, NewLives, int32, OldLives);
	UPROPERTY(BlueprintAssignable, Category = "Lives|Events")
	FOnLivesChanged OnLivesChanged;
};
