// Fill out your copyright notice in the Description page of Project Settings.

#include "Blaster/PlayerController/PickpackerPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Blaster/GameState/PickpackerGameState.h"
#include "Engine/NetConnection.h"
#include "Blaster/GameMode/PickpackerGameMode.h"

namespace
{
	static const TCHAR* NetModeToString(ENetMode NetMode)
	{
		switch (NetMode)
		{
		case NM_Standalone:       return TEXT("Standalone");
		case NM_DedicatedServer:  return TEXT("DedicatedServer");
		case NM_ListenServer:     return TEXT("ListenServer");
		case NM_Client:           return TEXT("Client");
		default:                  return TEXT("Unknown");
		}
	}

	static const TCHAR* RoleToString(ENetRole Role)
	{
		switch (Role)
		{
		case ROLE_None:             return TEXT("None");
		case ROLE_SimulatedProxy:   return TEXT("SimulatedProxy");
		case ROLE_AutonomousProxy:  return TEXT("AutonomousProxy");
		case ROLE_Authority:        return TEXT("Authority");
		default:                    return TEXT("Unknown");
		}
	}

	static FString GetPlayerDisplayName(const AController* Controller)
	{
		const APlayerState* PS = Controller ? Controller->GetPlayerState<APlayerState>() : nullptr;
		return PS ? PS->GetPlayerName() : TEXT("Unknown");
	}

	static FString GetUniqueIdString(const AController* Controller)
	{
		const APlayerState* PS = Controller ? Controller->GetPlayerState<APlayerState>() : nullptr;
		if (!PS)
		{
			return TEXT("Invalid");
		}
		const FUniqueNetIdRepl& Id = PS->GetUniqueId();
		return Id.IsValid() ? Id->ToString() : TEXT("Invalid");
	}

	static FString BuildNetLogContext(const AController* Controller)
	{
		const UWorld* World = Controller ? Controller->GetWorld() : nullptr;
		const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;

		const AActor* AsActor = Cast<const AActor>(Controller);
		const ENetRole LocalRole = AsActor ? AsActor->GetLocalRole() : ROLE_None;
		const bool bHasAuth = AsActor ? AsActor->HasAuthority() : false;

		const bool bIsLocalController = (Cast<const APlayerController>(Controller) != nullptr) ? CastChecked<const APlayerController>(Controller)->IsLocalController() : false;

		int32 PlayerId = -1;
		if (const APlayerState* PS = Controller ? Controller->GetPlayerState<APlayerState>() : nullptr)
		{
			PlayerId = PS->GetPlayerId();
		}

		FString ConnStr = TEXT("None");
#if WITH_SERVER_CODE
		if (const APlayerController* PC = Cast<const APlayerController>(Controller))
		{
			if (PC->NetConnection)
			{
				ConnStr = PC->NetConnection->LowLevelGetRemoteAddress(true);
			}
		}
#endif

		const FString WorldName = World ? World->GetName() : TEXT("NoWorld");
		const FString PlayerName = GetPlayerDisplayName(Controller);
		const FString UniqueIdStr = GetUniqueIdString(Controller);

		return FString::Printf(
			TEXT("World=%s | NetMode=%s | HasAuth=%s | LocalRole=%s | IsLocalController=%s | PlayerName=%s | PlayerId=%d | UniqueId=%s | Remote=%s"),
			*WorldName,
			NetModeToString(NetMode),
			bHasAuth ? TEXT("true") : TEXT("false"),
			RoleToString(LocalRole),
			bIsLocalController ? TEXT("true") : TEXT("false"),
			*PlayerName,
			PlayerId,
			*UniqueIdStr,
			*ConnStr
		);
	}
}

APickpackerPlayerController::APickpackerPlayerController()
{
	bClientPCGReady = false;
	bHasReportedPCGReady = false;
}

void APickpackerPlayerController::ServerReportPCGReady_Implementation()
{
	const UWorld* World = GetWorld();
	const ENetMode NM = World ? World->GetNetMode() : NM_Standalone;

	UE_LOG(LogTemp, Warning,
		TEXT("[PC %s] Addr=%p  NetMode=%d(0 SA,1 DS,2 LS,3 CL)  Role=%d  HasAuth=%d  IsLocalPC=%d"),
		*GetNameSafe(this), this,
		(int32)NM,
		(int32)GetLocalRole(),
		HasAuthority() ? 1 : 0,
		IsLocalController() ? 1 : 0
	);

	// This should execute on the server only
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerPlayerController] ServerReportPCGReady_Implementation invoked on non-authority. Ignoring. | Ctx | %s"), *BuildNetLogContext(this));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[PickpackerPlayerController] ServerReportPCGReady_Implementation (SERVER) | Ctx | %s"), *BuildNetLogContext(this));

	if (bHasReportedPCGReady)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[PickpackerPlayerController] PCG ready already reported (server)"));
		return;
	}

	bClientPCGReady = true;
	bHasReportedPCGReady = true;

	UWorld* WorldNonConst = GetWorld();
	if (!WorldNonConst)
	{
		return;
	}

	APlayerState* PS = GetPlayerState<APlayerState>();

	// Use GameMode server tracker (doesn't depend on GS authority timing)
	if (APickpackerGameMode* GM = WorldNonConst->GetAuthGameMode<APickpackerGameMode>())
	{
		// Stub: removed RegisterClientPCGReady call if not implemented
		UE_LOG(LogTemp, Verbose, TEXT("[PickpackerPlayerController] Would notify GameMode of PCG readiness (stub)"));
	}

	// Also notify GameState for compatibility
	if (APickpackerGameState* GS = WorldNonConst->GetGameState<APickpackerGameState>())
	{
		// Stub: removed HandleClientPCGReadyFor call if not implemented
		UE_LOG(LogTemp, Verbose, TEXT("[PickpackerPlayerController] Would notify GameState of PCG readiness (stub)"));
	}
}

void APickpackerPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APickpackerPlayerController, bClientPCGReady);
}

void APickpackerPlayerController::SetClientPCGReady(bool bReady)
{
	bClientPCGReady = bReady;
	
	UE_LOG(LogTemp, Log, TEXT("[PickpackerPlayerController] SetClientPCGReady(%s)"),
		bClientPCGReady ? TEXT("true") : TEXT("false"));

	if (bClientPCGReady && !bHasReportedPCGReady)
	{
		ServerReportPCGReady();
	}
}

void APickpackerPlayerController::OnClientPCGReady()
{
	UE_LOG(LogTemp, Log, TEXT("[PickpackerPlayerController] Client PCG ready"));
}