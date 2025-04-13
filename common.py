import glob


def register_scons_options(env, is_extension):
    from SCons.Script import BoolVariable, Variables, Help

    env_vars = Variables()

    env_vars.Add(BoolVariable("voxel_tests", 
        "Build with tests for the voxel module, which will run on startup of the engine", False))
    
    env_vars.Add(BoolVariable("voxel_smooth_meshing", "Build with smooth voxels meshing support", True))
    env_vars.Add(BoolVariable("voxel_modifiers", "Build with experimental modifiers support", True))
    env_vars.Add(BoolVariable("voxel_sqlite", "Build with SQLite save stream support", True))
    env_vars.Add(BoolVariable("voxel_instancer", "Build with VoxelInstancer support", True))
    env_vars.Add(BoolVariable("voxel_gpu", "Build with GPU compute support", True))
    env_vars.Add(BoolVariable("voxel_basic_generators", "Build with basic/example generators", True))
    env_vars.Add(BoolVariable("voxel_mesh_sdf", "Build with mesh voxelization support", True))
    env_vars.Add(BoolVariable("voxel_vox", "Build with support for loading .vox files", True))
    
    if not is_extension:
        env_vars.Add(BoolVariable("tracy", "Build with enabled Tracy Profiler integration", False))
        env_vars.Add(BoolVariable("voxel_fast_noise_2", "Build FastNoise2 support (x86-only)", True))

    env_vars.Update(env)

    Help(env_vars.GenerateHelpText(env))


# Gets sources and configurations that are common to compiling as a module and an extension.
# For module-specific configuration, see `SCsub`.
# For extension-specific configuration, see `SConstruct`.
def get_sources(env, is_editor_build):
    env.Append(CPPPATH=["."])

    env.Append(CPPDEFINES=[
        # See https://github.com/zeux/meshoptimizer/issues/311
        "MESHOPTIMIZER_ZYLANN_NEVER_COLLAPSE_BORDERS",
        # Because of the above, the MeshOptimizer library in this module is different to an official one.
        # Godot 4 includes an official version, which means they would both conflict at linking time.
        # To prevent this clash we wrap the entire library within an additional namespace.
        # This should be solved either by solving issue #311 or by porting the module to a dynamic library (GDExtension).
        "MESHOPTIMIZER_ZYLANN_WRAP_LIBRARY_IN_NAMESPACE",
    ])
    
    tests_enabled = env["voxel_tests"]
    smoosh_meshing_enabled = env["voxel_smooth_meshing"]
    modifiers_enabled = env["voxel_modifiers"]
    sqlite_enabled = env["voxel_sqlite"]
    instancer_enabled = env["voxel_instancer"]
    gpu_enabled = env["voxel_gpu"]
    basic_generators_enabled = env["voxel_basic_generators"]
    voxel_mesh_sdf_enabled = env["voxel_mesh_sdf"]
    voxel_vox_enabled = env["voxel_vox"]

    if not smoosh_meshing_enabled:
        modifiers_enabled = False
    
    if not voxel_mesh_sdf_enabled:
        modifiers_enabled = False
    
    sources = [
        "constants/*.cpp",

        "meshers/blocky/*.cpp",
        "meshers/blocky/types/*.cpp",
        "meshers/cubes/*.cpp",
        "meshers/*.cpp",

        "streams/*.cpp",
        "streams/region/*.cpp",

        "storage/*.cpp",
        "storage/metadata/*.cpp",

        "generators/generate_block_task.cpp",
        "generators/voxel_generator_script.cpp",
        "generators/voxel_generator.cpp",

        "generators/graph/*.cpp",
        "generators/multipass/*.cpp",

        "terrain/*.cpp",
        "terrain/fixed_lod/*.cpp",
        "terrain/variable_lod/*.cpp",

        "engine/*.cpp",

        "edition/floating_chunks.cpp",
        "edition/funcs.cpp",
        "edition/raycast.cpp",
        "edition/voxel_raycast_result.cpp",
        "edition/voxel_tool_buffer.cpp",
        "edition/voxel_tool_lod_terrain.cpp",
        "edition/voxel_tool_terrain.cpp",
        "edition/voxel_tool.cpp",

        "shaders/*.cpp",

        "register_types.cpp",

        # Utility

        "util/*.cpp",
        "util/containers/*.cpp",
        "util/math/*.cpp",
        "util/memory/*.cpp",
        "util/noise/fast_noise_lite/*.cpp",
        "util/noise/gd_noise_range.cpp",
        "util/noise/spot_noise_gd.cpp",
        "util/string/*.cpp",
        "util/thread/thread.cpp",
        "util/thread/spatial_lock_2d.cpp",
        "util/thread/spatial_lock_3d.cpp",
        "util/tasks/*.cpp",
        "util/tasks/godot/*.cpp",

        "util/godot/classes/array_mesh.cpp",
        "util/godot/classes/concave_polygon_shape_3d.cpp",
        "util/godot/classes/geometry_2d.cpp",
        "util/godot/classes/geometry_instance_3d.cpp",
        "util/godot/classes/input_event_key.cpp",
        "util/godot/classes/image_texture_3d.cpp",
        "util/godot/classes/material.cpp",
        "util/godot/classes/mesh.cpp",
        "util/godot/classes/multimesh.cpp",
        "util/godot/classes/node.cpp",
        "util/godot/classes/object.cpp",
        "util/godot/classes/project_settings.cpp",
        "util/godot/classes/rendering_device.cpp",
        "util/godot/classes/rendering_server.cpp",
        "util/godot/classes/resource_loader.cpp",
        "util/godot/classes/shader.cpp",
        "util/godot/classes/shape_3d.cpp",

        "util/godot/core/aabb.cpp",
        "util/godot/core/string.cpp",
        "util/godot/core/variant.cpp",
        "util/godot/core/packed_arrays.cpp",
        "util/godot/core/rect2i.cpp",

        "util/godot/direct_mesh_instance.cpp",
        "util/godot/direct_multimesh_instance.cpp",
        "util/godot/direct_static_body.cpp",
        "util/godot/file_utils.cpp",
        "util/godot/shader_material_pool.cpp",

        "util/io/*.cpp",

        # Thirdparty

        "thirdparty/lz4/*.c",
        # "thirdparty/sqlite/*.c",
    ]

    if is_editor_build:
        sources += [
            "editor/*.cpp",
            "editor/terrain/*.cpp",
            "editor/fast_noise_lite/*.cpp",
            "editor/spot_noise/*.cpp",
            "editor/graph/*.cpp",
            "editor/blocky_library/*.cpp",
            "editor/blocky_library/types/*.cpp",
            "editor/multipass/*.cpp",

            "util/godot/debug_renderer.cpp",
            "util/godot/check_ref_ownership.cpp",

            "util/godot/classes/editor_plugin.cpp",
            "util/godot/classes/editor_import_plugin.cpp",
            "util/godot/classes/editor_inspector_plugin.cpp",
            "util/godot/classes/editor_property.cpp",
            "util/godot/classes/editor_settings.cpp",
            "util/godot/classes/graph_edit.cpp", # Not editor-only, but only used in editor for now
            "util/godot/classes/graph_node.cpp" # Not editor-only, but only used in editor for now
        ]

    if tests_enabled:
        env.Append(CPPDEFINES={"VOXEL_TESTS": 1})

        sources += [
            "util/testing/*.cpp",

            "tests/*.cpp",
            "tests/util/*.cpp",

            "tests/voxel/test_block_serializer.cpp",
            "tests/voxel/test_curve_range.cpp",
            "tests/voxel/test_edition_funcs.cpp",
            "tests/voxel/test_octree.cpp",
            "tests/voxel/test_raycast.cpp",
            "tests/voxel/test_region_file.cpp",
            "tests/voxel/test_storage_funcs.cpp",
            "tests/voxel/test_util.cpp",
            "tests/voxel/test_voxel_buffer.cpp",
            "tests/voxel/test_voxel_data_map.cpp",
            "tests/voxel/test_voxel_graph.cpp",
            "tests/voxel/test_voxel_instancer.cpp",
            "tests/voxel/test_voxel_mesher_cubes.cpp",
        ]

    if smoosh_meshing_enabled:
        env.Append(CPPDEFINES={"VOXEL_ENABLE_SMOOTH_MESHING": 1})

        sources += [
            "meshers/transvoxel/*.cpp",
            
            "engine/detail_rendering/detail_rendering.cpp",
            "engine/detail_rendering/render_detail_texture_task.cpp",

            "thirdparty/meshoptimizer/*.cpp"
        ]

        if gpu_enabled:
            sources += ["engine/detail_rendering/render_detail_texture_gpu_task.cpp"]

            if tests_enabled:
                sources += ["tests/voxel/test_detail_rendering_gpu.cpp"]
        
    if modifiers_enabled:
        env.Append(CPPDEFINES={"VOXEL_ENABLE_MODIFIERS": 1})
        sources += [
            "modifiers/*.cpp",
            "modifiers/godot/*.cpp",
        ]
    
    if sqlite_enabled:
        env.Append(CPPDEFINES={"VOXEL_ENABLE_SQLITE": 1})

        if env["platform"] == "windows":
            # When compiling SQLite with Godot on Windows with MSVC, it produces the following warning:
            # `sqlite3.c(42754): warning C4996: 'GetVersionExA': was declared deprecated `
            # To fix it, let's indicate to SQLite it should not use this function, even if it is available.
            # https://stackoverflow.com/questions/20031597/error-c4996-received-when-compiling-sqlite-c-in-visual-studio-2013
            env.Append(CPPDEFINES={"SQLITE_WIN32_GETVERSIONEX": 0})
        
        sources += ["streams/sqlite/*.cpp"]
        
        if tests_enabled:
            sources += ["tests/voxel/test_stream_sqlite.cpp"]
    
    if instancer_enabled:
        env.Append(CPPDEFINES={"VOXEL_ENABLE_INSTANCER": 1})

        sources += [
            "terrain/instancing/*.cpp",
        ]

        if is_editor_build:
            sources += [
                "editor/instancer/*.cpp",
                "editor/instance_library/*.cpp",
            ]

    if gpu_enabled:
        env.Append(CPPDEFINES={"VOXEL_ENABLE_GPU": 1})

        sources += [
            "engine/gpu/*.cpp",
            "generators/generate_block_gpu_task.cpp",
        ]
    
    if basic_generators_enabled:
        env.Append(CPPDEFINES={"VOXEL_ENABLE_BASIC_GENERATORS": 1})
        
        sources += [
            "generators/simple/*.cpp",
        ]

    if voxel_mesh_sdf_enabled:
        env.Append(CPPDEFINES={"VOXEL_ENABLE_MESH_SDF": 1})

        sources += [
            "edition/voxel_mesh_sdf_gd.cpp",
            "edition/mesh_sdf.cpp",
        ]

        if is_editor_build:
            sources += ["editor/mesh_sdf/*.cpp"]
        
        if tests_enabled:
            sources += ["tests/voxel/test_mesh_sdf.cpp"]

    if voxel_vox_enabled:
        env.Append(CPPDEFINES={"VOXEL_ENABLE_VOX": 1})

        sources += ["streams/vox/*.cpp"]

        if is_editor_build:
            sources += ["editor/vox/*.cpp"]

    def process_glob_paths(p_sources):
        out = []
        for path in p_sources:
            if '*' in path:
                paths = glob.glob(path)
                out += paths
            else:
                out.append(path)
        return out

    sources = process_glob_paths(sources)

    return sources

