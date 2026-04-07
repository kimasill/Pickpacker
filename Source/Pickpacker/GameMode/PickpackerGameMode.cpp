// Fill out your copyright notice in the Description page of Project Settings.

#include "PickpackerGameMode.h"
#include "GameState/PickpackerGameState.h"
#include "Components/EscapeProgressComponent.h"
#include "Escape/EscapeZoneActor.h"
#include "Engine/World.h"
#include "Engine/LevelStreaming.h"
#include "DataAssets/DA_LevelVariant.h"
#include "DataAssets/DA_OrderWaveData.h"
#include "Parcel/ParcelActor.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Character/BlasterCharacter.h"
#include "Components/PlayerInventoryComponent.h"
#include "Environment/ConveyorBeltActor.h"
#include "PlayerController/BlasterPlayerController.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/NetDriver.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"

namespace
{
	static void AppendDebugLog_PickpackerGameMode(const FString& JsonLine)
	{
		const FString LogDir = FPaths::Combine(FPaths::ProjectDir(), TEXT(".cursor"), TEXT("debug.log"));
		FFileHelper::SaveStringToFile(JsonLine + LINE_TERMINATOR, *LogDir, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
	}
}

APickpackerGameMode::APickpackerGameMode()
{
	GameStateClass = APickpackerGameState::StaticClass();
	CurrentMissionConfig = FSeedSet();
	// controlroom 스트리밍 레벨 기본 등록 (BP에서 덮어쓰기 가능)
	StreamingLevelNamesToLoadAtStart.Add(FName(TEXT("controlroom")));
}

void APickpackerGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 스트리밍 레벨(controlroom 등) 즉시 로드 - 패키징 빌드에서 텔레포트/액터 참조 실패 방지
	if (HasAuthority() && StreamingLevelNamesToLoadAtStart.Num() > 0)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
			{
				if (!StreamingLevel) continue;
				FString LevelName = StreamingLevel->GetWorldAssetPackageFName().ToString();
				FString LevelShortName = FPaths::GetBaseFilename(LevelName);
				for (const FName& ToLoad : StreamingLevelNamesToLoadAtStart)
				{
					if (LevelShortName.Equals(ToLoad.ToString(), ESearchCase::IgnoreCase) ||
						LevelName.Contains(ToLoad.ToString(), ESearchCase::IgnoreCase))
					{
						StreamingLevel->SetShouldBeLoaded(true);
						StreamingLevel->SetShouldBeVisible(true);
						UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Streaming level load requested: %s"), *LevelShortName);
						break;
					}
				}
			}
			// 패키징 빌드: InspectionActorLocations 등이 ControlRoom 내 액터 참조 시, 로드 완료까지 블록
			UGameplayStatics::FlushLevelStreaming(World);
		}
	}

	PickpackerGameState = Cast<APickpackerGameState>(GameState);

	if (PickpackerGameState)
	{
		PickpackerGameState->OnSuspicionChanged.AddDynamic(this, &APickpackerGameMode::OnSuspicionChanged);
		// 매치 시작 전에도 초기 크레딧 설정 (UI가 처음부터 StartingTeamCredits 표시)
		if (HasAuthority())
		{
			PickpackerGameState->SetTeamCredits(StartingTeamCredits);
		}
	}
}

void APickpackerGameMode::OnMatchStateSet()
{
	Super::OnMatchStateSet();
	if (MatchState == MatchState::InProgress)
	{
		HandleMatchStart();
	}
}

void APickpackerGameMode::HandleMatchStart()
{
	if (!HasAuthority()) return;
	if (bPCGGenerationInProgress) return;
	if (!PickpackerGameState) return;

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Starting match with mission: %s, seed: %d"),
		*CurrentMissionConfig.MissionId, CurrentMissionConfig.Seed);

	// Replicate seed via game state
	PickpackerGameState->SetRandomSeed(CurrentMissionConfig.Seed);

	bPCGGenerationInProgress = true;

	// If PCG subsystem exists in this project, trigger it here. Otherwise proceed immediately.
	// if (PCGDungeonSubsystem) { PCGDungeonSubsystem->GenerateDungeon(CurrentMissionConfig); }

	OnPCGGenerationComplete(true);
}

void APickpackerGameMode::SetMissionConfig(const FString& MissionId, int32 CustomSeed)
{
	if (!HasAuthority()) return;
	CurrentMissionConfig.MissionId = MissionId;
	CurrentMissionConfig.Seed = CustomSeed > 0 ? CustomSeed : FMath::RandRange(1, 999999);

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Mission config set - MissionId: %s, Seed: %d"),
		*CurrentMissionConfig.MissionId, CurrentMissionConfig.Seed);
}

void APickpackerGameMode::OnPCGGenerationComplete(bool bSuccess)
{
	bPCGGenerationInProgress = false;
	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] PCG generation successful - starting gameplay"));
		StartGameplay();
	}
}

void APickpackerGameMode::StartGameplay()
{
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Gameplay started"));

	// Start checking game end conditions periodically
	if (HasAuthority())
	{
		if (PickpackerGameState)
		{
			PickpackerGameState->SetTeamCredits(StartingTeamCredits);
		}

		GetWorld()->GetTimerManager().SetTimer(
			GameEndCheckTimer,
			this,
			&APickpackerGameMode::CheckGameEndConditions,
			1.0f,
			true
		);

		if (bAutoStartOrders)
		{
			StartOrderSystem();
		}
	}
}

void APickpackerGameMode::OnPlayerEscaped(ACharacter* Player)
{
	if (!HasAuthority() || !Player)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Player escaped: %s"), *Player->GetName());

	// Check if all players escaped
	CheckGameEndConditions();
}

void APickpackerGameMode::OnAllPlayersEscaped()
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] All players escaped - VICTORY!"));

	// End game with victory
	OnGameOver(TEXT("All players escaped!"));
}

void APickpackerGameMode::OnGameOver(const FString& Reason)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bGameOverInProgress)
	{
		return;
	}
	bGameOverInProgress = true;

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Game Over: %s"), *Reason);

	// Stop game end check timer
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(GameEndCheckTimer);
		GetWorld()->GetTimerManager().ClearTimer(OrderSystemTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(NextWaveTimerHandle);
	}

	// End simulation
	EndWarehouseSimulation();

	// Broadcast game over event (can be handled by UI)
	OnGameOverEvent.Broadcast(Reason);

	// 전원 사망 처리: 컨베이어 이동/사망 연출 트리거
	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				if (ABlasterCharacter* Character = Cast<ABlasterCharacter>(PC->GetPawn()))
				{
					Character->HandleOutOfLives();
				}
			}
		}
	}

	BP_PlayGameOverSequence(Reason);

	if (bReturnToLobbyOnGameOver && !bDeferLobbyReturnUntilSubmissionZone && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			ReturnToLobbyTimerHandle,
			this,
			&APickpackerGameMode::ReturnPlayersToLobby,
			FMath::Max(0.1f, GameOverReturnDelay),
			false
		);
	}
}

void APickpackerGameMode::RequestReturnToLobby()
{
	if (!bGameOverInProgress)
	{
		// #region agent log
		AppendDebugLog_PickpackerGameMode(FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H16\",\"location\":\"PickpackerGameMode.cpp:210\",\"message\":\"RequestReturnToLobby skipped\",\"data\":{\"reason\":\"gameOverFalse\",\"timerActive\":%s},\"timestamp\":%lld}"),
			(GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(ReturnToLobbyTimerHandle)) ? TEXT("true") : TEXT("false"),
			FDateTime::UtcNow().ToUnixTimestamp() * 1000));
		// #endregion
		return;
	}
	if (bReturnToLobbyTriggered)
	{
		// #region agent log
		AppendDebugLog_PickpackerGameMode(FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H17\",\"location\":\"PickpackerGameMode.cpp:215\",\"message\":\"RequestReturnToLobby skipped\",\"data\":{\"reason\":\"alreadyTriggered\"},\"timestamp\":%lld}"),
			FDateTime::UtcNow().ToUnixTimestamp() * 1000));
		// #endregion
		return;
	}
	if (GetWorld() && !GetWorld()->GetTimerManager().IsTimerActive(ReturnToLobbyTimerHandle))
	{
		// #region agent log
		AppendDebugLog_PickpackerGameMode(FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H1\",\"location\":\"PickpackerGameMode.cpp:206\",\"message\":\"RequestReturnToLobby\",\"data\":{\"gameOver\":%s,\"world\":\"%s\"},\"timestamp\":%lld}"),
			bGameOverInProgress ? TEXT("true") : TEXT("false"),
			GetWorld() ? *GetWorld()->GetMapName() : TEXT("none"),
			FDateTime::UtcNow().ToUnixTimestamp() * 1000));
		// #endregion
		ReturnPlayersToLobby();
	}
	else
	{
		// #region agent log
		AppendDebugLog_PickpackerGameMode(FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H16\",\"location\":\"PickpackerGameMode.cpp:219\",\"message\":\"RequestReturnToLobby skipped\",\"data\":{\"reason\":\"timerActive\",\"timerActive\":%s},\"timestamp\":%lld}"),
			(GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(ReturnToLobbyTimerHandle)) ? TEXT("true") : TEXT("false"),
			FDateTime::UtcNow().ToUnixTimestamp() * 1000));
		// #endregion
	}
}

void APickpackerGameMode::ReturnPlayersToLobby()
{
	if (!HasAuthority() || LobbyTravelPath.IsEmpty())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (bReturnToLobbyTriggered)
	{
		// #region agent log
		AppendDebugLog_PickpackerGameMode(FString::Printf(
			TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H17\",\"location\":\"PickpackerGameMode.cpp:238\",\"message\":\"ReturnPlayersToLobby skipped\",\"data\":{\"reason\":\"alreadyTriggered\"},\"timestamp\":%lld}"),
			FDateTime::UtcNow().ToUnixTimestamp() * 1000));
		// #endregion
		return;
	}
	bReturnToLobbyTriggered = true;
	// #region agent log
	AppendDebugLog_PickpackerGameMode(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H17\",\"location\":\"PickpackerGameMode.cpp:244\",\"message\":\"ReturnPlayersToLobby first\",\"data\":{\"setTriggered\":true},\"timestamp\":%lld}"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion

	// #region agent log
	int32 ClientConnCount = 0;
	if (UNetDriver* NetDriver = World->GetNetDriver())
	{
		ClientConnCount = NetDriver->ClientConnections.Num();
	}
	AppendDebugLog_PickpackerGameMode(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H14\",\"location\":\"PickpackerGameMode.cpp:232\",\"message\":\"NetDriver clients\",\"data\":{\"clientConnections\":%d},\"timestamp\":%lld}"),
		ClientConnCount,
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion

	// #region agent log
	AppendDebugLog_PickpackerGameMode(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H15\",\"location\":\"PickpackerGameMode.cpp:241\",\"message\":\"Seamless before set\",\"data\":{\"bUseSeamless\":%s},\"timestamp\":%lld}"),
		bUseSeamlessTravel ? TEXT("true") : TEXT("false"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion

	bUseSeamlessTravel = true;

	// #region agent log
	AppendDebugLog_PickpackerGameMode(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H15\",\"location\":\"PickpackerGameMode.cpp:247\",\"message\":\"Seamless after set\",\"data\":{\"bUseSeamless\":%s},\"timestamp\":%lld}"),
		bUseSeamlessTravel ? TEXT("true") : TEXT("false"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion

	// #region agent log
	const FString TravelPath = FString::Printf(TEXT("%s?listen"), *LobbyTravelPath);
	// #region agent log
	AppendDebugLog_PickpackerGameMode(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H9\",\"location\":\"PickpackerGameMode.cpp:240\",\"message\":\"ServerTravel to lobby\",\"data\":{\"url\":\"%s\",\"world\":\"%s\"},\"timestamp\":%lld}"),
		*TravelPath,
		World ? *World->GetMapName() : TEXT("none"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
	// #region agent log
	AppendDebugLog_PickpackerGameMode(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H21\",\"location\":\"PickpackerGameMode.cpp:244\",\"message\":\"SeamlessTravel requested\",\"data\":{\"useSeamless\":%s},\"timestamp\":%lld}"),
		bUseSeamlessTravel ? TEXT("true") : TEXT("false"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
	bUseSeamlessTravel = false;

	// #region agent log
	AppendDebugLog_PickpackerGameMode(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H15\",\"location\":\"PickpackerGameMode.cpp:247\",\"message\":\"Seamless after set\",\"data\":{\"bUseSeamless\":%s},\"timestamp\":%lld}"),
		bUseSeamlessTravel ? TEXT("true") : TEXT("false"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
	// #region agent log
	AppendDebugLog_PickpackerGameMode(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H9\",\"location\":\"PickpackerGameMode.cpp:240\",\"message\":\"ServerTravel to lobby\",\"data\":{\"url\":\"%s\",\"world\":\"%s\"},\"timestamp\":%lld}"),
		*TravelPath,
		World ? *World->GetMapName() : TEXT("none"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
	// #region agent log
	AppendDebugLog_PickpackerGameMode(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H21\",\"location\":\"PickpackerGameMode.cpp:244\",\"message\":\"SeamlessTravel requested\",\"data\":{\"useSeamless\":%s},\"timestamp\":%lld}"),
		bUseSeamlessTravel ? TEXT("true") : TEXT("false"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
	bUseSeamlessTravel = true;
	World->ServerTravel(TravelPath);
	// #region agent log
	AppendDebugLog_PickpackerGameMode(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H18\",\"location\":\"PickpackerGameMode.cpp:247\",\"message\":\"ServerTravel issued\",\"data\":{\"inSeamless\":%s},\"timestamp\":%lld}"),
		World->IsInSeamlessTravel() ? TEXT("true") : TEXT("false"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion
}

void APickpackerGameMode::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();

	UWorld* World = GetWorld();
	// #region agent log
	AppendDebugLog_PickpackerGameMode(FString::Printf(
		TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H12\",\"location\":\"PickpackerGameMode.cpp:272\",\"message\":\"PostSeamlessTravel\",\"data\":{\"world\":\"%s\",\"numPC\":%d},\"timestamp\":%lld}"),
		World ? *World->GetMapName() : TEXT("none"),
		World ? World->GetNumPlayerControllers() : -1,
		FDateTime::UtcNow().ToUnixTimestamp() * 1000));
	// #endregion

	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(It->Get()))
		{
			// #region agent log
			AppendDebugLog_PickpackerGameMode(FString::Printf(
				TEXT("{\"sessionId\":\"debug-session\",\"runId\":\"pre-fix\",\"hypothesisId\":\"H26\",\"location\":\"PickpackerGameMode.cpp:283\",\"message\":\"PostSeamlessTravel PC\",\"data\":{\"pc\":\"%s\",\"netConn\":%s,\"world\":\"%s\"},\"timestamp\":%lld}"),
				*GetNameSafe(PC),
				PC->GetNetConnection() ? TEXT("true") : TEXT("false"),
				PC->GetWorld() ? *PC->GetWorld()->GetMapName() : TEXT("none"),
				FDateTime::UtcNow().ToUnixTimestamp() * 1000));
			// #endregion
			// ClientEnsureLobbyTravel 제거: 로비->게임 레벨 도착 시 클라이언트를 다시 로비로 보내던 버그 수정
			PC->ClientNotifyLevelLoaded();
		}
	}
}

void APickpackerGameMode::CheckGameEndConditions()
{
	if (!HasAuthority() || !PickpackerGameState)
	{
		return;
	}

	// Check if suspicion reached maximum
	float SuspicionLevel = PickpackerGameState->GetSuspicionLevel();
	if (SuspicionLevel >= 1.0f)
	{
		OnGameOver(TEXT("Suspicion reached maximum!"));
		return;
	}

	// 모든 플레이어 사망 실패 엔딩
	if (UEscapeProgressComponent* Progress = PickpackerGameState->GetEscapeProgressComponent())
	{
		int32 AlivePlayers = 0;
		for (APlayerState* PS : PickpackerGameState->PlayerArray)
		{
			if (PS && PS->GetPawn())
			{
				++AlivePlayers;
			}
		}

		if (AlivePlayers == 0 && Progress->GetCurrentEndingId().IsNone())
		{
			Progress->StartEndingById(TEXT("Ending_Fail_AllDead"));
			return;
		}
	}

	// Check if all players escaped (handled by EscapeZoneActor)
	// This is checked when players escape, not here
}

void APickpackerGameMode::OnSuspicionChanged(float NewSuspicionLevel)
{
	if (!HasAuthority())
	{
		return;
	}

	// Check if suspicion reached maximum
	if (NewSuspicionLevel >= 1.0f)
	{
		OnGameOver(TEXT("Suspicion reached maximum!"));
	}
}
// ===== Implementations required by header (to fix LNK2019) =====
void APickpackerGameMode::InitializeLevel(UDA_LevelVariant* LevelVariantData, int32 Seed)
{
	if (!HasAuthority()) return;
	CurrentLevelVariant = LevelVariantData;
	CurrentSeed = (Seed > 0) ? Seed : FMath::RandRange(1, 999999);

	if (APickpackerGameState* GS = Cast<APickpackerGameState>(GameState))
	{
		GS->SetLevelVariant(LevelVariantData);
		GS->SetRandomSeed(CurrentSeed);
	}

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] InitializeLevel done. Seed=%d, Variant=%s"), CurrentSeed, LevelVariantData ? *LevelVariantData->GetName() : TEXT("None"));
}

void APickpackerGameMode::StartWarehouseSimulation()
{
	if (!HasAuthority()) return;
	if (APickpackerGameState* GS = Cast<APickpackerGameState>(GameState))
	{
		GS->SetSimulationRunning(true);
	}
	bSimulationRunning = true;
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Simulation started"));
}

void APickpackerGameMode::EndWarehouseSimulation()
{
	if (!HasAuthority()) return;
	if (APickpackerGameState* GS = Cast<APickpackerGameState>(GameState))
	{
		GS->SetSimulationRunning(false);
	}
	bSimulationRunning = false;
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Simulation ended"));

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(OrderSystemTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(NextWaveTimerHandle);
	}
}

void APickpackerGameMode::StopOrderWaves()
{
	if (!HasAuthority()) return;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(OrderSystemTimerHandle);
		World->GetTimerManager().ClearTimer(NextWaveTimerHandle);
	}

	ActiveOrders.Empty();
	CurrentOrderWaveIndex = INDEX_NONE;
	CurrentWaveRepeatIndex = 0;
	PendingWaveIndex = INDEX_NONE;
	PendingWaveRepeatIndex = 0;

	SyncOrdersToGameState();

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Order waves stopped for escape sequence"));
}

void APickpackerGameMode::RegisterClientPCGReady(APlayerState* PlayerState)
{
	const TCHAR* NameOrNone = TEXT("None");
	if (PlayerState)
	{
		const FString& Nm = PlayerState->GetPlayerName();
		NameOrNone = *Nm;
	}
	UE_LOG(LogTemp, Verbose, TEXT("[PickpackerGameMode] Client PCG ready reported: %s"), NameOrNone);
}

void APickpackerGameMode::ReportParcelSubmitted(AParcelActor* Parcel)
{
	if (!HasAuthority() || !Parcel)
	{
		return;
	}

	// 파슬 파괴 전 정리: 인벤토리 제거 + 컨베이어에서 해제 (ESC 메뉴 등에서 dangling pointer 참조로 크래시 방지)
	if (UWorld* World = GetWorld())
	{
		// 1. 모든 플레이어 인벤토리에서 제거
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (ABlasterCharacter* Character = Cast<ABlasterCharacter>(It->Get()->GetPawn()))
			{
				if (UPlayerInventoryComponent* Inv = Character->GetPlayerInventoryComponent())
				{
					Inv->RemoveItem(Parcel);
				}
			}
		}
		// 2. 컨베이어에서 submission으로 바로 제출되는 경우: 컨베이어 목록에서 해제
		for (TActorIterator<AConveyorBeltActor> ConvIt(World); ConvIt; ++ConvIt)
		{
			ConvIt->ReleaseParcelIfConveyed(Parcel);
		}
	}

	const bool bAccepted = TryFulfillOrders(Parcel);
	if (!bAccepted)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameMode] Parcel submission rejected - no matching order"));

		// Apply a small suspicion penalty for incorrect submissions
		FActiveOrderState DummyPenalty;
		DummyPenalty.SuspicionPenalty = 1.0f;
		ApplyOrderPenalty(DummyPenalty);
		ApplyCreditDelta(-1, TEXT("Incorrect parcel submission"));
	}

	// Blueprint에서 사운드/이펙트 등 처리 (파슬 파괴 전)
	OnParcelSubmitted.Broadcast(Parcel, bAccepted);

	if (Parcel->IsPendingKillPending() == false)
	{
		Parcel->Destroy();
	}
}

void APickpackerGameMode::ApplyCreditDelta(int32 Delta, const FString& Reason)
{
	if (!HasAuthority() || Delta == 0)
	{
		return;
	}

	if (!PickpackerGameState)
	{
		return;
	}

	const int32 PreviousCredits = PickpackerGameState->GetTeamCredits();
	PickpackerGameState->ApplyCreditDelta(Delta, Reason);
	const int32 CurrentCredits = PickpackerGameState->GetTeamCredits();

	if (CurrentCredits <= 0 && PreviousCredits > 0)
	{
		OnGameOver(TEXT("Team credits depleted"));
	}
}

void APickpackerGameMode::StartOrderSystem()
{
	if (!HasAuthority() || bOrderSystemInitialized)
	{
		return;
	}

	if (!OrderWaveData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameMode] Cannot start order system - OrderWaveData not assigned"));
		return;
	}

	bOrderSystemInitialized = true;
	ActiveOrders.Reset();
	CurrentOrderWaveIndex = INDEX_NONE;
	CurrentWaveRepeatIndex = 0;
	PendingWaveIndex = INDEX_NONE;
	PendingWaveRepeatIndex = 0;

	if (CanSpawnNewOrders())
	{
		const FParcelOrderWave* FirstWave = OrderWaveData->GetWave(0);
		if (FirstWave)
		{
			const float InitialDelay = FMath::Max(0.0f, InitialOrderStartDelay);
			ScheduleNextOrderWave(0, 0, InitialDelay);
		}
	}

	if (UWorld* World = GetWorld())
	{
		const float Interval = FMath::Max(0.25f, OrderUpdateInterval);
		World->GetTimerManager().SetTimer(
			OrderSystemTimerHandle,
			this,
			&APickpackerGameMode::TickOrderSystem,
			Interval,
			true
		);
	}
}

void APickpackerGameMode::BeginOrderWave(int32 WaveIndex, int32 RepeatIndex)
{
	if (!HasAuthority() || !OrderWaveData)
	{
		return;
	}

	const FParcelOrderWave* Wave = OrderWaveData->GetWave(WaveIndex);
	if (!Wave)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameMode] Attempted to start invalid order wave index %d"), WaveIndex);
		return;
	}

	if (Wave->Orders.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameMode] Order wave %d has no templates"), WaveIndex);
		return;
	}

	CurrentOrderWaveIndex = WaveIndex;
	CurrentWaveRepeatIndex = RepeatIndex;

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const int32 TemplateCount = Wave->Orders.Num();
	const int32 OrdersToSpawn = Wave->OrdersPerWave <= 0
		? TemplateCount
		: FMath::Clamp(Wave->OrdersPerWave, 1, TemplateCount);

	TArray<int32> TemplateIndices;
	TemplateIndices.Reserve(TemplateCount);
	for (int32 Index = 0; Index < TemplateCount; ++Index)
	{
		TemplateIndices.Add(Index);
	}
	for (int32 Index = TemplateIndices.Num() - 1; Index > 0; --Index)
	{
		const int32 SwapIndex = FMath::RandRange(0, Index);
		TemplateIndices.Swap(Index, SwapIndex);
	}

	for (int32 SelectionIndex = 0; SelectionIndex < OrdersToSpawn; ++SelectionIndex)
	{
		const int32 TemplateIdx = TemplateIndices.IsValidIndex(SelectionIndex)
			? TemplateIndices[SelectionIndex]
			: FMath::RandRange(0, TemplateCount - 1);
		const FParcelOrderDefinition& Definition = Wave->Orders[TemplateIdx];

		FActiveOrderState OrderState;
		OrderState.OrderId = FGuid::NewGuid();
		OrderState.OrderName = Definition.OrderName;
		if (OrdersToSpawn > 1)
		{
			const FString NameOverride = FString::Printf(TEXT("%s_%d"), *Definition.OrderName.ToString(), SelectionIndex + 1);
			OrderState.OrderName = FName(*NameOverride);
		}
		OrderState.OrderDescription = Definition.OrderDescription;
		OrderState.RequiredParcelTag = Definition.RequiredParcelTag;
		OrderState.RequiredItemTag = Definition.RequiredItemTag;
		OrderState.RequiredQuantity = FMath::Max(1, Definition.RequiredQuantity);
		OrderState.SubmittedQuantity = 0;
		OrderState.bRequirePackaged = Definition.bRequirePackaged;
		OrderState.ExpireTime = Definition.TimeLimitSeconds > 0.f ? Now + Definition.TimeLimitSeconds : -1.f;
		OrderState.CreditReward = Definition.CreditReward;
		OrderState.CreditPenalty = Definition.CreditPenalty;
		OrderState.SuspicionPenalty = Definition.SuspicionPenalty;
		OrderState.ResolutionTime = -1.0f;

		ActiveOrders.Add(OrderState);
	}

	SyncOrdersToGameState();

	if (CurrentWaveRepeatIndex == 0)
	{
		if (PickpackerGameState)
		{
			PickpackerGameState->SetCurrentOrderWaveNumber(WaveIndex + 1);
		}
		BP_OnOrderWaveStarted(WaveIndex + 1);
	}

	// 주문 생성 시점 기준으로 다음 웨이브 스케줄 (주문 종료와 무관)
	const int32 NextRepeatIndex = RepeatIndex + 1;
	const float DelaySeconds = FMath::Max(0.0f,
		(NextRepeatIndex < FMath::Max(1, Wave->RepeatCount))
			? Wave->RepeatDelay
			: Wave->NextWaveDelay);
	const int32 NextWaveIdx = (NextRepeatIndex < FMath::Max(1, Wave->RepeatCount))
		? WaveIndex
		: WaveIndex + 1;
	const int32 NextRepIdx = (NextRepeatIndex < FMath::Max(1, Wave->RepeatCount))
		? NextRepeatIndex
		: 0;
	if (OrderWaveData->GetWave(NextWaveIdx))
	{
		ScheduleNextOrderWave(NextWaveIdx, NextRepIdx, DelaySeconds);
	}

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Order wave %d started (%d orders), next wave in %.1fs"), WaveIndex, OrdersToSpawn, DelaySeconds);
}

void APickpackerGameMode::ScheduleNextOrderWave(int32 WaveIndex, int32 RepeatIndex, float DelaySeconds)
{
	if (!HasAuthority() || !OrderWaveData)
	{
		return;
	}

	if (!OrderWaveData->GetWave(WaveIndex))
	{
		return; // No more waves
	}

	PendingWaveIndex = WaveIndex;
	PendingWaveRepeatIndex = RepeatIndex;

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(NextWaveTimerHandle);

		if (DelaySeconds <= 0.f)
		{
			HandleNextOrderWaveTimer();
		}
		else
		{
			TimerManager.SetTimer(
				NextWaveTimerHandle,
				this,
				&APickpackerGameMode::HandleNextOrderWaveTimer,
				DelaySeconds,
				false
			);
		}
	}
}

void APickpackerGameMode::HandleNextOrderWaveTimer()
{
	if (!HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NextWaveTimerHandle);
	}

	// 스케줄된 웨이브 실행 (repeat은 의도적으로 예약된 것이므로 CanSpawnNewOrders 무시)
	if (PendingWaveIndex != INDEX_NONE)
	{
		BeginOrderWave(PendingWaveIndex, PendingWaveRepeatIndex);
	}
}

void APickpackerGameMode::TickOrderSystem()
{
	if (!HasAuthority() || !bOrderSystemInitialized || !GetWorld())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	bool bOrdersChanged = false;

	for (FActiveOrderState& Order : ActiveOrders)
	{
		if (!Order.bCompleted && !Order.bFailed && Order.ExpireTime > 0.f && Now >= Order.ExpireTime)
		{
			Order.bFailed = true;
			Order.ResolutionTime = Now;
			HandleOrderFailure(Order, TEXT("Expired"));
			bOrdersChanged = true;
		}
	}

	if (bOrdersChanged)
	{
		SyncOrdersToGameState();
	}

	CleanupResolvedOrders();
	// 다음 웨이브 스케줄링은 BeginOrderWave에서 주문 생성 시점 기준으로 처리됨 (주문 종료와 무관)
}

void APickpackerGameMode::CleanupResolvedOrders()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const float HoldTime = FMath::Max(0.0f, OrderResolutionHoldTime);

	const int32 Removed = ActiveOrders.RemoveAll([HoldTime, Now](const FActiveOrderState& Order)
	{
		if (!Order.bCompleted && !Order.bFailed)
		{
			return false;
		}

		if (HoldTime <= 0.f)
		{
			return true;
		}

		return Order.ResolutionTime > 0.f && (Now - Order.ResolutionTime) >= HoldTime;
	});

	if (Removed > 0)
	{
		SyncOrdersToGameState();
	}
}

bool APickpackerGameMode::AreAllOrdersResolved() const
{
	if (ActiveOrders.Num() == 0)
	{
		return true;
	}

	for (const FActiveOrderState& Order : ActiveOrders)
	{
		if (!Order.bCompleted && !Order.bFailed)
		{
			return false;
		}
	}
	return true;
}

bool APickpackerGameMode::TryFulfillOrders(AParcelActor* Parcel)
{
	if (!HasAuthority() || !Parcel)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[PickpackerGameMode::TryFulfillOrders] Early exit: HasAuthority=%s, Parcel=%s"),
			HasAuthority() ? TEXT("true") : TEXT("false"), Parcel ? *Parcel->GetName() : TEXT("nullptr"));
		return false;
	}

	const bool bParcelPackaged = Parcel->IsPackaged();
	const int32 ContentUnits = Parcel->GetContentUnitTotal();
	const int32 ContentValue = Parcel->GetContentValueTotal();

	if (ContentUnits <= 0)
	{
		return false;
	}

	// 태그별 이미 적용한 unit 수 (동일 태그를 요구하는 여러 주문에 분배 시 중복 적용 방지)
	TMap<FGameplayTag, int32> ConsumedUnitsByTag;
	bool bAppliedToAnyOrder = false;

	for (FActiveOrderState& Order : ActiveOrders)
	{
		if (Order.bCompleted || Order.bFailed)
		{
			continue;
		}

		if (Order.bRequirePackaged && !bParcelPackaged)
		{
			continue;
		}

		// RequiredParcelTag와 일치하는 콘텐츠 unit만 사용 (포장 시 contents 기준)
		const FGameplayTag TagToMatch = Order.RequiredParcelTag;
		const int32 TotalMatchingUnits = TagToMatch.IsValid()
			? Parcel->GetContentUnitsForTag(TagToMatch)
			: Parcel->GetContentUnitTotal();
		const int32 AlreadyConsumed = TagToMatch.IsValid() ? ConsumedUnitsByTag.FindOrAdd(TagToMatch, 0) : 0;
		const int32 AvailableUnits = FMath::Max(0, TotalMatchingUnits - AlreadyConsumed);

		if (AvailableUnits <= 0)
		{
			continue;
		}

		const int32 RemainingNeed = Order.RequiredQuantity - Order.SubmittedQuantity;
		const int32 UnitsToApply = FMath::Min(RemainingNeed, AvailableUnits);
		if (UnitsToApply <= 0)
		{
			continue;
		}

		Order.SubmittedQuantity += UnitsToApply;
		if (TagToMatch.IsValid())
		{
			ConsumedUnitsByTag.FindOrAdd(TagToMatch, 0) += UnitsToApply;
		}
		bAppliedToAnyOrder = true;

		// 크레딧: 매칭된 콘텐츠 가치에 비례
		int32 CreditsToGive = 0;
		if (TagToMatch.IsValid())
		{
			const int32 MatchingValue = Parcel->GetContentValueForTag(TagToMatch);
			const int32 MatchingUnits = Parcel->GetContentUnitsForTag(TagToMatch);
			if (MatchingUnits > 0 && MatchingValue != 0)
			{
				CreditsToGive = (MatchingValue * UnitsToApply) / MatchingUnits;
			}
		}
		else if (ContentValue != 0 && ContentUnits > 0)
		{
			CreditsToGive = (ContentValue * UnitsToApply) / ContentUnits;
		}
		if (CreditsToGive != 0)
		{
			ApplyCreditDelta(CreditsToGive, FString::Printf(TEXT("Parcel (%s) submitted"), *Order.OrderName.ToString()));
		}

		if (Order.SubmittedQuantity >= Order.RequiredQuantity)
		{
			Order.bCompleted = true;
			Order.ResolutionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			HandleOrderSuccess(Order);
		}
	}

	if (bAppliedToAnyOrder)
	{
		SyncOrdersToGameState();
		return true;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[PickpackerGameMode::TryFulfillOrders] No matching order for parcel '%s' (Packaged=%s)"),
		*Parcel->GetName(), bParcelPackaged ? TEXT("true") : TEXT("false"));
	return false;
}

void APickpackerGameMode::HandleOrderFailure(FActiveOrderState& Order, const FString& Reason)
{
	if (Order.ResolutionTime < 0.f && GetWorld())
	{
		Order.ResolutionTime = GetWorld()->GetTimeSeconds();
	}
	UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameMode] Order failed (%s) - %s"), *Reason, *Order.OrderName.ToString());
	ApplyOrderPenalty(Order);

	// 요구수량 미달분 × CreditPenalty 만큼 차감
	if (Order.CreditPenalty > 0)
	{
		const int32 Shortfall = FMath::Max(0, Order.RequiredQuantity - Order.SubmittedQuantity);
		const int32 PenaltyAmount = Shortfall * Order.CreditPenalty;
		if (PenaltyAmount > 0)
		{
			ApplyCreditDelta(-PenaltyAmount, FString::Printf(TEXT("%s failed"), *Order.OrderName.ToString()));
		}
	}
}

void APickpackerGameMode::HandleOrderSuccess(FActiveOrderState& Order)
{
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Order completed - %s (Reward: %d)"), *Order.OrderName.ToString(), Order.CreditReward);

	if (Order.CreditReward != 0)
	{
		ApplyCreditDelta(Order.CreditReward, FString::Printf(TEXT("%s completed"), *Order.OrderName.ToString()));
	}
}

void APickpackerGameMode::SyncOrdersToGameState()
{
	if (PickpackerGameState)
	{
		PickpackerGameState->SetActiveOrders(ActiveOrders);
	}
}

void APickpackerGameMode::ApplyOrderPenalty(const FActiveOrderState& Order) const
{
	if (PickpackerGameState && Order.SuspicionPenalty > 0.f)
	{
		PickpackerGameState->AddTeamSuspicion(Order.SuspicionPenalty);
	}
}

bool APickpackerGameMode::CanSpawnNewOrders() const
{
	if (MaxActiveOrders <= 0)
	{
		return true;
	}
	return ActiveOrders.Num() < MaxActiveOrders;
}
