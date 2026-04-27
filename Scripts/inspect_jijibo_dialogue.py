import unreal


def safe_name(value):
    if value is None:
        return "None"
    try:
        return str(value)
    except Exception:
        return repr(value)


def dump_data_asset():
    asset = unreal.EditorAssetLibrary.load_asset("/Game/BluePrints/AI/Jijibo/DA_NPC_Jijibo")
    unreal.log(f"[InspectJijibo] DA_NPC_Jijibo asset={asset}")
    if not asset:
        return

    for prop in ["config_data_table", "config_csv_file", "config_row_name", "dialogue_script_data_table", "dialogue_csv_file", "mesh_override", "anim_class_override"]:
        try:
            unreal.log(f"[InspectJijibo] {prop}={safe_name(asset.get_editor_property(prop))}")
        except Exception as exc:
            unreal.log_warning(f"[InspectJijibo] failed to read {prop}: {exc}")

    try:
        config_table = asset.get_editor_property("config_data_table")
        if config_table:
            row_names = unreal.DataTableFunctionLibrary.get_data_table_row_names(config_table)
            unreal.log(f"[InspectJijibo] config row names={row_names}")
    except Exception as exc:
        unreal.log_warning(f"[InspectJijibo] failed reading config table rows: {exc}")


def dump_dialogue_table():
    table = unreal.EditorAssetLibrary.load_asset("/Game/BluePrints/AI/Jijibo/DT_Dialogue_Jijibo")
    unreal.log(f"[InspectJijibo] DT_Dialogue_Jijibo asset={table}")
    if not table:
        return

    try:
        row_names = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
        unreal.log(f"[InspectJijibo] dialogue row names={row_names}")
        for row_name in row_names[:8]:
            row = unreal.DataTableFunctionLibrary.get_data_table_row_from_name(table, row_name)
            unreal.log(f"[InspectJijibo] row {row_name}={row}")
    except Exception as exc:
        unreal.log_warning(f"[InspectJijibo] failed reading table rows: {exc}")


def dump_blueprint_defaults():
    bp = unreal.EditorAssetLibrary.load_asset("/Game/BluePrints/AI/Jijibo/BP_Jijibo")
    unreal.log(f"[InspectJijibo] BP_Jijibo asset={bp}")
    if not bp:
        return

    try:
        generated_class = bp.generated_class()
    except Exception:
        generated_class = None

    unreal.log(f"[InspectJijibo] generated_class={generated_class}")
    if not generated_class:
        return

    try:
        cdo = unreal.get_default_object(generated_class)
        unreal.log(f"[InspectJijibo] CDO={cdo}")
        for prop in ["npc_data", "npc_id", "display_name", "disposition"]:
            try:
                unreal.log(f"[InspectJijibo] CDO {prop}={safe_name(cdo.get_editor_property(prop))}")
            except Exception as exc:
                unreal.log_warning(f"[InspectJijibo] failed to read CDO {prop}: {exc}")

        try:
            components = cdo.get_components_by_class(unreal.ActorComponent)
            for component in components:
                component_name = component.get_name()
                component_class = component.get_class().get_name()
                if "Dialogue" not in component_name and "Dialogue" not in component_class:
                    continue

                unreal.log(f"[InspectJijibo] component {component_name} class={component_class}")
                for prop in ["default_speaker_name", "dialogue_nodes", "b_in_conversation", "current_node_index"]:
                    try:
                        value = component.get_editor_property(prop)
                        if prop == "dialogue_nodes":
                            unreal.log(f"[InspectJijibo] component {component_name} dialogue_nodes_count={len(value)}")
                            if len(value) > 0:
                                first_node = value[0]
                                unreal.log(f"[InspectJijibo] first_node speaker={safe_name(first_node.speaker_name)} text={safe_name(first_node.dialogue_text)}")
                        else:
                            unreal.log(f"[InspectJijibo] component {component_name} {prop}={safe_name(value)}")
                    except Exception as exc:
                        unreal.log_warning(f"[InspectJijibo] failed to read component {component_name} {prop}: {exc}")
        except Exception as exc:
            unreal.log_warning(f"[InspectJijibo] failed reading components: {exc}")
    except Exception as exc:
        unreal.log_warning(f"[InspectJijibo] failed reading BP defaults: {exc}")


dump_data_asset()
dump_dialogue_table()
dump_blueprint_defaults()
