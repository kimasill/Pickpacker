// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BlasterPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class BLASTER_API ABlasterPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	void SetHUDHealth(float Health, float MaxHealth);
	void SetHUDShield(float Shield, float MaxShield);
	void SetHUDScore(float Score);
	void SetHUDDefeats(int32 Defeats);
	void SetHUDWeaponAmmo(int32 ammo);
	void SetHUDCarriedAmmo(int32 ammo);
	void SetHUDMatchCountdown(float CountdownTime);
	void SetHUDAnnouncementCountdown(float CountdownTime);
	void SetHUDGrenades(int32 Grenades);
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual float GetServerTime();
	virtual void ReceivedPlayer() override; // Syncs with server clock as soon as possible
	void OnMatchStateSet(FName State);
	void HandleMatchHasStarted();
	void HandleCooldown();
protected:
	virtual void BeginPlay() override;
	void SetHUDTime();
	void PollInit(); // Polls for the BlasterHUD and CharacterOverlay widgets

	/**
	* Sync time between server and client
	*/

	// Requests the current server time, passing in the client's request time
	UFUNCTION(Server, Reliable)
	void ServerRequestServerTime(float TimeOfClientRequest);

	// Reports the current server time to the client in response to the request
	UFUNCTION(Client, Reliable)
	void ClientReportServerTime(float TimeOfClientRequest, float TimeServerRecievedClientRequest);

	float ClientServerDelta = 0.f; // Difference between client and server time

	UPROPERTY(EditAnywhere, Category = "Time")
	float TimeSyncFrequency = 5.f; // How often to sync time with the server

	float TimeSyncRunningTime = 0.f; // How long the time sync has been running
	void CheckTimeSync(float DeltaTime);

	UFUNCTION(Server, Reliable)
	void ServerCheckMatchState(); // Server checks match state

	UFUNCTION(Client, Reliable)
	void ClientJoinMidGame(FName StateOfMatch, float Warmup, float Match, float Cooldown, float StartingTime); // Client joins mid-game and initializes HUD

	void HighPingWarning();
	void StopHighPingWarning();
	void CheckPing(float DeltaTime);
private:
	UPROPERTY()
	class ABlasterHUD* BlasterHUD;

	UPROPERTY()
	class ABlasterGameMode* BlasterGameMode;

	float LevelStartingTime = 0.f;
	float MatchTime = 0.f;
	float WarmupTime = 0.f;
	float CooldownTime = 0.f; // Time for match countdown
	uint32 CountdownInt = 0;

	UPROPERTY(ReplicatedUsing = "OnRep_MatchState")
	FName MatchState;

	UFUNCTION()
	void OnRep_MatchState(); // Called when MatchState changes on the client

	UPROPERTY()
	class UCharacterOverlay* CharacterOverlay;	

	float HUDHealth = 0.f;
	bool bInitializeHealth = false;
	float HUDMaxHealth = 0.f;
	float HUDScore = 0.f;
	bool bInitializeScore = false;
	int32 HUDDefeats = 0;
	bool bInitializeDefeats = false;
	int32 HUDGrenades = 0;
	bool bInitializeGrenades = false;
	float HUDShield = 0.f;
	bool bInitializeShield = false;
	float HUDMaxShield = 0.f;
	float HUDCarriedAmmo = 0.f;
	bool bInitializeCarriedAmmo = false;
	float HUDWeaponAmmo = 0.f;
	bool bInitializeWeaponAmmo = false;

	float HighPingRunningTime = 0.f; // Time for high ping warning

	UPROPERTY(EditAnywhere)
	float HighPingDuration = 5.f; // Duration of high ping warning

	float PingAnimationRunningTime = 0.f; // Time for ping animation

	UPROPERTY(EditAnywhere)
	float CheckPingFrequency = 20.f; // How often to check ping

	UPROPERTY(EditAnywhere)
	float HighPingThreshold = 50.f; // Threshold for high ping warning
};
