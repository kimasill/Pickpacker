// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PickpackerTypes/CoreLoopTypes.h"
#include "UI/NPCDialogueUIData.h"

class ULevelSequence;
class UUserWidget;
class UInputAction;
class UTrainDestinationSelectionWidget;
class UTrainTravelComponent;
class UNPCDialogueWidget;

#include "BlasterPlayerController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHighPingDelegate, bool, bHighPing);
/**
 * 
 */
UCLASS()
class PICKPACKER_API ABlasterPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ABlasterPlayerController();
	
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
	void ClientPlayEndingSequence(const TSoftObjectPtr<ULevelSequence>& SequenceAsset, bool bHidePlayerInSequence = true);
	UFUNCTION(BlueprintImplementableEvent, Category = "Ending")
	void BP_PlayEndingSequence(ULevelSequence* Sequence, bool bHidePlayerInSequence);

	/** 시퀀스 시작 시 플레이어/HUD 숨김 (탈출 시퀀스 등, 블루프린트에서 구현) */
	UFUNCTION(Client, Reliable)
	void Client_HideForSequence(bool bIncludeSelf = true);
	UFUNCTION(BlueprintImplementableEvent, Category = "Ending")
	void BP_HideForSequence();

	/** 크레딧 재생 (엔딩 시퀀스 후 호출, 블루프린트에서 구현) */
	UFUNCTION(Client, Reliable)
	void ClientPlayCredits();
	UFUNCTION(BlueprintImplementableEvent, Category = "Ending")
	void BP_PlayCredits();

	/** 크레딧 종료 시 블루프린트에서 호출. 서버에 맵 이동 요청 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Ending")
	void ServerRequestTravelAfterCredits();

	/** 엔딩 레벨 로드 완료 시 서버에 알림 (동시 시퀀스 재생용) */
	UFUNCTION(Server, Reliable, Category = "Ending")
	void Server_NotifyEndingLevelReady();
	UFUNCTION(Client, Reliable, Category = "Ending")
	void Client_RequestEndingLevelReady();

	/** 시퀀스 시작 전까지 검은 화면 표시 (별도 위젯 블루프린트에서 구현) */
	UFUNCTION(Client, Reliable, Category = "Ending")
	void Client_ShowEndingBlackScreen();
	UFUNCTION(Client, Reliable, Category = "Ending")
	void Client_HideEndingBlackScreen();
	UFUNCTION(BlueprintImplementableEvent, Category = "Ending")
	void BP_ShowEndingBlackScreen();
	UFUNCTION(BlueprintImplementableEvent, Category = "Ending")
	void BP_HideEndingBlackScreen();


	/** 사망 후 관전 시작 */
	UFUNCTION(Client, Reliable)
	void ClientBeginDeathSpectate();

	/** 로딩 화면 표시/해제 */
	UFUNCTION(Client, Reliable)
	void ClientShowLoadingScreen();
	UFUNCTION(Client, Reliable)
	void ClientShowLoadingScreenWithKey(FName TextKey, FName CompleteTextKey = NAME_None, float CompleteTextDelay = 0.0f);
	UFUNCTION(Client, Reliable)
	void ClientNotifyLevelLoaded();
	UFUNCTION(Client, Reliable)
	void ClientHideLoadingScreen();

	virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel) override;
	virtual void PostSeamlessTravel() override;
	virtual void OnRep_Pawn() override;

	/** 로비 복귀 보장 (클라이언트에서 로비가 아니면 접속 재시도) */
	UFUNCTION(Client, Reliable)
	void ClientEnsureLobbyTravel(const FString& HostAddress, const FString& LobbyPath);

	// --- 열차 목적지 선택 ---

	/** 클라이언트 → 서버: 목적지 선택 요청 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Train")
	void ServerSelectTrainDestination(FName DestinationId);

	/** 서버 → 클라이언트: 목적지 선택 UI 표시 */
	UFUNCTION(Client, Reliable, Category = "Train")
	void ClientOpenDestinationSelectUI();

	/** 블루프린트에서 목적지 선택 UI 구현 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Train")
	void BP_OpenDestinationSelectUI();

	UFUNCTION(BlueprintCallable, Category = "Loading")
	void ShowLoadingScreen();
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void ShowLoadingScreenWithKey(FName TextKey, FName CompleteTextKey = NAME_None, float CompleteTextDelay = 0.0f);
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void HideLoadingScreen();
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void SetLoadingTextKey(FName TextKey);

	UFUNCTION(Client, Reliable, Category = "Dialogue")
	void ClientShowNPCDialogue(AActor* DialogueActor, const FDialogueUIState& DialogueState);

	UFUNCTION(Client, Reliable, Category = "Dialogue")
	void ClientCloseNPCDialogue(AActor* DialogueActor);

	UFUNCTION(Server, Reliable, Category = "Dialogue")
	void ServerSelectNPCDialogueChoice(AActor* DialogueActor, int32 ChoiceIndex);

	UFUNCTION(Server, Reliable, Category = "Dialogue")
	void ServerAdvanceNPCDialogue(AActor* DialogueActor);

	UFUNCTION(Server, Reliable, Category = "Dialogue")
	void ServerCloseNPCDialogue(AActor* DialogueActor);


protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void SetHUDTime();
	void PollInit(); // Polls for the BlasterHUD and CharacterOverlay widgets
	virtual void SetupInputComponent() override;
	void EnsureDefaultAssetReferences();
	void EnsureGameplayHUD();
	bool EnsurePickpackerHUDWidget();

	/** 로비 설정 패널 토글 (키 바인딩 필요: "LobbyPanel") */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void ToggleLobbySettingsPanel();

	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void TogglePanel();

	/** 레거시 블루프린트 호환용 로비 UI 열기 엔트리 포인트 */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OnOpenLobbyUI();

	/** 시맨틱 트래블 후 블루프린트 HUD 재생성용. BP_BlasterPlayerController에서 이벤트 구현 시 WBP_PickPackerHUD Create Widget + Add to Viewport 호출 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void OnPostSeamlessTravel_RecreateHUD();

	/** 지연 HUD 생성 (패키징 빌드 클라이언트에서 BeginPlay 타이밍 이슈 회피). BP에서 CreateHUD와 동일 로직 구현 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void BP_DeferredCreateHUD();

	/** 타이머 콜백: BP_DeferredCreateHUD 호출 */
	void TriggerDeferredHUDCreation();

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

	/** 로딩 화면 위젯 */
	UPROPERTY(EditAnywhere, Category = "Loading")
	TSubclassOf<UUserWidget> LoadingScreenWidgetClass;

	UPROPERTY()
	UUserWidget* LoadingScreenWidget;

	UPROPERTY(EditAnywhere, Category = "Train|UI")
	TSubclassOf<UTrainDestinationSelectionWidget> TrainDestinationSelectionWidgetClass;

	UPROPERTY()
	TObjectPtr<UTrainDestinationSelectionWidget> TrainDestinationSelectionWidget;

	UPROPERTY()
	TObjectPtr<UTrainTravelComponent> BoundTrainTravelComponent;

	UPROPERTY(EditAnywhere, Category = "Dialogue|UI")
	TSubclassOf<UNPCDialogueWidget> NPCDialogueWidgetClass;

	UPROPERTY()
	TObjectPtr<UNPCDialogueWidget> NPCDialogueWidget;

	UPROPERTY()
	TObjectPtr<AActor> ActiveDialogueActor;

	/** 로딩 화면에 표시할 텍스트 맵 */
	UPROPERTY(EditAnywhere, Category = "Loading")
	TMap<FName, FString> LoadingTextMap;

	UPROPERTY(EditAnywhere, Category = "Loading")
	FName DefaultLoadingTextKey = TEXT("Booting");

	UPROPERTY()
	FString LoadingText;

	UPROPERTY()
	FName PendingCompleteTextKey = NAME_None;

	UPROPERTY()
	float PendingCompleteTextDelay = 0.0f;

	FTimerHandle LoadingCompleteTimerHandle;

	void HandlePostLoadMap(UWorld* LoadedWorld);
	void UpdateLoadingScreenText();
	void EnsureTrainDestinationSelectionBinding();
	void EnsureTrainDestinationSelectionWidget();
	void RefreshTrainDestinationSelectionWidget();
	void SetTrainDestinationSelectionVisible(bool bVisible);
	void SetGameplayHUDVisible(bool bVisible);
	void UpdateGameplayHUDVisibility();
	UTrainTravelComponent* GetTrainTravelComponent() const;
	void EnsureNPCDialogueWidget();
	void SetNPCDialogueVisible(bool bVisible);

	UFUNCTION()
	void HandleNPCDialogueChoiceSelected(int32 ChoiceIndex);

	UFUNCTION()
	void HandleNPCDialogueAdvanceRequested();

	UFUNCTION()
	void HandleNPCDialogueClosedRequested();

	UFUNCTION()
	void HandleTrainSelectionContextUpdated(const FTrainSelectionContext& SelectionContext);

	UFUNCTION()
	void HandleTrainDestinationVotesUpdated(const TArray<FTrainDestinationVoteState>& VoteStates);

	UFUNCTION()
	void HandleTrainRouteSelectionResultUpdated(const FRouteSelectionResult& RouteSelectionResult);

	/**
	* Return to Main Menu
	*/
	UPROPERTY(EditAnywhere, Category = "HUD")
	TSubclassOf<class UUserWidget> ReturnToMainMenuWidget; // Widget class for return to main menu

	/** 로비 설정 패널 위젯 */
	UPROPERTY(EditAnywhere, Category = "Lobby|UI")
	TSubclassOf<class UUserWidget> LobbySettingsWidgetClass;

	UPROPERTY()
	class UUserWidget* LobbySettingsWidget;

	bool bLobbySettingsOpen = false;

	UPROPERTY()  // UPROPERTY 필수 - 없으면 GC에 의해 위젯이 수거되어 ESC 메뉴 열 때 크래시 (파슬 제출 후 등)
	TObjectPtr<class UReturnToMainMenu> ReturnToMainMenu;

	bool bReturnToMainMenuOpen = false; // Flag to check if the return to main menu widget is open

	/** Enhanced Input Actions */
	UPROPERTY(EditDefaultsOnly, Category = "Input|UI")
	TObjectPtr<UInputAction> QuitAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|UI")
	TObjectPtr<UInputAction> LobbyPanelAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|UI")
	TObjectPtr<UInputAction> PanelAction;

	/** 관전 전환 입력 */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Spectate")
	TObjectPtr<UInputAction> SpectateNextAction;

	UPROPERTY()
	class ABlasterGameMode* BlasterGameMode;

	/** 관전 상태 */
	UPROPERTY()
	bool bDeathSpectating = false;

	UPROPERTY()
	bool bSpectateFadeInPending = false;

	UPROPERTY()
	int32 CurrentSpectateIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, Category = "Spectate")
	float SpectateFadeDuration = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Spectate")
	float SpectateBlendTime = 0.2f;

	FTimerHandle SpectateFadeTimerHandle;

	void HandleSpectateNext();
	void SwitchSpectateTarget(int32 Direction);
	TArray<AActor*> GetSpectateTargets() const;

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
