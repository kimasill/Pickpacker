import unreal


def inspect_blueprint(asset_path):
    bp = unreal.EditorAssetLibrary.load_asset(asset_path)
    unreal.log(f"[InspectPCDialogue] asset={bp}")
    if not bp:
        return

    try:
        generated_class = bp.generated_class()
        cdo = unreal.get_default_object(generated_class)
        unreal.log(f"[InspectPCDialogue] class={generated_class}")
        for prop in ["npc_dialogue_widget_class", "hud_class", "player_input_class"]:
            try:
                unreal.log(f"[InspectPCDialogue] {asset_path} {prop}={cdo.get_editor_property(prop)}")
            except Exception as exc:
                unreal.log_warning(f"[InspectPCDialogue] failed {asset_path} {prop}: {exc}")
    except Exception as exc:
        unreal.log_warning(f"[InspectPCDialogue] failed {asset_path}: {exc}")


inspect_blueprint("/Game/BluePrints/PlayerController/BP_BlasterPlayerController")
inspect_blueprint("/Game/BluePrints/PlayerController/BP_PickpackerPlayerController")
