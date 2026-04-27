import unreal


ROOTS = ["/Game/Fx", "/Game/Sequences", "/Game/Characters"]


def get_redirectors():
    redirectors = []
    for root in ROOTS:
        for asset_path in unreal.EditorAssetLibrary.list_assets(
            root, recursive=True, include_folder=False
        ):
            asset = unreal.EditorAssetLibrary.load_asset(asset_path)
            if asset and asset.get_class().get_name() == "ObjectRedirector":
                redirectors.append(asset)
    return redirectors


def get_package_name(asset):
    package = asset.get_outermost()
    return package.get_name()


def get_referencers(asset_registry, package_name):
    options = unreal.AssetRegistryDependencyOptions(
        include_hard_package_references=True,
        include_soft_package_references=True,
        include_hard_management_references=False,
        include_soft_management_references=False,
        include_searchable_names=False,
    )
    referencers = asset_registry.get_referencers(package_name, options)
    return [str(name) for name in referencers if str(name) != package_name]


def save_referencer_package(package_name):
    if not package_name.startswith("/Game/"):
        return False

    loaded = unreal.EditorAssetLibrary.load_asset(package_name)
    if loaded:
        saved = unreal.EditorAssetLibrary.save_loaded_asset(loaded, only_if_is_dirty=False)
        unreal.log(f"Saved loaded referencer: {package_name} -> {saved}")
        return bool(saved)

    saved = unreal.EditorAssetLibrary.save_asset(package_name, only_if_is_dirty=False)
    unreal.log(f"Saved referencer asset path: {package_name} -> {saved}")
    return bool(saved)


def main():
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    redirectors = get_redirectors()

    unreal.log(f"Found redirectors: {len(redirectors)}")

    referencer_packages = set()
    for redirector in redirectors:
        package_name = get_package_name(redirector)
        referencers = get_referencers(asset_registry, package_name)
        unreal.log(f"Redirector {package_name} referencers: {len(referencers)}")
        for referencer in referencers:
            unreal.log(f"  - {referencer}")
            referencer_packages.add(referencer)

    saved_count = 0
    for referencer in sorted(referencer_packages):
        try:
            if save_referencer_package(referencer):
                saved_count += 1
        except Exception as exc:
            unreal.log_warning(f"Failed to save {referencer}: {exc}")

    unreal.log(f"Saved referencer packages: {saved_count}/{len(referencer_packages)}")


if __name__ == "__main__":
    main()
