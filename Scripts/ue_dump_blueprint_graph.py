from pathlib import Path

import unreal


BLUEPRINT_PATH = "/Game/Pickpacker/Core/Data/Blueprints/Virtual_AC_RunAction"


def get_graphs(bp):
    graphs = []
    for attr in ("ubergraph_pages", "function_graphs", "macro_graphs", "delegate_signature_graphs"):
        try:
            graphs.extend(bp.get_editor_property(attr) or [])
        except Exception:
            pass
    return graphs


def main():
    bp = unreal.EditorAssetLibrary.load_asset(BLUEPRINT_PATH)
    if not bp:
        raise RuntimeError(f"Failed to load {BLUEPRINT_PATH}")

    lines = [f"Blueprint: {BLUEPRINT_PATH}"]

    for graph in get_graphs(bp):
        lines.append(f"Graph: {graph.get_name()}")
        for node in graph.get_nodes():
            title = ""
            try:
                title = node.get_node_title(unreal.NodeTitleType.LIST_VIEW)
            except Exception:
                title = node.get_name()
            lines.append(
                f"  node={node.get_name()} class={node.get_class().get_name()} title={title}"
            )
        lines.append("")

    report_path = Path(unreal.Paths.project_dir()) / "Docs" / "Virtual_AC_RunAction_Graph.txt"
    report_path.write_text("\n".join(lines), encoding="utf-8")
    unreal.log(f"Wrote report: {report_path}")


if __name__ == "__main__":
    main()
