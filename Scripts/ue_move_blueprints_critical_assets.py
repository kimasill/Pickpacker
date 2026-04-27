import sys
import unreal


CRITICAL_ASSET_MOVES = [
    ("/Game/BluePrints/Data/BP_FunctionLibrary", "/Game/Pickpacker/Core/Data/Blueprints/BP_FunctionLibrary"),
    ("/Game/BluePrints/Data/ML_A_MacroLibrary", "/Game/Pickpacker/Core/Data/Blueprints/ML_A_MacroLibrary"),
    ("/Game/BluePrints/Data/ML_Ob_MacroLibrary", "/Game/Pickpacker/Core/Data/Blueprints/ML_Ob_MacroLibrary"),
    ("/Game/BluePrints/Pawn/ThirdPersonCharacter", "/Game/Pickpacker/Characters/Pawn/ThirdPersonCharacter"),
    ("/Game/BluePrints/RenderTarget/RT_DroneCamera", "/Game/Pickpacker/Core/Shared/RenderTarget/RT_DroneCamera"),
    ("/Game/BluePrints/UI/W_Crane", "/Game/Pickpacker/Core/UI/W_Crane"),
    ("/Game/BluePrints/UI/W_Drone", "/Game/Pickpacker/Core/UI/W_Drone"),
    ("/Game/BluePrints/UI/W_HitPoint", "/Game/Pickpacker/Core/UI/W_HitPoint"),
    ("/Game/BluePrints/UI/W_OutCar", "/Game/Pickpacker/Core/UI/W_OutCar"),
]


def ensure_directory(path: str) -> None:
    if unreal.EditorAssetLibrary.does_directory_exist(path):
        return
    if not unreal.EditorAssetLibrary.make_directory(path):
        raise RuntimeError(f"Failed to create directory: {path}")


def ensure_parent_directory(asset_path: str) -> None:
    parts = asset_path.strip("/").split("/")[:-1]
    current = ""
    for part in parts:
        current = f"{current}/{part}" if current else f"/{part}"
        ensure_directory(current)


def main() -> None:
    failures = []

    for src, dst in CRITICAL_ASSET_MOVES:
        unreal.log(f"Checking critical asset move: {src} -> {dst}")
        unreal.log(f"  source exists: {unreal.EditorAssetLibrary.does_asset_exist(src)}")
        unreal.log(f"  destination exists: {unreal.EditorAssetLibrary.does_asset_exist(dst)}")

        if not unreal.EditorAssetLibrary.does_asset_exist(src):
            continue

        if unreal.EditorAssetLibrary.does_asset_exist(dst):
            unreal.log_warning(f"Destination already exists, skipping: {dst}")
            continue

        try:
            ensure_parent_directory(dst)
            if not unreal.EditorAssetLibrary.rename_asset(src, dst):
                raise RuntimeError(f"rename_asset returned false for {src} -> {dst}")
            unreal.log(f"Moved critical asset: {src} -> {dst}")
        except Exception as exc:
            failures.append(str(exc))
            unreal.log_error(str(exc))

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    if failures:
        raise RuntimeError("Critical blueprint asset move failed:\n" + "\n".join(failures))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"Critical blueprint asset move failed: {exc}")
        sys.exit(1)
