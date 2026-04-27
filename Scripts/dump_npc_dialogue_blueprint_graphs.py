from pathlib import Path

import unreal


BLUEPRINT_PATHS = [
    "/Game/Pickpacker/Core/UI/WBP_NPCDialogue",
    "/Game/Pickpacker/Core/UI/WBP_NPCDialogueChoice",
]


def get_graphs(bp):
    graphs = []
    for attr in ("ubergraph_pages", "function_graphs", "macro_graphs", "delegate_signature_graphs"):
        try:
            graphs.extend(bp.get_editor_property(attr) or [])
        except Exception:
            pass
    return graphs


def main():
    lines = []

    for blueprint_path in BLUEPRINT_PATHS:
        bp = unreal.EditorAssetLibrary.load_asset(blueprint_path)
        lines.append(f"Blueprint: {blueprint_path}")
        if not bp:
            lines.append("  FAILED TO LOAD")
            lines.append("")
            continue

        for graph in get_graphs(bp):
            lines.append(f"Graph: {graph.get_name()}")
            for node in graph.get_nodes():
                try:
                    title = node.get_node_title(unreal.NodeTitleType.LIST_VIEW)
                except Exception:
                    title = node.get_name()

                if not any(token in str(title) for token in [
                    "Mouse", "Hover", "Click", "Pressed", "Released", "Visibility",
                    "Enabled", "Focus", "Choice", "Continue", "Close", "Construct", "Destruct"
                ]):
                    continue

                lines.append(
                    f"  node={node.get_name()} class={node.get_class().get_name()} title={title}"
                )
            lines.append("")

    report_path = Path(unreal.Paths.project_dir()) / "Docs" / "NPCDialogueBlueprintGraphs.txt"
    report_path.write_text("\n".join(lines), encoding="utf-8")
    unreal.log(f"Wrote report: {report_path}")


if __name__ == "__main__":
    main()
