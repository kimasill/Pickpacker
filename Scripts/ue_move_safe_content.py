import sys
import unreal


SAFE_MOVES = [
    ("/Game/Audio", "/Game/Pickpacker/Audio"),
    ("/Game/Fx", "/Game/Pickpacker/VFX"),
    ("/Game/Sequences", "/Game/Pickpacker/Cinematics"),
    ("/Game/Characters", "/Game/Pickpacker/Characters"),
]


def ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.log(f"Creating directory: {path}")
        if not unreal.EditorAssetLibrary.make_directory(path):
            raise RuntimeError(f"Failed to create directory: {path}")


def move_directory(src: str, dst: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(src):
        unreal.log_warning(f"Skipping missing directory: {src}")
        return

    if unreal.EditorAssetLibrary.does_directory_exist(dst):
        unreal.log_warning(f"Skipping existing destination: {dst}")
        return

    unreal.log(f"Moving directory: {src} -> {dst}")
    if not unreal.EditorAssetLibrary.rename_directory(src, dst):
        raise RuntimeError(f"Failed to move directory: {src} -> {dst}")

    unreal.EditorAssetLibrary.save_directory(dst, only_if_is_dirty=False, recursive=True)


def main() -> None:
    unreal.log("Safe content move script started")
    ensure_directory("/Game/Pickpacker")
    ensure_directory("/Game/ThirdParty")

    failures = []

    for src, dst in SAFE_MOVES:
        try:
            move_directory(src, dst)
        except Exception as exc:
            failures.append(str(exc))
            unreal.log_error(str(exc))

    if failures:
        raise RuntimeError("Safe content move failed:\n" + "\n".join(failures))

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log("Safe content move script finished successfully")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(f"Safe content move script failed: {exc}")
        sys.exit(1)
