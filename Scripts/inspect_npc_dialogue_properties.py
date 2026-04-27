import unreal


BLUEPRINT_PATHS = [
    "/Game/Pickpacker/Core/UI/WBP_NPCDialogue",
    "/Game/Pickpacker/Core/UI/WBP_NPCDialogueChoice",
]


def log(msg):
    unreal.log(f"[InspectNPCDialogueProps] {msg}")


def safe_str(value):
    try:
        return str(value)
    except Exception:
        return repr(value)


for blueprint_path in BLUEPRINT_PATHS:
    bp = unreal.EditorAssetLibrary.load_asset(blueprint_path)
    log(f"asset={blueprint_path} -> {safe_str(bp)}")
    if not bp:
        continue

    generated_class = bp.generated_class()
    cdo = unreal.get_default_object(generated_class)
    log(f"class={safe_str(generated_class)}")

    for name in dir(cdo):
        lowered = name.lower()
        if any(token in lowered for token in [
            "choice", "button", "dialogue", "speaker", "text", "panel", "focus"
        ]):
            log(f"attr={name}")
