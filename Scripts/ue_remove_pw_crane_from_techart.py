from pathlib import Path

import unreal


MAP_PATH = "/Game/Maps/TechArt"
TARGET_CLASS_PATH = "/Game/Pickpacker/Characters/Pawn/PW_Crane.PW_Crane_C"


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)

    removed = []
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        cls = actor.get_class()
        cls_path = str(cls.get_path_name()) if cls else ""
        if cls_path == TARGET_CLASS_PATH:
            removed.append(actor.get_name())
            unreal.EditorLevelLibrary.destroy_actor(actor)

    save_result = unreal.EditorAssetLibrary.save_asset(MAP_PATH, only_if_is_dirty=False)

    report_lines = [
        f"Map: {MAP_PATH}",
        f"Removed actors: {len(removed)}",
        *removed,
        f"Saved: {save_result}",
    ]
    report_path = Path(unreal.Paths.project_dir()) / "Docs" / "PW_Crane_Removal.txt"
    report_path.write_text("\n".join(report_lines), encoding="utf-8")
    unreal.log(f"Wrote report: {report_path}")


if __name__ == "__main__":
    main()
