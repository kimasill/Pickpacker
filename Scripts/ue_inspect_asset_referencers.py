from pathlib import Path

import unreal


ASSET_PATHS = [
    "/Game/Pickpacker/Characters/Pawn/PW_Crane",
    "/Game/BluePrints/Pawn/PW_Crane",
    "/Game/BluePrints/UI/W_Crane",
    "/Game/Pickpacker/Core/Data/Blueprints/Virtual_AC_RunAction",
    "/Game/BluePrints/Data/Virtual_AC_RunAction",
    "/Game/BluePrints/Data/ML_A_MacroLibrary",
    "/Game/BluePrints/Data/ML_Ob_MacroLibrary",
]


def get_referencers(asset_registry, package_name):
    options = unreal.AssetRegistryDependencyOptions(
        include_hard_package_references=True,
        include_soft_package_references=True,
        include_hard_management_references=True,
        include_soft_management_references=True,
        include_searchable_names=True,
    )
    referencers = asset_registry.get_referencers(package_name, options)
    if not referencers:
        return []
    return sorted(str(name) for name in referencers)


def main():
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    report_lines = []

    for asset_path in ASSET_PATHS:
        data = asset_registry.get_asset_by_object_path(f"{asset_path}.{asset_path.rsplit('/', 1)[-1]}")
        package_name = data.package_name if data and data.is_valid() else asset_path
        package_name = str(package_name)
        header = f"=== {asset_path} ==="
        unreal.log(header)
        report_lines.append(header)
        referencers = get_referencers(asset_registry, package_name)
        count_line = f"Referencers: {len(referencers)}"
        unreal.log(count_line)
        report_lines.append(count_line)
        for referencer in referencers:
            line = f"  - {referencer}"
            unreal.log(line)
            report_lines.append(line)
        report_lines.append("")

    report_path = Path(unreal.Paths.project_dir()) / "Docs" / "PW_Crane_Referencers.txt"
    report_path.write_text("\n".join(report_lines), encoding="utf-8")
    unreal.log(f"Wrote report: {report_path}")


if __name__ == "__main__":
    main()
