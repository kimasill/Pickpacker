// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

class ULevelSequence;

#include "BlasterPlayerController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHighPingDelegate, bool, bHighPing);
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
	void UpdateInventoryWidget(const TArray<class AParcelActor*>& Items);
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void HideTeamScores(); // Hides team scores in the HUD
	void InitTeamScores(); // Initializes team scores in the HUD
	void SetHUDRedTeamScore(int32 Score); // Sets the red team score in the HUD
	void SetHUDBlueTeamScore(int32 Score); // Sets the blue team score in the HUD



	virtual float GetServerTime();
	virtual void ReceivedPlayer() override; // Syncs with server clock as soon as possible
	void OnMatchStateSet(FName State, bool bTeamsMatch = false);
	void HandleMatchHasStarted(bool bTeamsMatch = false);
	void HandleCooldown();

	float SingleTripTime = 0.f;
	FHighPingDelegate HighPingDelegate; // Delegate for high ping events

	void BroadcastElim(APlayerState* Attacker, APlayerState* Victim);

	/** 엔딩 중 입력 차단 */
	UFUNCTION(BlueprintCallable, Category = "Ending")
	void SetInputBlocked(bool bBlocked);

	UFUNCTION(Client, Reliable)
	void ClientPlayEndingSequence(const TSoftObjectPtr<ULevelSequence>& SequenceAsset);
	UFUNCTION(BlueprintImplementableEvent, Category = "Ending")
	void BP_PlayEndingSequence(ULevelSequence* Sequence);

protected:
	virtual void BeginPlay() override;
	void SetHUDTime();
	void PollInit(); // Polls for the BlasterHUD and CharacterOverlay widgets
	virtual void SetupInputComponent() override;

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
	void ClientJoinMidgame(FName StateOfMatch, float Warmup, float Match, float Cooldown, float StartingTime); // Client joins mid-game and initializes HUD

	void HighPingWarning();
	void StopHighPingWarning();
	void CheckPing(float DeltaTime);

	void ShowReturnToMainMenu();
	
	UFUNCTION(Client, Reliable)
	void ClientElimAnnouncement(APlayerState* Attacker, APlayerState* Victim);

	UPROPERTY(ReplicatedUsing = OnRep_ShowTeamScores)
	bool bShowTeamScores = false; // Flag to check if team scores are shown

	UFUNCTION()
	void OnRep_ShowTeamScores(); // Called when ShowTeamScores is replicated

	FString GetInfoText(const TArray<class ABlasterPlayerState*>& Players); // Returns a string with player information for the HUD
	FString GetTeamsInfoText(class ABlasterGameState* BlasterGameState); // Returns a string with team information for the HUD
private:

	UPROPERTY()
	class ABlasterHUD* BlasterHUD;

	/**
	* Return to Main Menu
	*/
	UPROPERTY(EditAnywhere, Category = "HUD")
	TSubclassOf<class UUserWidget> ReturnToMainMenuWidget; // Widget class for return to main menu

	class UReturnToMainMenu* ReturnToMainMenu;

	bool bReturnToMainMenuOpen = false; // Flag to check if the return to main menu widget is open

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

	UFUNCTION(Server, Reliable)
	void ServerReportPingStatus(bool bHighPing); // Server reports high ping status

	/** 엔딩 중 입력 차단 상태 */
	UPROPERTY()
	bool bEndingInputBlocked = false;

	UFUNCTION(Client, Reliable)
	void ClientSetInputBlocked(bool bBlocked);


	UPROPERTY(EditAnywhere)
	float HighPingThreshold = 50.f; // Threshold for high ping warning
};
