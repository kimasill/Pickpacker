import unreal


ROOT = "/Game/BluePrints"


def main():
    assets = unreal.EditorAssetLibrary.list_assets(ROOT, recursive=True, include_folder=False)
    unreal.log(f"{ROOT} asset count: {len(assets)}")
    for asset_path in assets:
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        class_name = asset.get_class().get_name() if asset else "None"
        unreal.log(f"{asset_path} -> {class_name}")


if __name__ == "__main__":
    main()
