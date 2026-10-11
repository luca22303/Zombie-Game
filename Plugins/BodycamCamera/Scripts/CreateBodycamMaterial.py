"""
Creates /Game/Bodycam/M_Bodycam_Lens (the post-process material used by BodycamProfile > Lens Material)
from Docs/HLSL/BodycamLens.hlsl.

Run it inside the Unreal Editor (enable the "Python Editor Script Plugin" first):
    Output Log > switch to "Python" > py "<project>/Plugins/BodycamCamera/Scripts/CreateBodycamMaterial.py"
or  Tools > Execute Python Script...

NOTE: written against the UE5 Python API but not run in CI. If a step errors, follow the manual steps in
README.md (they produce the identical material).
"""

import os
import unreal

PACKAGE_PATH = "/Game/Bodycam"
ASSET_NAME = "M_Bodycam_Lens"

HLSL_PATH = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Docs", "HLSL", "BodycamLens.hlsl")
)

# (parameter name, default). The runtime component overwrites these every frame.
SCALAR_PARAMS = [
    ("Distortion", 0.35),
    ("RollingShutter", 0.0),
    ("YawRate", 0.0),
    ("Compression", 0.0),
    ("SensorNoise", 0.0),
    ("Aspect", 1.7778),
    ("ViewWidth", 1920.0),
    ("ViewHeight", 1080.0),
]
TIME_INPUT = "ClockTime"


def main():
    full_path = f"{PACKAGE_PATH}/{ASSET_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        unreal.log_warning(f"{full_path} already exists - delete it first if you want to regenerate it.")
        return

    with open(HLSL_PATH, "r", encoding="utf-8") as f:
        code = f.read()

    lib = unreal.MaterialEditingLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(ASSET_NAME, PACKAGE_PATH, unreal.Material, unreal.MaterialFactoryNew())

    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    mat.set_editor_property("blendable_location", unreal.BlendableLocation.BL_AFTER_TONEMAPPING)

    custom = lib.create_material_expression(mat, unreal.MaterialExpressionCustom, 300, 0)

    nodes = []
    y = -450
    for name, default in SCALAR_PARAMS:
        node = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -300, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", default)
        nodes.append((name, node))
        y += 150

    time_node = lib.create_material_expression(mat, unreal.MaterialExpressionTime, -300, y)
    nodes.append((TIME_INPUT, time_node))

    inputs = []
    for name, _ in nodes:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)

    custom.set_editor_property("code", code)
    custom.set_editor_property("description", "Bodycam Lens")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("inputs", inputs)

    for name, node in nodes:
        lib.connect_material_expressions(node, "", custom, name)

    lib.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    unreal.log(f"Created {full_path}. Assign it to your BodycamProfile > Lens > Lens Material.")


main()
