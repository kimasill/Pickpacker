from pathlib import Path

import unreal


ASSET_PATHS = [
    "/Game/Pickpacker/Core/Data/Blueprints/Virtual_AC_RunAction",
    "/Game/BluePrints/Data/Virtual_AC_RunAction",
    "/Game/BluePrints/GamePlay/AC_CallAction",
    "/Game/Pickpacker/Gameplay/Systems/AC_SitInCar",
    "/Game/Pickpacker/Gameplay/Systems/AC_SitInDrone",
]


def describe_asset(asset_path):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not asset:
        return [f"{asset_path} | load_failed"]

    lines = [f"{asset_path} | class={asset.get_class().get_name()}"]

    try:
        parent_class = asset.get_editor_property("parent_class")
        if parent_class:
            lines.append(f"  parent_class={parent_class.get_path_name()}")
    except Exception:
        pass

    try:
        generated_class = asset.get_editor_property("generated_class")
        if generated_class:
            lines.append(f"  generated_class={generated_class.get_path_name()}")
    except Exception:
        pass

    return lines


def main():
    lines = []
    for asset_path in ASSET_PATHS:
        lines.extend(describe_asset(asset_path))
        lines.append("")

    report_path = Path(unreal.Paths.project_dir()) / "Docs" / "ActionAsset_Introspection.txt"
    report_path.write_text("\n".join(lines), encoding="utf-8")
    unreal.log(f"Wrote report: {report_path}")


if __name__ == "__main__":
    main()
