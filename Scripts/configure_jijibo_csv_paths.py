import unreal


asset = unreal.EditorAssetLibrary.load_asset("/Game/BluePrints/AI/Jijibo/DA_NPC_Jijibo")
if not asset:
    raise RuntimeError("DA_NPC_Jijibo not found")

config_path = unreal.FilePath()
config_path.file_path = "Docs/NPC_Config_Jijibo.csv"
dialogue_path = unreal.FilePath()
dialogue_path.file_path = "Docs/NPC_Dialogue_Jijibo.csv"

asset.set_editor_property("config_csv_file", config_path)
asset.set_editor_property("dialogue_csv_file", dialogue_path)

unreal.EditorAssetLibrary.save_loaded_asset(asset)
unreal.log("[ConfigureJijibo] Updated DA_NPC_Jijibo CSV paths.")
