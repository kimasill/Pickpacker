#pragma once

#include "CoreMinimal.h"

namespace PickpackerAssetPaths
{
	namespace Blueprints
	{
		inline constexpr const TCHAR* InputMainGameMappingContext = TEXT("/Game/Pickpacker/Core/Input/IMC_MainGameInputs.IMC_MainGameInputs");
		inline constexpr const TCHAR* InputOpenPauseMenu = TEXT("/Game/Pickpacker/Core/Input/IA_OpenPauseMenu.IA_OpenPauseMenu");
		inline constexpr const TCHAR* InputOpenLobbyMenu = TEXT("/Game/Pickpacker/Core/Input/IA_OpenLobbyMenu.IA_OpenLobbyMenu");
		inline constexpr const TCHAR* InputOpenPanel = TEXT("/Game/Pickpacker/Core/Input/IA_OpenPanel.IA_OpenPanel");

		inline constexpr const TCHAR* WidgetLoadingScreen = TEXT("/Game/Pickpacker/Core/UI/WBP_LoadingScreen");
		inline constexpr const TCHAR* WidgetLoadingScreenClass = TEXT("/Game/Pickpacker/Core/UI/WBP_LoadingScreen.WBP_LoadingScreen_C");
		inline constexpr const TCHAR* WidgetLobbyMenu = TEXT("/Game/Pickpacker/Core/UI/WBP_LobbyMenu");
		inline constexpr const TCHAR* WidgetLobbyMenuClass = TEXT("/Game/Pickpacker/Core/UI/WBP_LobbyMenu.WBP_LobbyMenu_C");
		inline constexpr const TCHAR* WidgetInteraction = TEXT("/Game/Pickpacker/Core/UI/HUD/WBP_Interaction");
		inline constexpr const TCHAR* WidgetPickpackerHUDClass = TEXT("/Game/Pickpacker/Core/UI/HUD/WBP_PickpackerHUD.WBP_PickpackerHUD_C");
		inline constexpr const TCHAR* WidgetNPCDialogueClass = TEXT("/Game/Pickpacker/Core/UI/WBP_NPCDialogue.WBP_NPCDialogue_C");
		inline constexpr const TCHAR* WidgetNPCDialogueChoiceClass = TEXT("/Game/Pickpacker/Core/UI/WBP_NPCDialogueChoice.WBP_NPCDialogueChoice_C");
	}

	namespace Maps
	{
		inline constexpr const TCHAR* Root = TEXT("/Game/Maps");
		inline constexpr const TCHAR* EntryMap = TEXT("/Game/Maps/EntryMap");
		inline constexpr const TCHAR* Lobby = TEXT("/Game/Maps/Lobby");
		inline constexpr const TCHAR* GameStartupMap = TEXT("/Game/Maps/GameStartupMap.GameStartupMap");
		inline constexpr const TCHAR* TransitionMap = TEXT("/Game/Maps/TransitionMap.TransitionMap");
		inline constexpr const TCHAR* IndustralMap = TEXT("/Game/Maps/IndustralMap");
		inline constexpr const TCHAR* ElevatorEndingMap = TEXT("/Game/Maps/ElevatorEndingMap");
		inline constexpr const TCHAR* ControlRoom = TEXT("/Game/Maps/Section/ControlRoom");

		inline FString FromMapName(const FString& MapName)
		{
			return FString::Printf(TEXT("%s/%s"), Root, *MapName);
		}
	}

	namespace Config
	{
		inline constexpr const TCHAR* EasyGameUIInputIconDataTable = TEXT("/Game/EasyGameUI/EasyInputPrompts/Datas/DataTables/DT_InputsPrompts_MouseKeyboard.DT_InputsPrompts_MouseKeyboard");
		inline constexpr const TCHAR* RenderTargetDirectory = TEXT("/Game/Pickpacker/Core/Shared/RenderTarget");
	}
}
