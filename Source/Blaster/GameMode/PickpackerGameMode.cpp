// Fill out your copyright notice in the Description page of Project Settings.

#include "PickpackerGameMode.h"
#include "Blaster/GameState/PickpackerGameState.h"
#include "Blaster/Escape/EscapeZoneActor.h"
#include "Engine/World.h"
#include "Blaster/DataAssets/DA_LevelVariant.h"
#include "Blaster/DataAssets/DA_OrderWaveData.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

APickpackerGameMode::APickpackerGameMode()
{
	GameStateClass = APickpackerGameState::StaticClass();
	CurrentMissionConfig = FSeedSet();
}

void APickpackerGameMode::BeginPlay()
{
	Super::BeginPlay();

	PickpackerGameState = Cast<APickpackerGameState>(GameState);

	// Subscribe to suspicion changes
	if (PickpackerGameState)
	{
		PickpackerGameState->OnSuspicionChanged.AddDynamic(this, &APickpackerGameMode::OnSuspicionChanged);
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

	BeginOrderWave(0);

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

void APickpackerGameMode::BeginOrderWave(int32 WaveIndex)
{
	if (!HasAuthority() || !OrderWaveData)
	{
		return;
	}

	const FParcelOrderWave* Wave = OrderWaveData->GetWave(0);
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

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const int32 TemplateCount = Wave->Orders.Num();
	const int32 DifficultyTier = WaveIndex % 5; // 10% increase each tier
	const float QuantityMultiplier = 1.0f + static_cast<float>(DifficultyTier) * 0.1f;
	const int32 AdditionalItems = WaveIndex / 5;
	const int32 OrdersToSpawn = FMath::Clamp(1 + AdditionalItems, 1, TemplateCount);

	for (int32 SelectionIndex = 0; SelectionIndex < OrdersToSpawn; ++SelectionIndex)
	{
		const int32 TemplateIdx = FMath::RandRange(0, TemplateCount - 1);
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
		const int32 BaseQuantity = Definition.RequiredQuantity > 0 ? Definition.RequiredQuantity : 1;
		const int32 AdjustedQuantity = FMath::Max(1, FMath::RoundToInt(FMath::CeilToFloat(static_cast<float>(BaseQuantity) * QuantityMultiplier)));
		OrderState.RequiredQuantity = AdjustedQuantity;
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

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Order wave %d started (%d orders)"), WaveIndex, Wave->Orders.Num());
}

void APickpackerGameMode::ScheduleNextOrderWave(float DelaySeconds)
{
	if (!HasAuthority() || !OrderWaveData)
	{
		return;
	}

	if (!OrderWaveData->GetWave(0))
	{
		return; // No more waves
	}

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		if (TimerManager.IsTimerActive(NextWaveTimerHandle))
		{
			return;
		}

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

	BeginOrderWave(CurrentOrderWaveIndex + 1);
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

	// 기존 구현은 "다음 인덱스 wave"가 존재할 때만 다음 웨이브를 시작했는데,
	// 데이터 자산이 0번 웨이브 하나만 가지고 있고 그 안에서 템플릿을 랜덤 선택하는 구조라면
	// 추가 웨이브가 생성되지 않는다. 따라서 웨이브 존재 여부는 0번 웨이브만 확인하고,
	// 모든 오더가 해결되면 동일 웨이브(난이도 인덱스만 증가)로 다음 라운드를 스케줄한다.
	if (OrderWaveData && AreAllOrdersResolved())
	{
		const FParcelOrderWave* NextWave = OrderWaveData->GetWave(0);
		const float Delay = NextWave ? NextWave->StartDelay : 0.0f;
		ScheduleNextOrderWave(Delay);
	}
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

	FGameplayTagContainer ParcelTags = Parcel->GetParcelTags();
	const bool bParcelPackaged = Parcel->IsPackaged();
	const FGameplayTag ParcelItemId = Parcel->GetItemData().ItemId;

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

		if (Order.RequiredParcelTag.IsValid() && !ParcelTags.HasTag(Order.RequiredParcelTag))
		{				
			continue;
		}

		Order.SubmittedQuantity = FMath::Clamp(Order.SubmittedQuantity + 1, 0, Order.RequiredQuantity);

		const int32 ParcelValue = Parcel->GetParcelPrice();
		if (ParcelValue != 0)
		{
			ApplyCreditDelta(ParcelValue, FString::Printf(TEXT("Parcel (%s) submitted"), *Order.OrderName.ToString()));
		}

		if (Order.SubmittedQuantity >= Order.RequiredQuantity)
		{
			Order.bCompleted = true;
			Order.ResolutionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			HandleOrderSuccess(Order);
		}

		SyncOrdersToGameState();
		return true;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[PickpackerGameMode::TryFulfillOrders] No matching order for parcel '%s' (Packaged=%s, ItemId=%s, Tags=%s)"),
		*Parcel->GetName(), bParcelPackaged ? TEXT("true") : TEXT("false"), *ParcelItemId.ToString(), *ParcelTags.ToStringSimple());
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

	if (Order.CreditPenalty > 0)
	{
		ApplyCreditDelta(-Order.CreditPenalty, FString::Printf(TEXT("%s failed"), *Order.OrderName.ToString()));
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
