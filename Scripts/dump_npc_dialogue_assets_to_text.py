from pathlib import Path

import unreal


ASSET_PATHS = [
    "/Game/Pickpacker/Core/UI/WBP_NPCDialogue",
    "/Game/Pickpacker/Core/UI/WBP_NPCDialogueChoice",
]


def main():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    out_lines = []

    for asset_path in ASSET_PATHS:
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        out_lines.append(f"Asset: {asset_path}")
        out_lines.append(f"Loaded: {asset}")
        if not asset:
            out_lines.append("")
            continue

        try:
            dump_path = asset_tools.dump_asset_to_temp_file(asset)
            out_lines.append(f"DumpPath: {dump_path}")

            if dump_path:
                text = Path(dump_path).read_text(encoding="utf-8", errors="ignore")
                out_lines.append(text)
        except Exception as exc:
            out_lines.append(f"ERROR: {exc}")

        out_lines.append("")

    report_path = Path(unreal.Paths.project_dir()) / "Docs" / "NPCDialogueAssetDump.txt"
    report_path.write_text("\n".join(out_lines), encoding="utf-8")
    unreal.log(f"Wrote asset dump: {report_path}")


if __name__ == "__main__":
    main()
