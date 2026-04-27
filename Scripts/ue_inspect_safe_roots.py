import unreal

ROOTS = ["/Game/Fx", "/Game/Sequences", "/Game/Characters", "/Game/Audio"]

for root in ROOTS:
    assets = unreal.EditorAssetLibrary.list_assets(root, recursive=True, include_folder=False)
    unreal.log(f"{root} asset count: {len(assets)}")
    for asset_path in assets:
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        class_name = asset.get_class().get_name() if asset else "None"
        unreal.log(f"{asset_path} -> {class_name}")
