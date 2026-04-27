from pathlib import Path

import unreal


MAP_PATH = "/Game/Maps/TechArt"
TARGET_PACKAGE = "/Game/Pickpacker/Characters/Pawn/PW_Crane"


def main():
    report_lines = []
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)

    actors = unreal.EditorLevelLibrary.get_all_level_actors()

    target_actor_names = []
    for actor in actors:
        cls = actor.get_class()
        cls_path = str(cls.get_path_name()) if cls else ""
        if TARGET_PACKAGE in cls_path or "PW_Crane" in cls_path:
            target_actor_names.append(f"{actor.get_name()} | class={cls_path}")

    report_lines.append(f"Map: {MAP_PATH}")
    report_lines.append(f"Matched actors: {len(target_actor_names)}")
    report_lines.extend(target_actor_names)

    report_path = Path(unreal.Paths.project_dir()) / "Docs" / "PW_Crane_MapActors.txt"
    report_path.write_text("\n".join(report_lines), encoding="utf-8")
    unreal.log(f"Wrote report: {report_path}")


if __name__ == "__main__":
    main()
