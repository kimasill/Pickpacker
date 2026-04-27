from pathlib import Path

import unreal


ASSET_PATHS = [
    "/Game/Pickpacker/Core/GameModes/BP_PickPackerGameMode",
    "/Game/Pickpacker/Core/PlayerController/BP_PickpackerPlayerController",
    "/Game/Pickpacker/Core/PlayerState/BP_PickpackerPlayerState",
    "/Game/Pickpacker/Characters/Blueprints/BP_Robot",
    "/Game/Pickpacker/Core/Input/IA_Jump",
    "/Game/Pickpacker/Core/Input/IMC_MainGameInputs",
]


def describe_asset(asset_path):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not asset:
        return f"{asset_path} | load_failed"

    parts = [f"{asset_path} | class={asset.get_class().get_name()}"]

    try:
        generated_class = asset.get_editor_property("generated_class")
        if generated_class:
            parts.append(f"generated_class={generated_class.get_path_name()}")
            if asset_path.endswith("BP_PickPackerGameMode"):
                default_object = unreal.get_default_object(generated_class)
                player_controller_class = default_object.get_editor_property("player_controller_class")
                player_state_class = default_object.get_editor_property("player_state_class")
                parts.append(f"player_controller_class={player_controller_class.get_path_name() if player_controller_class else 'None'}")
                parts.append(f"player_state_class={player_state_class.get_path_name() if player_state_class else 'None'}")
    except Exception as exc:
        parts.append(f"inspect_error={exc}")

    return " | ".join(parts)


def main():
    lines = [describe_asset(asset_path) for asset_path in ASSET_PATHS]
    report_path = Path(unreal.Paths.project_dir()) / "Docs" / "StartupAssetValidation.txt"
    report_path.write_text("\n".join(lines), encoding="utf-8")
    unreal.log(f"Wrote report: {report_path}")


if __name__ == "__main__":
    main()
