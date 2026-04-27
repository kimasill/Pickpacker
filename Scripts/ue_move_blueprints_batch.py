import sys
import unreal


BLUEPRINT_MOVES = [
    ("/Game/BluePrints/Inputs", "/Game/Pickpacker/Core/Input"),
    ("/Game/BluePrints/UI", "/Game/Pickpacker/Core/UI"),
    ("/Game/BluePrints/HUD", "/Game/Pickpacker/Core/UI/HUD"),
    ("/Game/BluePrints/GameModes", "/Game/Pickpacker/Core/GameModes"),
    ("/Game/BluePrints/Data", "/Game/Pickpacker/Core/Data/Blueprints"),
    ("/Game/BluePrints/RenderTarget", "/Game/Pickpacker/Core/Shared/RenderTarget"),
    ("/Game/BluePrints/GamePlay", "/Game/Pickpacker/Gameplay/Systems"),
    ("/Game/BluePrints/AI", "/Game/Pickpacker/Gameplay/AI"),
    ("/Game/BluePrints/Weapon", "/Game/Pickpacker/Gameplay/Weapons"),
    ("/Game/BluePrints/Parcel", "/Game/Pickpacker/Gameplay/Parcel"),
    ("/Game/BluePrints/Station", "/Game/Pickpacker/Gameplay/Stations"),
    ("/Game/BluePrints/SpawnPoints", "/Game/Pickpacker/Gameplay/SpawnPoints"),
    ("/Game/BluePrints/Gates", "/Game/Pickpacker/Gameplay/Gates"),
    ("/Game/BluePrints/Shelf", "/Game/Pickpacker/Gameplay/Shelf"),
    ("/Game/BluePrints/Acting", "/Game/Pickpacker/Gameplay/Acting"),
    ("/Game/BluePrints/Notify", "/Game/Pickpacker/Characters/Animation/Notify"),
    ("/Game/BluePrints/Camera", "/Game/Pickpacker/Core/Camera"),
    ("/Game/BluePrints/Character", "/Game/Pickpacker/Characters/Blueprints"),
    ("/Game/BluePrints/Pawn", "/Game/Pickpacker/Characters/Pawn"),
    ("/Game/BluePrints/PlayerController", "/Game/Pickpacker/Core/PlayerController"),
    ("/Game/BluePrints/PlayerState", "/Game/Pickpacker/Core/PlayerState"),
    ("/Game/BluePrints/GameState", "/Game/Pickpacker/Core/GameState"),
    ("/Game/BluePrints/Edits", "/Game/Pickpacker/Editor/Edits"),
]


def ensure_directory(path: str) -> None:
    if unreal.EditorAssetLibrary.does_directory_exist(path):
        return

    if not unreal.EditorAssetLibrary.make_directory(path):
        raise RuntimeError(f"Failed to create directory: {path}")


def ensure_parent_directories(path: str) -> None:
    parts = path.strip("/").split("/")
    current = ""
    for part in parts:
        current = f"{current}/{part}" if current else f"/{part}"
        ensure_directory(current)


def move_directory(src: str, dst: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(src):
        unreal.log_warning(f"Skipping missing directory: {src}")
        return

    if unreal.EditorAssetLibrary.does_directory_exist(dst):
        unreal.log_warning(f"Skipping existing destination: {dst}")
        return

    ensure_parent_directories("/".join(dst.split("/")[:-1]))

    unreal.log(f"Moving directory: {src} -> {dst}")
    if not unreal.EditorAssetLibrary.rename_directory(src, dst):
        raise RuntimeError(f"Failed to move directory: {src} -> {dst}")

    unreal.EditorAssetLibrary.save_directory(dst, only_if_is_dirty=False, recursive=True)


def main() -> None:
    unreal.log("Blueprint batch move started")

    failures = []
    for src, dst in BLUEPRINT_MOVES:
        try:
            move_directory(src, dst)
        except Exception as exc:
            failures.append(str(exc))
            unreal.log_error(str(exc))

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    if failures:
        raise RuntimeError("Blueprint batch move failed:\n" + "\n".join(failures))

    unreal.log("Blueprint batch move finished successfully")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"Blueprint batch move failed: {exc}")
        sys.exit(1)
