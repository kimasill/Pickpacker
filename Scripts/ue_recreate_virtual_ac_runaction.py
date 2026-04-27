from pathlib import Path

import unreal


TARGET_DIR = "/Game/Pickpacker/Core/Data/Blueprints"
TARGET_NAME = "Virtual_AC_RunAction"
PARENT_BLUEPRINT = "/Game/BluePrints/GamePlay/AC_CallAction"


def main():
    parent_class = unreal.EditorAssetLibrary.load_blueprint_class(PARENT_BLUEPRINT)
    if not parent_class:
        raise RuntimeError(f"Failed to load parent blueprint class: {PARENT_BLUEPRINT}")

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)

    asset = asset_tools.create_asset(TARGET_NAME, TARGET_DIR, unreal.Blueprint, factory)
    if not asset:
        raise RuntimeError("Failed to create blueprint asset")

    saved = unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

    report_lines = [
        f"created={asset.get_path_name()}",
        f"parent={parent_class.get_path_name()}",
        f"saved={saved}",
    ]
    report_path = Path(unreal.Paths.project_dir()) / "Docs" / "Virtual_AC_RunAction_Recreated.txt"
    report_path.write_text("\n".join(report_lines), encoding="utf-8")
    unreal.log(f"Wrote report: {report_path}")


if __name__ == "__main__":
    main()
