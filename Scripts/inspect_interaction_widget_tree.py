import unreal
assets = ["/Game/BluePrints/HUD/WBP_Interaction", "/Game/BluePrints/HUD/WBP_InteractionSub"]
for asset_path in assets:
    asset = unreal.load_asset(asset_path)
    unreal.log(f"[inspect_interaction] asset={asset_path} loaded={asset is not None}")
    if not asset:
        continue
    try:
        widget_tree = asset.get_editor_property("widget_tree")
        unreal.log(f"[inspect_interaction] widget_tree={widget_tree}")
        if widget_tree:
            for widget in widget_tree.get_all_widgets():
                unreal.log(f"[inspect_interaction] widget name={widget.get_name()} class={widget.get_class().get_name()}")
                for prop_name in ["text", "visibility", "input_action", "input_mapping_context", "use_input_action", "mouse_keyboard_key", "gamepad_key"]:
                    try:
                        value = widget.get_editor_property(prop_name)
                        unreal.log(f"[inspect_interaction]   {widget.get_name()}.{prop_name}={value}")
                    except Exception:
                        pass
    except Exception as exc:
        unreal.log_warning(f"[inspect_interaction] widget_tree_error={exc}")
