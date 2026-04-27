import unreal


ASSET_PATHS = [
    "/Game/Pickpacker/Core/UI/WBP_NPCDialogue",
    "/Game/Pickpacker/Core/UI/WBP_NPCDialogueChoice",
    "/Game/BluePrints/PlayerController/BP_BlasterPlayerController",
    "/Game/BluePrints/PlayerController/BP_PickpackerPlayerController",
]


def safe_str(value):
    try:
        return str(value)
    except Exception:
        return repr(value)


def log(msg):
    unreal.log(f"[InspectNPCDialogue] {msg}")


def log_warn(msg):
    unreal.log_warning(f"[InspectNPCDialogue] {msg}")


def widget_children(widget):
    children = []

    try:
        count = widget.get_children_count()
        for index in range(count):
            children.append(widget.get_child_at(index))
        return children
    except Exception:
        pass

    try:
        content = widget.get_editor_property("content")
        if content:
            children.append(content)
    except Exception:
        pass

    return children


def dump_widget(widget, depth=0, seen=None):
    if widget is None:
        return

    if seen is None:
        seen = set()

    widget_id = id(widget)
    if widget_id in seen:
        log(f"{'  ' * depth}- {widget.get_name()} (already visited)")
        return
    seen.add(widget_id)

    parts = [
        f"name={widget.get_name()}",
        f"class={widget.get_class().get_name()}",
    ]

    for prop in ["visibility", "is_enabled", "is_variable"]:
        try:
            parts.append(f"{prop}={safe_str(widget.get_editor_property(prop))}")
        except Exception:
            pass

    log(f"{'  ' * depth}- " + ", ".join(parts))

    for child in widget_children(widget):
        dump_widget(child, depth + 1, seen)


def dump_widget_blueprint(asset_path):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    log(f"asset_path={asset_path}, asset={safe_str(asset)}")
    if not asset:
        return

    try:
        generated_class = asset.generated_class()
        log(f"{asset_path}: generated_class={safe_str(generated_class)}")
        cdo = unreal.get_default_object(generated_class)
        try:
            widget_tree = cdo.get_editor_property("widget_tree")
        except Exception as exc:
            log_warn(f"{asset_path}: no widget_tree on CDO ({exc})")
            widget_tree = None

        if widget_tree:
            try:
                root = widget_tree.get_editor_property("root_widget")
            except Exception as exc:
                log_warn(f"{asset_path}: failed to get root_widget from CDO ({exc})")
                root = None

            if root:
                log(f"{asset_path}: widget tree from CDO")
                dump_widget(root)
            else:
                log_warn(f"{asset_path}: root_widget on CDO is None")

        for prop in ["choice_widget_class", "npc_dialogue_widget_class"]:
            try:
                log(f"{asset_path}: {prop}={safe_str(cdo.get_editor_property(prop))}")
            except Exception:
                pass
        for prop in [
            "speaker_text_block", "dialogue_text_block", "choices_panel",
            "continue_button", "close_button", "continue_button_text_block",
            "close_button_text_block", "choice_button", "choice_text_block"
        ]:
            try:
                value = cdo.get_editor_property(prop)
                log(f"{asset_path}: {prop}={safe_str(value)}")
            except Exception:
                pass
    except Exception as exc:
        log_warn(f"{asset_path}: failed to inspect defaults ({exc})")


for asset_path in ASSET_PATHS:
    dump_widget_blueprint(asset_path)
