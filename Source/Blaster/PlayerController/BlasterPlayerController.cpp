// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterPlayerController.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "Blaster/HUD/BlasterHUD.h"
#include "Blaster/HUD/CharacterOverlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Blaster/GameMode/BlasterGameMode.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Blaster/HUD/Announcement.h"
#include "Kismet/GameplayStatics.h"
#include "Blaster/BlasterComponents/CombatComponent.h"
#include "Blaster/GameState/BlasterGameState.h"
#include "Components/Image.h"
#include "Blaster/HUD/ReturnToMainMenu.h"
#include "Blaster/BlasterTypes/Announcement.h"
#include "Blaster/UI/InventoryWidget.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "LevelSequence.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "GameFramework/GameStateBase.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	static void AppendDebugLog(const FString& JsonLine)
	{
		const FString LogDir = TEXT("s:/Project/Unreal5/Blaster/.cursor/debug.log");
		FFileHelper::SaveStringToFile(JsonLine + LINE_TERMINATOR, *LogDir, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
	}

	static bool bTravelDelegatesBound = false;
	static bool bMapDelegatesBound = false;

	static void OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
	{
		// #region agent log
		AppendDebugLog(FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H23\",\"location\":\"BlasterPlayerController.cpp:44\",\"message\":\"TravelFailure\",\"data\":{\"world\":\"%s\",\"type\":%d,\"error\":\"%s\"},\"timestamp\":%lld}"),
			World ? *World->GetMapName() : TEXT("none"),
			static_cast<int32>(FailureType),
			*ErrorString,
			FDateTime::UtcNow().ToUnixTimestamp() * 1000));
		// #endregion
	}

	static void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
	{
		// #region agent log
		AppendDebugLog(FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H24\",\"location\":\"BlasterPlayerController.cpp:55\",\"message\":\"NetworkFailure\",\"data\":{\"world\":\"%s\",\"type\":%d,\"error\":\"%s\",\"netDriver\":\"%s\"},\"timestamp\":%lld}"),
			World ? *World->GetMapName() : TEXT("none"),
			static_cast<int32>(FailureType),
			*ErrorString,
			NetDriver ? *NetDriver->GetName() : TEXT("none"),
			FDateTime::UtcNow().ToUnixTimestamp() * 1000));
		// #endregion
	}

	static void OnPreLoadMap(const FString& MapName)
	{
		// #region agent log
		AppendDebugLog(FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H31\",\"location\":\"BlasterPlayerController.cpp:66\",\"message\":\"PreLoadMap\",\"data\":{\"map\":\"%s\"},\"timestamp\":%lld}"),
			*MapName,
			FDateTime::UtcNow().ToUnixTimestamp() * 1000));
		// #endregion
	}

	static void OnPostLoadMap(UWorld* World)
	{
		// #region agent log
		AppendDebugLog(FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H32\",\"location\":\"BlasterPlayerController.cpp:74\",\"message\":\"PostLoadMap\",\"data\":{\"world\":\"%s\",\"type\":%d,\"hasWorldSettings\":%s},\"timestamp\":%lld}"),
			World ? *World->GetMapName() : TEXT("none"),
			World ? static_cast<int32>(World->WorldType) : -1,
			(World && World->GetWorldSettings()) ? TEXT("true") : TEXT("false"),
			FDateTime::UtcNow().ToUnixTimestamp() * 1000));
		// #endregion
	}

	static void OnPreWorldInit(UWorld* World, const UWorld::InitializationValues IVS)
	{
		// #region agent log
		AppendDebugLog(FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H35\",\"location\":\"BlasterPlayerController.cpp:82\",\"message\":\"PreWorldInit\",\"data\":{\"world\":\"%s\",\"type\":%d},\"timestamp\":%lld}"),
			World ? *World->GetMapName() : TEXT("none"),
			World ? static_cast<int32>(World->WorldType) : -1,
			FDateTime::UtcNow().ToUnixTimestamp() * 1000));
		// #endregion
	}

	static void OnPostWorldInit(UWorld* World, const UWorld::InitializationValues IVS)
	{
		// #region agent log
		AppendDebugLog(FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H36\",\"location\":\"BlasterPlayerController.cpp:91\",\"message\":\"PostWorldInit\",\"data\":{\"world\":\"%s\",\"type\":%d,\"hasWorldSettings\":%s},\"timestamp\":%lld}"),
			World ? *World->GetMapName() : TEXT("none"),
			World ? static_cast<int32>(World->WorldType) : -1,
			(World && World->GetWorldSettings()) ? TEXT("true") : TEXT("false"),
			FDateTime::UtcNow().ToUnixTimestamp() * 1000));
		// #endregion
	}
}

void ABlasterPlayerController::BroadcastElim(APlayerState* Attacker, APlayerState* Victim)
{
	ClientElimAnnouncement(Attacker, Victim);
}

void ABlasterPlayerController::ClientElimAnnouncement_Implementation(APlayerState* Attacker, APlayerState* Victim)
{
	APlayerState* Self = GetPlayerState<APlayerState>();
	if (Attacker && Victim && Self)
	{
		BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
		if (BlasterHUD)
		{
			if (Attacker == Self && Victim != Self)
			{
				BlasterHUD->AddElimAnnouncement("You", Victim->GetPlayerName());
				return;
			}
			if(Victim == Self && Attacker != Self)
			{
				BlasterHUD->AddElimAnnouncement(Attacker->GetPlayerName(), "You");
				return;
			}
			if(Attacker == Self && Victim == Self)
			{
				BlasterHUD->AddElimAnnouncement("You", "Yourself");
				return;
			}
			if(Attacker == Victim && Attacker != Self)
			{
				BlasterHUD->AddElimAnnouncement(Attacker->GetPlayerName(), Victim->GetPlayerName());
				return;
			}
			BlasterHUD->AddElimAnnouncement(Attacker->GetPlayerName(), Victim->GetPlayerName());
		}
	}
}
ABlasterPlayerController::ABlasterPlayerController()
{
	LoadingTextMap.Add(TEXT("Booting"), TEXT("시스템 부팅 중.."));
	LoadingTextMap.Add(TEXT("Ready"), TEXT("곧 작동을 시작합니다"));
	LoadingTextMap.Add(TEXT("Error"), TEXT("시스템 오류 -"));
}

void ABlasterPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!bTravelDelegatesBound && GEngine)
	{
		GEngine->OnTravelFailure().AddStatic(&OnTravelFailure);
		GEngine->OnNetworkFailure().AddStatic(&OnNetworkFailure);
		bTravelDelegatesBound = true;
		// #region agent log
		AppendDebugLog(FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H25\",\"location\":\"BlasterPlayerController.cpp:96\",\"message\":\"BindTravelDelegates\",\"data\":{},\"timestamp\":%lld}"),
			FDateTime::UtcNow().ToUnixTimestamp() * 1000));
		// #endregion
	}
	if (!bMapDelegatesBound)
	{
		FCoreUObjectDelegates::PreLoadMap.AddStatic(&OnPreLoadMap);
		FCoreUObjectDelegates::PostLoadMapWithWorld.AddStatic(&OnPostLoadMap);
		FWorldDelegates::OnPreWorldInitialization.AddStatic(&OnPreWorldInit);
		FWorldDelegates::OnPostWorldInitialization.AddStatic(&OnPostWorldInit);
		bMapDelegatesBound = true;
	}
	// #region agent log
	AppendDebugLog(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H22\",\"location\":\"BlasterPlayerController.cpp:102\",\"message\":\"BeginPlay\",\"data\":{\"world\":\"%s\",\"netMode\":%d,\"hasServerConn\":%s},\"timestamp\":%lld}"),
		GetWorld() ? *GetWorld()->GetMapName() : TEXT("none"),
		GetWorld() ? static_cast<int32>(GetWorld()->GetNetMode()) : -1,
		(GetWorld() && GetWorld()->GetNetDriver() && GetWorld()->GetNetDriver()->ServerConnection) ? TEXT("true") : TEXT("false"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion

	// 1인칭 시점 Pitch 제한 설정 (-80 ~ +80도)
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -80.0f;
		PlayerCameraManager->ViewPitchMax = 80.0f;
	}

	BlasterHUD = Cast<ABlasterHUD>(GetHUD());
	ServerCheckMatchState();
}

void ABlasterPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABlasterPlayerController, MatchState);
	DOREPLIFETIME(ABlasterPlayerController, bShowTeamScores);
}

void ABlasterPlayerController::HideTeamScores()
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->RedTeamScore &&
		BlasterHUD->CharacterOverlay->BlueTeamScore &&
		BlasterHUD->CharacterOverlay->ScoreSpacerText;
	if (bHUDValid)
	{
		BlasterHUD->CharacterOverlay->RedTeamScore->SetVisibility(ESlateVisibility::Hidden);
		BlasterHUD->CharacterOverlay->BlueTeamScore->SetVisibility(ESlateVisibility::Hidden);
		BlasterHUD->CharacterOverlay->ScoreSpacerText->SetVisibility(ESlateVisibility::Hidden);
	}
}

void ABlasterPlayerController::InitTeamScores()
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->RedTeamScore &&
		BlasterHUD->CharacterOverlay->BlueTeamScore &&
		BlasterHUD->CharacterOverlay->ScoreSpacerText;
	if (bHUDValid)
	{
		FString Zero("0");
		FString Spacer(" | ");
		BlasterHUD->CharacterOverlay->RedTeamScore->SetVisibility(ESlateVisibility::Visible);
		BlasterHUD->CharacterOverlay->BlueTeamScore->SetVisibility(ESlateVisibility::Visible);
		BlasterHUD->CharacterOverlay->ScoreSpacerText->SetVisibility(ESlateVisibility::Visible);
		BlasterHUD->CharacterOverlay->RedTeamScore->SetText(FText::FromString(Zero));
		BlasterHUD->CharacterOverlay->BlueTeamScore->SetText(FText::FromString(Zero));
		BlasterHUD->CharacterOverlay->ScoreSpacerText->SetText(FText::FromString(Spacer));
	}
}

void ABlasterPlayerController::SetHUDRedTeamScore(int32 Score)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->RedTeamScore;
	if (bHUDValid)
	{
		FString ScoreText = FString::Printf(TEXT("%d"), Score);
		BlasterHUD->CharacterOverlay->RedTeamScore->SetText(FText::FromString(ScoreText));
	}
}

void ABlasterPlayerController::SetHUDBlueTeamScore(int32 Score)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->BlueTeamScore;
	if (bHUDValid)
	{
		FString ScoreText = FString::Printf(TEXT("%d"), Score);
		BlasterHUD->CharacterOverlay->BlueTeamScore->SetText(FText::FromString(ScoreText));
	}
}

void ABlasterPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	SetHUDTime();
	CheckTimeSync(DeltaTime);
	PollInit();
	CheckPing(DeltaTime);
}

void ABlasterPlayerController::CheckPing(float DeltaTime)
{
	if (HasAuthority()) return;
	HighPingRunningTime += DeltaTime;
	if (HighPingRunningTime > CheckPingFrequency)
	{
		PlayerState = PlayerState == nullptr ? TObjectPtr<APlayerState>(GetPlayerState<APlayerState>()) : PlayerState;
		if (PlayerState)
		{
			UE_LOG(LogTemp, Warning, TEXT("PlayerState->GetPing() * 4 : %f"), PlayerState->ExactPing * 4);
			if (PlayerState->ExactPing * 4 > HighPingThreshold) // ping is compressed; it's actually ping / 4
			{
				HighPingWarning();
				PingAnimationRunningTime = 0.f;
				ServerReportPingStatus(true);
			}
			else
			{
				ServerReportPingStatus(false);
			}
		}
		HighPingRunningTime = 0.f;
	}
	bool bHighPingAnimationPlaying =
		BlasterHUD && BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->HighPingAnimation &&
		BlasterHUD->CharacterOverlay->IsAnimationPlaying(BlasterHUD->CharacterOverlay->HighPingAnimation);
	if (bHighPingAnimationPlaying)
	{
		PingAnimationRunningTime += DeltaTime;
		if (PingAnimationRunningTime > HighPingDuration)
		{
			StopHighPingWarning();
		}
	}
}

void ABlasterPlayerController::ShowReturnToMainMenu()
{
	if (ReturnToMainMenuWidget == nullptr) return;
	if (ReturnToMainMenu == nullptr)
	{
		ReturnToMainMenu = CreateWidget<UReturnToMainMenu>(this, ReturnToMainMenuWidget);
	}
	if (ReturnToMainMenu)
	{
		bReturnToMainMenuOpen = !bReturnToMainMenuOpen;
		if(bReturnToMainMenuOpen)
		{
			ReturnToMainMenu->MenuSetup();			
		}
		else
		{
			ReturnToMainMenu->MenuTearDown();
		}
	}
}

void ABlasterPlayerController::OnRep_ShowTeamScores()
{
	if(bShowTeamScores)
	{
		InitTeamScores();
	}
	else
	{
		HideTeamScores();
	}
}


// Is the ping too high?
void ABlasterPlayerController::ServerReportPingStatus_Implementation(bool bHighPing)
{
	HighPingDelegate.Broadcast(bHighPing);
}

void ABlasterPlayerController::CheckTimeSync(float DeltaTime)
{
	TimeSyncRunningTime += DeltaTime;
	if (IsLocalController() && TimeSyncRunningTime > TimeSyncFrequency)
	{
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
		TimeSyncRunningTime = 0.f;
	}
}

void ABlasterPlayerController::HighPingWarning()
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->HighPingImage &&
		BlasterHUD->CharacterOverlay->HighPingAnimation;
	if (bHUDValid)
	{
		BlasterHUD->CharacterOverlay->HighPingImage->SetOpacity(1.f);
		BlasterHUD->CharacterOverlay->PlayAnimation(
			BlasterHUD->CharacterOverlay->HighPingAnimation,
			0.f,
			5);
	}
}

void ABlasterPlayerController::StopHighPingWarning()
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->HighPingImage &&
		BlasterHUD->CharacterOverlay->HighPingAnimation;
	if (bHUDValid)
	{
		BlasterHUD->CharacterOverlay->HighPingImage->SetOpacity(0.f);
		if (BlasterHUD->CharacterOverlay->IsAnimationPlaying(BlasterHUD->CharacterOverlay->HighPingAnimation))
		{
			BlasterHUD->CharacterOverlay->StopAnimation(BlasterHUD->CharacterOverlay->HighPingAnimation);
		}
	}
}

void ABlasterPlayerController::ServerCheckMatchState_Implementation()
{
	ABlasterGameMode* GameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode)
	{
		WarmupTime = GameMode->WarmupTime;
		MatchTime = GameMode->MatchTime;
		CooldownTime = GameMode->CooldownTime;
		LevelStartingTime = GameMode->LevelStartingTime;
		MatchState = GameMode->GetMatchState();
		ClientJoinMidgame(MatchState, WarmupTime, MatchTime, CooldownTime, LevelStartingTime);
	}
}

void ABlasterPlayerController::ClientJoinMidgame_Implementation(FName StateOfMatch, float Warmup, float Match, float Cooldown, float StartingTime)
{
	WarmupTime = Warmup;
	MatchTime = Match;
	CooldownTime = Cooldown;
	LevelStartingTime = StartingTime;
	MatchState = StateOfMatch;
	OnMatchStateSet(MatchState);
	if (BlasterHUD && MatchState == MatchState::WaitingToStart)
	{
		BlasterHUD->AddAnnouncement();
	}
}

void ABlasterPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(InPawn);
	if (BlasterCharacter)
	{
		SetHUDHealth(BlasterCharacter->GetHealth(), BlasterCharacter->GetMaxHealth());
		if (IsLocalController())
		{
			// 로비 입력 차단 상태가 남아있는 경우 해제
			bEndingInputBlocked = false;
			SetIgnoreMoveInput(false);
			SetIgnoreLookInput(false);
			FInputModeGameOnly InputMode;
			SetInputMode(InputMode);
			bShowMouseCursor = false;
			if (BlasterCharacter->IsEndingInProgress())
			{
				BlasterCharacter->SetEndingInProgress(false);
			}
		}
	}
	const int32 MoveModeValue = (BlasterCharacter && BlasterCharacter->GetCharacterMovement())
		? static_cast<int32>(BlasterCharacter->GetCharacterMovement()->MovementMode)
		: -1;
	const int32 NetModeValue = GetWorld() ? static_cast<int32>(GetWorld()->GetNetMode()) : -1;
	// #region agent log
	AppendDebugLog(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H43\",\"location\":\"BlasterPlayerController.cpp:434\",\"message\":\"OnPossess state\",\"data\":{\"playerName\":\"%s\",\"blocked\":%s,\"ignoreMove\":%s,\"ignoreLook\":%s,\"ending\":%s,\"moveMode\":\"%s\",\"world\":\"%s\",\"netMode\":\"%s\"},\"timestamp\":%lld}"),
		GetPlayerState<APlayerState>() ? *GetPlayerState<APlayerState>()->GetPlayerName() : TEXT("none"),
		bEndingInputBlocked ? TEXT("true") : TEXT("false"),
		IsMoveInputIgnored() ? TEXT("true") : TEXT("false"),
		IsLookInputIgnored() ? TEXT("true") : TEXT("false"),
		BlasterCharacter ? (BlasterCharacter->IsEndingInProgress() ? TEXT("true") : TEXT("false")) : TEXT("none"),
		*FString::FromInt(MoveModeValue),
		GetWorld() ? *GetWorld()->GetMapName() : TEXT("none"),
		*FString::FromInt(NetModeValue),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
}

void ABlasterPlayerController::SetHUDHealth(float Health, float MaxHealth)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->HealthBar &&
		BlasterHUD->CharacterOverlay->HealthText;
	if (bHUDValid)
	{
		const float HealthPercent = Health / MaxHealth;
		BlasterHUD->CharacterOverlay->HealthBar->SetPercent(HealthPercent);
		FString HealthText = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Health), FMath::CeilToInt(MaxHealth));
		BlasterHUD->CharacterOverlay->HealthText->SetText(FText::FromString(HealthText));
	}
	else
	{
		bInitializeHealth = true;
		HUDHealth = Health;
		HUDMaxHealth = MaxHealth;
	}
}

void ABlasterPlayerController::SetHUDShield(float Shield, float MaxShield)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->ShieldBar &&
		BlasterHUD->CharacterOverlay->ShieldText;
	if (bHUDValid)
	{
		const float ShieldPercent = Shield / MaxShield;
		BlasterHUD->CharacterOverlay->ShieldBar->SetPercent(ShieldPercent);
		FString ShieldText = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Shield), FMath::CeilToInt(MaxShield));
		BlasterHUD->CharacterOverlay->ShieldText->SetText(FText::FromString(ShieldText));
	}
	else
	{
		bInitializeShield = true;
		HUDShield = Shield;
		HUDMaxShield = MaxShield;
	}
}

void ABlasterPlayerController::SetHUDScore(float Score)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->ScoreAmount;

	if (bHUDValid)
	{
		FString ScoreText = FString::Printf(TEXT("%d"), FMath::FloorToInt(Score));
		BlasterHUD->CharacterOverlay->ScoreAmount->SetText(FText::FromString(ScoreText));
	}
	else
	{
		bInitializeScore = true;
		HUDScore = Score;
	}
}

void ABlasterPlayerController::SetHUDDefeats(int32 Defeats)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->DefeatsAmount;
	if (bHUDValid)
	{
		FString DefeatsText = FString::Printf(TEXT("%d"), Defeats);
		BlasterHUD->CharacterOverlay->DefeatsAmount->SetText(FText::FromString(DefeatsText));
	}
	else
	{
		bInitializeDefeats = true;
		HUDDefeats = Defeats;
	}
}

void ABlasterPlayerController::SetHUDWeaponAmmo(int32 Ammo)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->WeaponAmmoAmount;
	if (bHUDValid)
	{
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		BlasterHUD->CharacterOverlay->WeaponAmmoAmount->SetText(FText::FromString(AmmoText));
	}
	else
	{
		bInitializeWeaponAmmo = true;
		HUDWeaponAmmo = Ammo;
	}
}

void ABlasterPlayerController::SetHUDCarriedAmmo(int32 Ammo)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->CarriedAmmoAmount;
	if (bHUDValid)
	{
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		BlasterHUD->CharacterOverlay->CarriedAmmoAmount->SetText(FText::FromString(AmmoText));
	}
	else
	{
		bInitializeCarriedAmmo = true;
		HUDCarriedAmmo = Ammo;
	}
}

void ABlasterPlayerController::SetHUDMatchCountdown(float CountdownTime)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->MatchCountdownText;
	if (bHUDValid)
	{
		if (CountdownTime < 0.f)
		{
			BlasterHUD->CharacterOverlay->MatchCountdownText->SetText(FText());
			return;
		}

		int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
		int32 Seconds = CountdownTime - Minutes * 60;

		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		BlasterHUD->CharacterOverlay->MatchCountdownText->SetText(FText::FromString(CountdownText));
	}
}

void ABlasterPlayerController::SetHUDAnnouncementCountdown(float CountdownTime)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->Announcement &&
		BlasterHUD->Announcement->WarmupTime;
	if (bHUDValid)
	{
		if (CountdownTime < 0.f)
		{
			BlasterHUD->Announcement->WarmupTime->SetText(FText());
			return;
		}

		int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
		int32 Seconds = CountdownTime - Minutes * 60;

		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		BlasterHUD->Announcement->WarmupTime->SetText(FText::FromString(CountdownText));
	}
}

void ABlasterPlayerController::UpdateInventoryWidget(const TArray<AParcelActor*>& Items)
{
	// DEPRECATED: This method is no longer used. 
	// Inventory widgets now listen to PlayerInventoryComponent::OnInventoryUpdated delegate events directly.
	// Keeping for backwards compatibility but this should not be called.
	
	// Old implementation (disabled):
	// if (CharacterOverlay && CharacterOverlay->InventoryWidget)
	// {
	//     CharacterOverlay->InventoryWidget->UpdateInventory(Items);
	// }
}

void ABlasterPlayerController::SetHUDGrenades(int32 Grenades)
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	bool bHUDValid = BlasterHUD &&
		BlasterHUD->CharacterOverlay &&
		BlasterHUD->CharacterOverlay->GrenadesText;
	if (bHUDValid)
	{
		FString GrenadesText = FString::Printf(TEXT("%d"), Grenades);
		BlasterHUD->CharacterOverlay->GrenadesText->SetText(FText::FromString(GrenadesText));
	}
	else
	{
		bInitializeGrenades = true;
		HUDGrenades = Grenades;
	}
}

void ABlasterPlayerController::SetHUDTime()
{
	float TimeLeft = 0.f;
	if (MatchState == MatchState::WaitingToStart) TimeLeft = WarmupTime - GetServerTime() + LevelStartingTime;
	else if (MatchState == MatchState::InProgress) TimeLeft = WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;
	else if (MatchState == MatchState::Cooldown) TimeLeft = CooldownTime + WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;
	uint32 SecondsLeft = FMath::CeilToInt(TimeLeft);
	if (CountdownInt != SecondsLeft)
	{
		if (MatchState == MatchState::WaitingToStart || MatchState == MatchState::Cooldown)
		{
			SetHUDAnnouncementCountdown(TimeLeft);
		}
		if (MatchState == MatchState::InProgress)
		{
			SetHUDMatchCountdown(TimeLeft);
		}
	}

	CountdownInt = SecondsLeft;
}

void ABlasterPlayerController::PollInit()
{
	if (CharacterOverlay == nullptr)
	{
		if (BlasterHUD && BlasterHUD->CharacterOverlay)
		{
			CharacterOverlay = BlasterHUD->CharacterOverlay;
			if (CharacterOverlay)
			{
				if (bInitializeHealth) SetHUDHealth(HUDHealth, HUDMaxHealth);
				if (bInitializeShield) SetHUDShield(HUDShield, HUDMaxShield);
				if (bInitializeScore) SetHUDScore(HUDScore);
				if (bInitializeDefeats) SetHUDDefeats(HUDDefeats);
				if (bInitializeCarriedAmmo) SetHUDCarriedAmmo(HUDCarriedAmmo);
				if (bInitializeWeaponAmmo) SetHUDWeaponAmmo(HUDWeaponAmmo);

				ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
				if (BlasterCharacter && BlasterCharacter->GetCombat())
				{
					if (bInitializeGrenades) SetHUDGrenades(BlasterCharacter->GetCombat()->GetGrenades());
				}
			}
		}
	}
}
void ABlasterPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (QuitAction)
		{
			EnhancedInput->BindAction(QuitAction, ETriggerEvent::Started, this, &ABlasterPlayerController::ShowReturnToMainMenu);
		}
		if (LobbyPanelAction)
		{
			EnhancedInput->BindAction(LobbyPanelAction, ETriggerEvent::Started, this, &ABlasterPlayerController::ToggleLobbySettingsPanel);
		}
		if(PanelAction)
		{
			EnhancedInput->BindAction(PanelAction, ETriggerEvent::Started, this, &ABlasterPlayerController::TogglePanel);
		}
		if (SpectateNextAction)
		{
			EnhancedInput->BindAction(SpectateNextAction, ETriggerEvent::Started, this, &ABlasterPlayerController::HandleSpectateNext);
		}
	}
}
void ABlasterPlayerController::ServerRequestServerTime_Implementation(float TimeOfClientRequest)
{
	float ServerTimeOfReceipt = GetWorld()->GetTimeSeconds();
	ClientReportServerTime(TimeOfClientRequest, ServerTimeOfReceipt);
}

void ABlasterPlayerController::ClientReportServerTime_Implementation(float TimeOfClientRequest, float TimeServerReceivedClientRequest)
{
	float RoundTripTime = GetWorld()->GetTimeSeconds() - TimeOfClientRequest;
	SingleTripTime = 0.5f * RoundTripTime;
	float CurrentServerTime = TimeServerReceivedClientRequest + SingleTripTime;
	ClientServerDelta = CurrentServerTime - GetWorld()->GetTimeSeconds();
}

float ABlasterPlayerController::GetServerTime()
{
	if (HasAuthority()) return GetWorld()->GetTimeSeconds();
	else return GetWorld()->GetTimeSeconds() + ClientServerDelta;
}


void ABlasterPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	if (IsLocalController())
	{
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
	}
}

void ABlasterPlayerController::OnMatchStateSet(FName State, bool bTeamsMatch)
{
	MatchState = State;

	if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted(bTeamsMatch);
	}
	else if (MatchState == MatchState::Cooldown)
	{
		HandleCooldown();
	}
}

void ABlasterPlayerController::OnRep_MatchState()
{
	if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted();
	}
	else if (MatchState == MatchState::Cooldown)
	{
		HandleCooldown();
	}
}

void ABlasterPlayerController::HandleMatchHasStarted(bool bTeamsMatch)
{
	if(HasAuthority()) bShowTeamScores = bTeamsMatch;
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	if (BlasterHUD)
	{
		if (BlasterHUD->CharacterOverlay == nullptr) BlasterHUD->AddCharacterOverlay();
		if (BlasterHUD->Announcement)
		{
			BlasterHUD->Announcement->SetVisibility(ESlateVisibility::Hidden);
		}
		if (!HasAuthority()) return;
		if (bTeamsMatch)
		{
			InitTeamScores();
		}
		else
		{
			HideTeamScores();
		}
	}
}

void ABlasterPlayerController::HandleCooldown()
{
	BlasterHUD = BlasterHUD == nullptr ? Cast<ABlasterHUD>(GetHUD()) : BlasterHUD;
	if (BlasterHUD)
	{
		BlasterHUD->CharacterOverlay->RemoveFromParent();
		bool bHUDValid = BlasterHUD->Announcement &&
			BlasterHUD->Announcement->AnnouncementText &&
			BlasterHUD->Announcement->InfoText;

		if (bHUDValid)
		{
			BlasterHUD->Announcement->SetVisibility(ESlateVisibility::Visible);
			FString AnnouncementText = Announcement::NewMatchStartsIn;
			BlasterHUD->Announcement->AnnouncementText->SetText(FText::FromString(AnnouncementText));

			ABlasterGameState* BlasterGameState = Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this));
			ABlasterPlayerState* BlasterPlayerState = GetPlayerState<ABlasterPlayerState>();
			if (BlasterGameState && BlasterPlayerState)
			{
				TArray<ABlasterPlayerState*> TopPlayers = BlasterGameState->TopScoringPlayers;
				FString InfoTextString = bShowTeamScores? GetTeamsInfoText(BlasterGameState) : GetInfoText(TopPlayers);

				BlasterHUD->Announcement->InfoText->SetText(FText::FromString(InfoTextString));
			}
		}
	}
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
	if (BlasterCharacter && BlasterCharacter->GetCombat())
	{
		BlasterCharacter->bDisableGameplay = true;
		BlasterCharacter->GetCombat()->FireButtonPressed(false);
	}
}


FString ABlasterPlayerController::GetInfoText(const TArray<class ABlasterPlayerState*>& Players)
{
	ABlasterPlayerState* BlasterPlayerState = GetPlayerState<ABlasterPlayerState>();
	if (BlasterPlayerState == nullptr) return FString();
	FString InfoTextString;
	if (Players.Num() == 0)
	{
		InfoTextString = Announcement::ThereIsNoWinner;
	}
	else if (Players.Num() == 1 && Players[0] == BlasterPlayerState)
	{
		InfoTextString = Announcement::YouAreTheWinner;
	}
	else if (Players.Num() == 1)
	{
		InfoTextString = FString::Printf(TEXT("Winner: \n%s"), *Players[0]->GetPlayerName());
	}
	else if (Players.Num() > 1)
	{
		InfoTextString = Announcement::PlayersTiedForTheWin;
		InfoTextString.Append(FString("\n"));
		for (auto TiedPlayer : Players)
		{
			InfoTextString.Append(FString::Printf(TEXT("%s\n"), *TiedPlayer->GetPlayerName()));
		}
	}
	return InfoTextString;
}

FString ABlasterPlayerController::GetTeamsInfoText(ABlasterGameState* BlasterGameState)
{
	if (BlasterGameState == nullptr) return FString();

	FString InfoTextString;
	const int32 RedTeamScore = BlasterGameState->RedTeamScore;
	const int32 BlueTeamScore = BlasterGameState->BlueTeamScore;

	if (RedTeamScore == 0 && BlueTeamScore == 0)
	{
		InfoTextString = Announcement::ThereIsNoWinner;
	}
	else if (RedTeamScore == BlueTeamScore)
	{
		InfoTextString =  FString::Printf(TEXT("%s\n"), *Announcement::TeamsTiedForTheWin);
		InfoTextString.Append(Announcement::RedTeam);
		InfoTextString.Append(TEXT("\n"));
		InfoTextString.Append(Announcement::BlueTeam);
		InfoTextString.Append(TEXT("\n"));
	}
	else if (RedTeamScore > BlueTeamScore)
	{
		InfoTextString = Announcement::RedTeamWins;
		InfoTextString.Append(TEXT("\n"));
		InfoTextString.Append(FString::Printf(TEXT("%s: %d\n"), *Announcement::RedTeam, RedTeamScore));
		InfoTextString.Append(FString::Printf(TEXT("%s: %d\n"), *Announcement::BlueTeam, BlueTeamScore));
	}
	else
	{
		InfoTextString = Announcement::BlueTeamWins;
		InfoTextString.Append(TEXT("\n"));
		InfoTextString.Append(FString::Printf(TEXT("%s: %d\n"), *Announcement::BlueTeam, BlueTeamScore));
		InfoTextString.Append(FString::Printf(TEXT("%s: %d\n"), *Announcement::RedTeam, RedTeamScore));
	}		

	return InfoTextString;
}

void ABlasterPlayerController::SetInputBlocked(bool bBlocked)
{
	bEndingInputBlocked = bBlocked;
	// #region agent log
	AppendDebugLog(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H40\",\"location\":\"BlasterPlayerController.cpp:891\",\"message\":\"SetInputBlocked\",\"data\":{\"blocked\":%s,\"world\":\"%s\",\"netMode\":%d},\"timestamp\":%lld}"),
		bBlocked ? TEXT("true") : TEXT("false"),
		GetWorld() ? *GetWorld()->GetMapName() : TEXT("none"),
		GetWorld() ? static_cast<int32>(GetWorld()->GetNetMode()) : -1,
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion

	SetIgnoreMoveInput(bBlocked);
	SetIgnoreLookInput(bBlocked);

	if (ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn()))
	{
		BlasterCharacter->SetEndingInProgress(bBlocked);
	}

	if (!IsLocalController())
	{
		ClientSetInputBlocked(bBlocked);
	}
}

void ABlasterPlayerController::ClientShowLoadingScreen_Implementation()
{
	ShowLoadingScreen();
}

void ABlasterPlayerController::ClientShowLoadingScreenWithKey_Implementation(FName TextKey, FName CompleteTextKey, float CompleteTextDelay)
{
	ShowLoadingScreenWithKey(TextKey, CompleteTextKey, CompleteTextDelay);
}

void ABlasterPlayerController::PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
	// #region agent log
	AppendDebugLog(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H7\",\"location\":\"BlasterPlayerController.cpp:812\",\"message\":\"PreClientTravel\",\"data\":{\"url\":\"%s\",\"travelType\":%d,\"seamless\":%s,\"isLocal\":%s,\"world\":\"%s\",\"netMode\":%d},\"timestamp\":%lld}"),
		*PendingURL,
		static_cast<int32>(TravelType),
		bIsSeamlessTravel ? TEXT("true") : TEXT("false"),
		IsLocalController() ? TEXT("true") : TEXT("false"),
		GetWorld() ? *GetWorld()->GetMapName() : TEXT("none"),
		GetWorld() ? static_cast<int32>(GetWorld()->GetNetMode()) : -1,
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
	// #region agent log
	const bool bHasServerConn = (GetWorld() && GetWorld()->GetNetDriver() && GetWorld()->GetNetDriver()->ServerConnection);
	AppendDebugLog(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H19\",\"location\":\"BlasterPlayerController.cpp:818\",\"message\":\"PreClientTravel Conn\",\"data\":{\"hasServerConn\":%s},\"timestamp\":%lld}"),
		bHasServerConn ? TEXT("true") : TEXT("false"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
	// #region agent log
	int32 ConnState = -1;
	if (GetWorld() && GetWorld()->GetNetDriver() && GetWorld()->GetNetDriver()->ServerConnection)
	{
		ConnState = static_cast<int32>(GetWorld()->GetNetDriver()->ServerConnection->GetConnectionState());
	}
	AppendDebugLog(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H46\",\"location\":\"BlasterPlayerController.cpp:824\",\"message\":\"PreClientTravel ConnState\",\"data\":{\"state\":%d,\"inSeamless\":%s},\"timestamp\":%lld}"),
		ConnState,
		(GetWorld() && GetWorld()->IsInSeamlessTravel()) ? TEXT("true") : TEXT("false"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
	// #region agent log
	const AGameModeBase* AuthGM = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;
	AppendDebugLog(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H15\",\"location\":\"BlasterPlayerController.cpp:820\",\"message\":\"PreClientTravel GM\",\"data\":{\"gmClass\":\"%s\",\"gmSeamless\":%s},\"timestamp\":%lld}"),
		AuthGM ? *AuthGM->GetClass()->GetName() : TEXT("none"),
		(AuthGM && AuthGM->bUseSeamlessTravel) ? TEXT("true") : TEXT("false"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
	Super::PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);
}

void ABlasterPlayerController::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();
	// #region agent log
	AppendDebugLog(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H27\",\"location\":\"BlasterPlayerController.cpp:842\",\"message\":\"PostSeamlessTravel\",\"data\":{\"world\":\"%s\",\"netMode\":%d,\"hasServerConn\":%s},\"timestamp\":%lld}"),
		GetWorld() ? *GetWorld()->GetMapName() : TEXT("none"),
		GetWorld() ? static_cast<int32>(GetWorld()->GetNetMode()) : -1,
		(GetWorld() && GetWorld()->GetNetDriver() && GetWorld()->GetNetDriver()->ServerConnection) ? TEXT("true") : TEXT("false"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
}

void ABlasterPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();
	APawn* NewPawn = GetPawn();
	const bool bIsLobbyPawn = (NewPawn && NewPawn->GetClass()->GetName().Contains(TEXT("LobbyCharacter")));
	// #region agent log
	AppendDebugLog(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H47\",\"location\":\"BlasterPlayerController.cpp:858\",\"message\":\"OnRep_Pawn\",\"data\":{\"pawn\":\"%s\",\"isLobby\":%s,\"ignoreMove\":%s,\"ignoreLook\":%s},\"timestamp\":%lld}"),
		*GetNameSafe(NewPawn),
		bIsLobbyPawn ? TEXT("true") : TEXT("false"),
		IsMoveInputIgnored() ? TEXT("true") : TEXT("false"),
		IsLookInputIgnored() ? TEXT("true") : TEXT("false"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
	if (IsLocalController() && NewPawn && !bIsLobbyPawn)
	{
		bEndingInputBlocked = false;
		SetIgnoreMoveInput(false);
		SetIgnoreLookInput(false);
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
	}
}

void ABlasterPlayerController::ClientEnsureLobbyTravel_Implementation(const FString& HostAddress, const FString& LobbyPath)
{
	const FString CurrentMap = GetWorld() ? GetWorld()->GetMapName() : TEXT("none");
	const bool bIsAlreadyLobby = CurrentMap.Contains(TEXT("Lobby"));
	const bool bHasServerConn = (GetWorld() && GetWorld()->GetNetDriver() && GetWorld()->GetNetDriver()->ServerConnection);
	const bool bIsInSeamless = (GetWorld() && GetWorld()->IsInSeamlessTravel());
	int32 ConnState = -1;
	if (bHasServerConn)
	{
		ConnState = static_cast<int32>(GetWorld()->GetNetDriver()->ServerConnection->GetConnectionState());
	}
	// #region agent log
	AppendDebugLog(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H33\",\"location\":\"BlasterPlayerController.cpp:853\",\"message\":\"EnsureLobbyTravel pre\",\"data\":{\"world\":\"%s\",\"hasWorldSettings\":%s,\"hasServerConn\":%s,\"inSeamless\":%s,\"connState\":%d},\"timestamp\":%lld}"),
		GetWorld() ? *GetWorld()->GetMapName() : TEXT("none"),
		(GetWorld() && GetWorld()->GetWorldSettings()) ? TEXT("true") : TEXT("false"),
		bHasServerConn ? TEXT("true") : TEXT("false"),
		bIsInSeamless ? TEXT("true") : TEXT("false"),
		ConnState,
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
	// #region agent log
	AppendDebugLog(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H28\",\"location\":\"BlasterPlayerController.cpp:854\",\"message\":\"ClientEnsureLobbyTravel\",\"data\":{\"current\":\"%s\",\"isLobby\":%s,\"host\":\"%s\",\"path\":\"%s\"},\"timestamp\":%lld}"),
		*CurrentMap,
		bIsAlreadyLobby ? TEXT("true") : TEXT("false"),
		*HostAddress,
		*LobbyPath,
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
	if (bIsAlreadyLobby || HostAddress.IsEmpty() || LobbyPath.IsEmpty())
	{
		return;
	}
	if (bHasServerConn)
	{
		// #region agent log
		AppendDebugLog(FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H34\",\"location\":\"BlasterPlayerController.cpp:864\",\"message\":\"ClientTravel relative lobby\",\"data\":{\"url\":\"%s\"},\"timestamp\":%lld}"),
			*LobbyPath,
			FDateTime::UtcNow().ToUnixTimestamp() * 1000));
		// #endregion
		ClientTravel(LobbyPath, TRAVEL_Relative, true);
	}
	else
	{
		const FString TravelUrl = FString::Printf(TEXT("%s%s"), *HostAddress, *LobbyPath);
		// #region agent log
		AppendDebugLog(FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H28\",\"location\":\"BlasterPlayerController.cpp:874\",\"message\":\"ClientTravel to host lobby\",\"data\":{\"url\":\"%s\"},\"timestamp\":%lld}"),
			*TravelUrl,
			FDateTime::UtcNow().ToUnixTimestamp() * 1000));
		// #endregion
		ClientTravel(TravelUrl, TRAVEL_Absolute);
	}
}

void ABlasterPlayerController::ClientNotifyLevelLoaded_Implementation()
{
	// #region agent log
	AppendDebugLog(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H3\",\"location\":\"BlasterPlayerController.cpp:804\",\"message\":\"ClientNotifyLevelLoaded\",\"data\":{\"isLocal\":%s,\"world\":\"%s\",\"netMode\":%d,\"hasServerConn\":%s},\"timestamp\":%lld}"),
		IsLocalController() ? TEXT("true") : TEXT("false"),
		GetWorld() ? *GetWorld()->GetMapName() : TEXT("none"),
		GetWorld() ? static_cast<int32>(GetWorld()->GetNetMode()) : -1,
		(GetWorld() && GetWorld()->GetNetDriver() && GetWorld()->GetNetDriver()->ServerConnection) ? TEXT("true") : TEXT("false"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
	HandlePostLoadMap(GetWorld());
}

void ABlasterPlayerController::ClientHideLoadingScreen_Implementation()
{
	HideLoadingScreen();
}

void ABlasterPlayerController::ShowLoadingScreenWithKey(FName TextKey, FName CompleteTextKey, float CompleteTextDelay)
{
	SetLoadingTextKey(TextKey);
	PendingCompleteTextKey = CompleteTextKey;
	PendingCompleteTextDelay = CompleteTextDelay;
	ShowLoadingScreen();
}

void ABlasterPlayerController::SetLoadingTextKey(FName TextKey)
{
	if (const FString* Found = LoadingTextMap.Find(TextKey))
	{
		LoadingText = *Found;
	}
	else if (!TextKey.IsNone())
	{
		LoadingText = TextKey.ToString();
	}
	else if (const FString* DefaultFound = LoadingTextMap.Find(DefaultLoadingTextKey))
	{
		LoadingText = *DefaultFound;
	}
	else
	{
		LoadingText.Empty();
	}
}

void ABlasterPlayerController::ShowLoadingScreen()
{
	if (!IsLocalController())
	{
		return;
	}

	if (LoadingScreenWidget == nullptr && LoadingScreenWidgetClass)
	{
		LoadingScreenWidget = CreateWidget<UUserWidget>(this, LoadingScreenWidgetClass);
	}

	if (LoadingScreenWidget && !LoadingScreenWidget->IsInViewport())
	{
		LoadingScreenWidget->AddToViewport(1000);
	}

	if (LoadingText.IsEmpty())
	{
		SetLoadingTextKey(DefaultLoadingTextKey);
	}
	UpdateLoadingScreenText();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LoadingCompleteTimerHandle);
	}

	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = false;

	// #region agent log
	AppendDebugLog(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H4\",\"location\":\"BlasterPlayerController.cpp:874\",\"message\":\"ShowLoadingScreen\",\"data\":{\"map\":\"%s\"},\"timestamp\":%lld}"),
		GetWorld() ? *GetWorld()->GetMapName() : TEXT("none"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
}

void ABlasterPlayerController::HideLoadingScreen()
{
	if (!IsLocalController())
	{
		return;
	}

	if (LoadingScreenWidget && LoadingScreenWidget->IsInViewport())
	{
		LoadingScreenWidget->RemoveFromParent();
	}

	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);

	// #region agent log
	AppendDebugLog(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H5\",\"location\":\"BlasterPlayerController.cpp:894\",\"message\":\"HideLoadingScreen\",\"data\":{\"map\":\"%s\"},\"timestamp\":%lld}"),
		GetWorld() ? *GetWorld()->GetMapName() : TEXT("none"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
}

void ABlasterPlayerController::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!IsLocalController())
	{
		return;
	}

	// #region agent log
	AppendDebugLog(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H6\",\"location\":\"BlasterPlayerController.cpp:904\",\"message\":\"HandlePostLoadMap\",\"data\":{\"map\":\"%s\",\"pendingKey\":\"%s\",\"delay\":%.2f,\"netMode\":%d,\"hasServerConn\":%s},\"timestamp\":%lld}"),
		LoadedWorld ? *LoadedWorld->GetMapName() : TEXT("none"),
		*PendingCompleteTextKey.ToString(),
		PendingCompleteTextDelay,
		LoadedWorld ? static_cast<int32>(LoadedWorld->GetNetMode()) : -1,
		(LoadedWorld && LoadedWorld->GetNetDriver() && LoadedWorld->GetNetDriver()->ServerConnection) ? TEXT("true") : TEXT("false"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion

	if (!PendingCompleteTextKey.IsNone() && PendingCompleteTextDelay > 0.0f)
	{
		SetLoadingTextKey(PendingCompleteTextKey);
		UpdateLoadingScreenText();

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				LoadingCompleteTimerHandle,
				this,
				&ABlasterPlayerController::HideLoadingScreen,
				PendingCompleteTextDelay,
				false);
		}
	}
	else
	{
		HideLoadingScreen();
	}

	PendingCompleteTextKey = NAME_None;
	PendingCompleteTextDelay = 0.0f;
	// #region agent log
	AppendDebugLog(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H44\",\"location\":\"BlasterPlayerController.cpp:1167\",\"message\":\"PostLoad state\",\"data\":{\"blocked\":%s,\"ignoreMove\":%s,\"ignoreLook\":%s,\"world\":\"%s\",\"netMode\":%d},\"timestamp\":%lld}"),
		bEndingInputBlocked ? TEXT("true") : TEXT("false"),
		IsMoveInputIgnored() ? TEXT("true") : TEXT("false"),
		IsLookInputIgnored() ? TEXT("true") : TEXT("false"),
		LoadedWorld ? *LoadedWorld->GetMapName() : TEXT("none"),
		LoadedWorld ? static_cast<int32>(LoadedWorld->GetNetMode()) : -1,
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
}

void ABlasterPlayerController::UpdateLoadingScreenText()
{
	static const FName FuncName(TEXT("SetLoadingText"));
	if (LoadingScreenWidget)
	{
		UFunction* Func = LoadingScreenWidget->FindFunction(FuncName);
		if (!Func)
		{
			return;
		}

		struct FSetLoadingTextParams
		{
			FString LoadingText;
		};
		FSetLoadingTextParams Params;
		Params.LoadingText = LoadingText;
		LoadingScreenWidget->ProcessEvent(Func, &Params);
	}
}

void ABlasterPlayerController::ClientPlayEndingSequence_Implementation(const TSoftObjectPtr<ULevelSequence>& SequenceAsset)
{
    TSoftObjectPtr<ULevelSequence> SequencePtr = SequenceAsset;
    if (SequencePtr.IsNull())
    {
        return;
    }

    ULevelSequence* Sequence = SequencePtr.LoadSynchronous();
    if (Sequence)
    {
        BP_PlayEndingSequence(Sequence);
    }
}

void ABlasterPlayerController::ClientSetInputBlocked_Implementation(bool bBlocked)
{
	bEndingInputBlocked = bBlocked;
	// #region agent log
	AppendDebugLog(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H41\",\"location\":\"BlasterPlayerController.cpp:1207\",\"message\":\"ClientSetInputBlocked\",\"data\":{\"blocked\":%s,\"world\":\"%s\",\"netMode\":%d},\"timestamp\":%lld}"),
		bBlocked ? TEXT("true") : TEXT("false"),
		GetWorld() ? *GetWorld()->GetMapName() : TEXT("none"),
		GetWorld() ? static_cast<int32>(GetWorld()->GetNetMode()) : -1,
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion

	SetIgnoreMoveInput(bBlocked);
	SetIgnoreLookInput(bBlocked);

	if (ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn()))
	{
		BlasterCharacter->SetEndingInProgress(bBlocked);
	}
}

void ABlasterPlayerController::ClientBeginDeathSpectate_Implementation()
{
	bDeathSpectating = true;
	CurrentSpectateIndex = INDEX_NONE;
	bSpectateFadeInPending = true;

	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(0.f, 1.f, SpectateFadeDuration, FLinearColor::Black, true, true);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpectateFadeTimerHandle);
		World->GetTimerManager().SetTimer(
			SpectateFadeTimerHandle,
			this,
			&ABlasterPlayerController::HandleSpectateNext,
			SpectateFadeDuration,
			false);
	}
}

void ABlasterPlayerController::HandleSpectateNext()
{
	if (!bDeathSpectating)
	{
		return;
	}

	SwitchSpectateTarget(1);

	if (bSpectateFadeInPending && PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(1.f, 0.f, SpectateFadeDuration, FLinearColor::Black, false, false);
		bSpectateFadeInPending = false;
	}
}

void ABlasterPlayerController::SwitchSpectateTarget(int32 Direction)
{
	TArray<AActor*> Targets = GetSpectateTargets();
	if (Targets.Num() == 0)
	{
		return;
	}

	if (CurrentSpectateIndex == INDEX_NONE)
	{
		CurrentSpectateIndex = 0;
	}
	else
	{
		CurrentSpectateIndex = (CurrentSpectateIndex + Direction) % Targets.Num();
		if (CurrentSpectateIndex < 0)
		{
			CurrentSpectateIndex += Targets.Num();
		}
	}

	AActor* Target = Targets.IsValidIndex(CurrentSpectateIndex) ? Targets[CurrentSpectateIndex] : nullptr;
	if (Target)
	{
		SetViewTargetWithBlend(Target, SpectateBlendTime);
	}
}

TArray<AActor*> ABlasterPlayerController::GetSpectateTargets() const
{
	TArray<AActor*> Targets;
	if (!GetWorld())
	{
		return Targets;
	}

	const AGameStateBase* GS = GetWorld()->GetGameState();
	if (!GS)
	{
		return Targets;
	}

	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS)
		{
			continue;
		}

		APawn* SpectatePawn = PS->GetPawn();
		if (!SpectatePawn || SpectatePawn == GetPawn())
		{
			continue;
		}

		const ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(SpectatePawn);
		if (BlasterCharacter && BlasterCharacter->IsOutOfLives())
		{
			continue;
		}

		Targets.Add(SpectatePawn);
	}

	return Targets;
}