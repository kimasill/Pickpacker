import unreal

SOURCE_ABP = "/Game/Pickpacker/Gameplay/AI/Animation/ABP_AI_Base"
TARGET_FOLDER = "/Game/Pickpacker/Gameplay/AI/Animation/NPC"
TARGET_ABP_NAME = "ABP_NPC_Base"
TARGET_ANIMSET_NAME = "DA_NPCAnimSet_Default"
DEFAULT_GROUNDED_LOCOMOTION = "/Game/Pickpacker/Gameplay/AI/Animation/Guard/BS_Guard_WalkRun"


def log(message: str) -> None:
    unreal.log(f"[create_npc_anim_assets] {message}")


def ensure_directory(asset_dir: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(asset_dir):
        unreal.EditorAssetLibrary.make_directory(asset_dir)
        log(f"Created directory: {asset_dir}")


def load_required_asset(asset_path: str):
    asset = unreal.load_asset(asset_path)
    if asset is None:
        raise RuntimeError(f"Could not load asset: {asset_path}")
    return asset


def duplicate_or_load_anim_blueprint():
    target_asset_path = f"{TARGET_FOLDER}/{TARGET_ABP_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(target_asset_path):
        blueprint = unreal.load_asset(target_asset_path)
        log(f"Using existing blueprint: {target_asset_path}")
    else:
        source_blueprint = load_required_asset(SOURCE_ABP)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
            TARGET_ABP_NAME,
            TARGET_FOLDER,
            source_blueprint,
        )
        if blueprint is None:
            raise RuntimeError("Failed to duplicate base animation blueprint.")
        log(f"Created blueprint: {target_asset_path}")

    npc_anim_instance_class = unreal.load_class(None, "/Script/Pickpacker.NPCAnimInstance")
    if npc_anim_instance_class is None:
        raise RuntimeError("Could not load /Script/Pickpacker.NPCAnimInstance")

    unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, npc_anim_instance_class)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
    log("Reparented ABP_NPC_Base to NPCAnimInstance")
    return blueprint


def create_or_load_anim_set():
    target_asset_path = f"{TARGET_FOLDER}/{TARGET_ANIMSET_NAME}"
    npc_animation_set_class = unreal.load_class(None, "/Script/Pickpacker.NPCAnimationSet")
    if npc_animation_set_class is None:
        raise RuntimeError("Could not load /Script/Pickpacker.NPCAnimationSet")

    if unreal.EditorAssetLibrary.does_asset_exist(target_asset_path):
        anim_set = unreal.load_asset(target_asset_path)
        log(f"Using existing animation set: {target_asset_path}")
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", npc_animation_set_class)
        anim_set = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            TARGET_ANIMSET_NAME,
            TARGET_FOLDER,
            npc_animation_set_class,
            factory,
        )
        if anim_set is None:
            raise RuntimeError("Failed to create DA_NPCAnimSet_Default.")
        log(f"Created animation set: {target_asset_path}")

    if unreal.EditorAssetLibrary.does_asset_exist(DEFAULT_GROUNDED_LOCOMOTION):
        grounded = unreal.load_asset(DEFAULT_GROUNDED_LOCOMOTION)
        anim_set.set_editor_property("grounded_locomotion", grounded)
        log(f"Assigned grounded locomotion: {DEFAULT_GROUNDED_LOCOMOTION}")

    unreal.EditorAssetLibrary.save_loaded_asset(anim_set)
    return anim_set


def main():
    ensure_directory(TARGET_FOLDER)
    duplicate_or_load_anim_blueprint()
    create_or_load_anim_set()
    log("Bootstrap complete. Open ABP_NPC_Base and wire the graph according to Docs/ABP_NPC_Base_Guide.md")


if __name__ == "__main__":
    main()
