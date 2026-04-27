import sys
import unreal


FOLDER_MAPPINGS = {
    "/Game/BluePrints/Acting": "/Game/Pickpacker/Gameplay/Acting",
    "/Game/BluePrints/AI": "/Game/Pickpacker/Gameplay/AI",
    "/Game/BluePrints/Camera": "/Game/Pickpacker/Core/Camera",
    "/Game/BluePrints/Character": "/Game/Pickpacker/Characters/Blueprints",
    "/Game/BluePrints/Data": "/Game/Pickpacker/Core/Data/Blueprints",
    "/Game/BluePrints/Edits": "/Game/Pickpacker/Editor/Edits",
    "/Game/BluePrints/GameModes": "/Game/Pickpacker/Core/GameModes",
    "/Game/BluePrints/GamePlay": "/Game/Pickpacker/Gameplay/Systems",
    "/Game/BluePrints/GameState": "/Game/Pickpacker/Core/GameState",
    "/Game/BluePrints/Gates": "/Game/Pickpacker/Gameplay/Gates",
    "/Game/BluePrints/HUD": "/Game/Pickpacker/Core/UI/HUD",
    "/Game/BluePrints/Inputs": "/Game/Pickpacker/Core/Input",
    "/Game/BluePrints/Notify": "/Game/Pickpacker/Characters/Animation/Notify",
    "/Game/BluePrints/Parcel": "/Game/Pickpacker/Gameplay/Parcel",
    "/Game/BluePrints/Pawn": "/Game/Pickpacker/Characters/Pawn",
    "/Game/BluePrints/PlayerController": "/Game/Pickpacker/Core/PlayerController",
    "/Game/BluePrints/PlayerState": "/Game/Pickpacker/Core/PlayerState",
    "/Game/BluePrints/RenderTarget": "/Game/Pickpacker/Core/Shared/RenderTarget",
    "/Game/BluePrints/Shelf": "/Game/Pickpacker/Gameplay/Shelf",
    "/Game/BluePrints/SpawnPoints": "/Game/Pickpacker/Gameplay/SpawnPoints",
    "/Game/BluePrints/Station": "/Game/Pickpacker/Gameplay/Stations",
    "/Game/BluePrints/UI": "/Game/Pickpacker/Core/UI",
    "/Game/BluePrints/Weapon": "/Game/Pickpacker/Gameplay/Weapons",
}


def ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        if not unreal.EditorAssetLibrary.make_directory(path):
            raise RuntimeError(f"Failed to create directory: {path}")


def ensure_parent_directory(asset_path: str) -> None:
    parts = asset_path.strip("/").split("/")[:-1]
    current = ""
    for part in parts:
        current = f"{current}/{part}" if current else f"/{part}"
        ensure_directory(current)


def get_destination_package(package_path: str) -> str | None:
    for source_root, target_root in sorted(FOLDER_MAPPINGS.items(), key=lambda item: len(item[0]), reverse=True):
        if package_path == source_root or package_path.startswith(source_root + "/"):
            suffix = package_path[len(source_root):]
            return target_root + suffix
    return None


def main() -> None:
    assets = unreal.EditorAssetLibrary.list_assets("/Game/BluePrints", recursive=True, include_folder=False)
    unreal.log(f"Remaining blueprint assets found: {len(assets)}")

    moved = 0
    skipped = 0
    failures = []

    for asset_object_path in assets:
        source_package = asset_object_path.split(".")[0]
        destination_package = get_destination_package(source_package)
        if not destination_package:
            skipped += 1
            unreal.log_warning(f"No mapping for asset: {source_package}")
            continue

        if unreal.EditorAssetLibrary.does_asset_exist(destination_package):
            skipped += 1
            unreal.log_warning(f"Destination already exists, skipping: {destination_package}")
            continue

        try:
            ensure_parent_directory(destination_package)
            unreal.log(f"Moving asset: {source_package} -> {destination_package}")
            if not unreal.EditorAssetLibrary.rename_asset(source_package, destination_package):
                raise RuntimeError(f"Failed to move asset: {source_package} -> {destination_package}")
            moved += 1
        except Exception as exc:
            failures.append(str(exc))
            unreal.log_error(str(exc))

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log(f"Remaining blueprint asset move finished. moved={moved}, skipped={skipped}, failures={len(failures)}")

    if failures:
        raise RuntimeError("Remaining blueprint asset move failed:\n" + "\n".join(failures))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"Remaining blueprint asset move failed: {exc}")
        sys.exit(1)
