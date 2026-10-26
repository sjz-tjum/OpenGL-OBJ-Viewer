"""Build the demo asset for the OpenGL OBJ viewer.

Run with:
    "D:/Blender/blender.exe" -b -P assets/models/build_drone.py

The script writes drone.obj / drone.mtl next to itself and a preview render.
It is kept in the project so the asset is reproducible.
"""
import math
import os

import bpy
from mathutils import Vector

OUT_DIR = os.path.dirname(os.path.abspath(__file__))
MODEL_PATH = os.path.join(OUT_DIR, "drone.obj")
PREVIEW_PATH = os.path.join(OUT_DIR, "drone_preview.png")


def reset_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for datablocks in (bpy.data.meshes, bpy.data.materials, bpy.data.cameras, bpy.data.lights):
        for block in list(datablocks):
            if block.users == 0:
                datablocks.remove(block)


def make_material(name, color, metallic=0.0, roughness=0.5, emission=None, emission_strength=0.0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if emission is not None:
        bsdf.inputs["Emission Color"].default_value = (*emission, 1.0)
        bsdf.inputs["Emission Strength"].default_value = emission_strength
    mat.diffuse_color = (*color, 1.0)
    return mat


def assign(obj, mat):
    obj.data.materials.clear()
    obj.data.materials.append(mat)


def apply_scale(obj):
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.select_set(False)


def bevel(obj, width=0.05, segments=3):
    mod = obj.modifiers.new("soft edges", "BEVEL")
    mod.width = width
    mod.segments = segments
    mod.limit_method = "ANGLE"
    mod.angle_limit = math.radians(35)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)


def box(name, loc, size, mat, bevel_width=0.04, rot=(0, 0, 0), smooth=False):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc, rotation=rot)
    obj = bpy.context.object
    obj.name = name
    obj.scale = size
    apply_scale(obj)
    if bevel_width > 0:
        bevel(obj, bevel_width, 3)
    assign(obj, mat)
    if smooth:
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.shade_smooth()
    return obj


def sphere(name, loc, size, mat, segments=48, rings=24):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, radius=1.0, location=loc)
    obj = bpy.context.object
    obj.name = name
    obj.scale = size
    apply_scale(obj)
    assign(obj, mat)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.shade_smooth()
    return obj


def cylinder(name, loc, radius, depth, mat, rot=(0, 0, 0), vertices=48, smooth=True):
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=vertices, radius=radius, depth=depth, location=loc, rotation=rot
    )
    obj = bpy.context.object
    obj.name = name
    assign(obj, mat)
    if smooth:
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.shade_smooth()
    return obj


def torus(name, loc, major, minor, mat, rot=(0, 0, 0), major_segments=48, minor_segments=12):
    bpy.ops.mesh.primitive_torus_add(
        major_radius=major,
        minor_radius=minor,
        major_segments=major_segments,
        minor_segments=minor_segments,
        location=loc,
        rotation=rot,
    )
    obj = bpy.context.object
    obj.name = name
    assign(obj, mat)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.shade_smooth()
    return obj


def cone(name, loc, r1, r2, depth, mat, rot=(0, 0, 0), vertices=48):
    bpy.ops.mesh.primitive_cone_add(
        vertices=vertices, radius1=r1, radius2=r2, depth=depth, location=loc, rotation=rot
    )
    obj = bpy.context.object
    obj.name = name
    assign(obj, mat)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.shade_smooth()
    return obj


def build_drone():
    hull = make_material("HullWhite", (0.72, 0.76, 0.80), metallic=0.35, roughness=0.30)
    graphite = make_material("Graphite", (0.045, 0.055, 0.070), metallic=0.65, roughness=0.35)
    metal = make_material("MetalDark", (0.16, 0.18, 0.21), metallic=0.85, roughness=0.22)
    orange = make_material("AccentOrange", (0.95, 0.26, 0.035), metallic=0.20, roughness=0.28)
    glass = make_material("CanopyBlue", (0.010, 0.055, 0.090), metallic=0.75, roughness=0.06)
    cyan = make_material(
        "LightCyan", (0.02, 0.75, 0.88), metallic=0.0, roughness=0.25,
        emission=(0.03, 0.85, 1.0), emission_strength=4.0,
    )

    # Main lifting body: a flattened, rounded hull with a separate lower pod.
    box("Hull_Main", (0, 0.05, 0.78), (1.12, 1.55, 0.34), hull, 0.16)
    box("Hull_Lower", (0, 0.10, 0.59), (0.86, 1.18, 0.20), graphite, 0.10)
    box("Spine", (0, 0.10, 0.97), (0.38, 1.20, 0.075), metal, 0.035)

    # Layered nose with an orange bumper and a recessed sensor eye.
    box("Nose_Upper", (0, -1.32, 0.80), (0.82, 0.52, 0.25), hull, 0.13)
    box("Nose_Bumper", (0, -1.58, 0.74), (0.72, 0.14, 0.14), orange, 0.05)
    cylinder("Nose_Sensor", (0, -1.58, 0.83), 0.13, 0.16, graphite, rot=(math.radians(90), 0, 0))
    sphere("Nose_Lens", (0, -1.67, 0.83), (0.085, 0.035, 0.085), cyan)

    # Cockpit canopy.
    sphere("Canopy", (0, -0.26, 1.02), (0.52, 0.78, 0.24), glass)
    box("Canopy_Trim", (0, -0.26, 0.94), (1.00, 1.42, 0.055), graphite, 0.025)

    # Four diagonal arms, each with a ducted rotor.
    arm_angles = [45, 135, 225, 315]
    for i, deg in enumerate(arm_angles):
        a = math.radians(deg)
        direction = Vector((math.cos(a), math.sin(a), 0.0))
        center = Vector((0, 0.0, 0.79)) + direction * 0.98
        box(
            "Arm_%d" % i,
            tuple(center),
            (1.45, 0.20, 0.14),
            graphite,
            0.055,
            rot=(0, 0, a),
        )
        # Orange stripe along each arm.
        stripe_center = Vector((0, 0.0, 0.875)) + direction * 0.76
        box(
            "Arm_Stripe_%d" % i,
            tuple(stripe_center),
            (0.72, 0.075, 0.022),
            orange,
            0.008,
            rot=(0, 0, a),
        )

        rotor_xy = Vector((0, 0.0, 0.0)) + direction * 1.55
        rotor_z = 0.83
        torus(
            "Rotor_Duct_%d" % i,
            (rotor_xy.x, rotor_xy.y, rotor_z),
            0.37,
            0.052,
            metal,
            major_segments=56,
            minor_segments=14,
        )
        torus(
            "Rotor_Ring_%d" % i,
            (rotor_xy.x, rotor_xy.y, rotor_z + 0.045),
            0.31,
            0.013,
            orange,
            major_segments=56,
            minor_segments=8,
        )
        cylinder(
            "Rotor_Hub_%d" % i,
            (rotor_xy.x, rotor_xy.y, rotor_z + 0.02),
            0.075,
            0.11,
            metal,
            vertices=32,
        )
        for blade in range(3):
            blade_angle = a + blade * math.tau / 3.0
            bx = rotor_xy.x + math.cos(blade_angle) * 0.17
            by = rotor_xy.y + math.sin(blade_angle) * 0.17
            box(
                "Rotor_Blade_%d_%d" % (i, blade),
                (bx, by, rotor_z + 0.06),
                (0.34, 0.055, 0.014),
                graphite,
                0.012,
                rot=(0, 0, blade_angle),
                smooth=True,
            )
        # Navigation emitter on the outside of the duct.
        light_pos = rotor_xy + direction * 0.39
        sphere(
            "Nav_Light_%d" % i,
            (light_pos.x, light_pos.y, rotor_z + 0.03),
            (0.042, 0.042, 0.042),
            orange if i in (0, 2) else cyan,
            segments=20,
            rings=12,
        )

    # Twin rear thrusters.
    for side in (-1, 1):
        cylinder(
            "Thruster_%s" % side,
            (0.38 * side, 1.38, 0.79),
            0.20,
            0.44,
            metal,
            rot=(math.radians(90), 0, 0),
        )
        cylinder(
            "Thruster_Glow_%s" % side,
            (0.38 * side, 1.61, 0.79),
            0.135,
            0.025,
            cyan,
            rot=(math.radians(90), 0, 0),
        )
        torus(
            "Thruster_Rim_%s" % side,
            (0.38 * side, 1.61, 0.79),
            0.175,
            0.025,
            orange,
            rot=(math.radians(90), 0, 0),
            major_segments=40,
            minor_segments=8,
        )

    # Under-slung sensor turret.
    cylinder("Turret_Base", (0, 0.18, 0.49), 0.24, 0.16, graphite, vertices=40)
    sphere("Turret_Eye", (0, 0.18, 0.40), (0.15, 0.15, 0.13), metal)
    sphere("Turret_Lens", (0, 0.18, 0.335), (0.075, 0.075, 0.045), cyan, segments=28, rings=16)

    # Slim antenna pair at the rear.
    for side in (-1, 1):
        cylinder(
            "Antenna_%s" % side,
            (0.62 * side, 0.88, 1.18),
            0.018,
            0.56,
            metal,
            rot=(math.radians(12), 0, math.radians(-8 * side)),
            vertices=12,
        )
        sphere("Antenna_Tip_%s" % side, (0.66 * side, 0.98, 1.45), (0.035, 0.035, 0.035), orange, 16, 10)

    # Small armour plates on the hull sides.
    for side in (-1, 1):
        box("Side_Plate_%s" % side, (0.575 * side, 0.24, 0.77), (0.09, 0.72, 0.22), hull, 0.035)
        box("Side_Stripe_%s" % side, (0.623 * side, -0.36, 0.77), (0.018, 0.30, 0.085), orange, 0.005)


def export_model():
    bpy.ops.object.select_all(action="DESELECT")
    mesh_objects = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    for obj in mesh_objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = mesh_objects[0]

    kwargs = dict(
        filepath=MODEL_PATH,
        export_selected_objects=True,
        apply_modifiers=True,
        export_materials=True,
        export_triangulated_mesh=True,
        export_normals=True,
        export_uv=False,
        forward_axis="NEGATIVE_Z",
        up_axis="Y",
    )
    bpy.ops.wm.obj_export(**kwargs)


def setup_preview():
    world = bpy.context.scene.world
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs[0].default_value = (0.015, 0.020, 0.030, 1.0)
    bg.inputs[1].default_value = 0.45

    bpy.ops.mesh.primitive_plane_add(size=30, location=(0, 0, -0.02))
    floor = bpy.context.object
    floor.name = "PreviewFloor"
    floor_mat = make_material("PreviewFloor", (0.035, 0.045, 0.060), metallic=0.0, roughness=0.62)
    assign(floor, floor_mat)

    bpy.ops.object.light_add(type="AREA", location=(3.8, -4.5, 5.5))
    key = bpy.context.object
    key.data.energy = 1100
    key.data.shape = "DISK"
    key.data.size = 5.0
    key.data.color = (1.0, 0.86, 0.72)
    key.rotation_euler = (math.radians(38), 0, math.radians(38))

    bpy.ops.object.light_add(type="AREA", location=(-4.0, 1.5, 3.2))
    fill = bpy.context.object
    fill.data.energy = 800
    fill.data.size = 4.0
    fill.data.color = (0.35, 0.62, 1.0)
    fill.rotation_euler = (math.radians(65), 0, math.radians(-105))

    bpy.ops.object.light_add(type="AREA", location=(0.0, 5.0, 2.2))
    rim = bpy.context.object
    rim.data.energy = 900
    rim.data.size = 3.0
    rim.data.color = (0.20, 0.80, 1.0)
    rim.rotation_euler = (math.radians(75), 0, math.radians(180))

    bpy.ops.object.camera_add(location=(4.8, -6.5, 3.9))
    cam = bpy.context.object
    cam.data.lens = 58
    target = Vector((0, 0, 0.72))
    direction = target - cam.location
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.camera = cam

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = 1000
    scene.render.resolution_y = 760
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = PREVIEW_PATH
    scene.render.film_transparent = False
    scene.view_settings.look = "AgX - Medium High Contrast"
    bpy.ops.render.render(write_still=True)


reset_scene()
build_drone()
export_model()
setup_preview()
print("OBJ written:", MODEL_PATH)
print("MTL written:", os.path.splitext(MODEL_PATH)[0] + ".mtl")
print("Preview written:", PREVIEW_PATH)
