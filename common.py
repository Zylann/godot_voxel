import glob

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
    if env["platform"] == "windows":
        # When compiling SQLite with Godot on Windows with MSVC, it produces the following warning:
        # `sqlite3.c(42754): warning C4996: 'GetVersionExA': was declared deprecated `
        # To fix it, let's indicate to SQLite it should not use this function, even if it is available.
        # https://stackoverflow.com/questions/20031597/error-c4996-received-when-compiling-sqlite-c-in-visual-studio-2013
        env.Append(CPPDEFINES={"SQLITE_WIN32_GETVERSIONEX": 0})

    sources = [
        "constants/*.cpp",

        "meshers/blocky/*.cpp",
        "meshers/transvoxel/*.cpp",
        "meshers/dmc/*.cpp",
        "meshers/cubes/*.cpp",
        "meshers/*.cpp",

        "streams/*.cpp",
        "streams/sqlite/*.cpp",
        "streams/region/*.cpp",
        "streams/vox/*.cpp",

        "storage/*.cpp",

        "generators/*.cpp",
        "generators/graph/*.cpp",
        "generators/simple/*.cpp",

        "modifiers/*.cpp",
        "modifiers/godot/*.cpp",

        "terrain/*.cpp",
        "terrain/instancing/*.cpp",
        "terrain/fixed_lod/*.cpp",
        "terrain/variable_lod/*.cpp",

        "engine/*.cpp",
        "edition/*.cpp",
        "shaders/*.cpp",

        "register_types.cpp",

        # Utility

        "util/*.cpp",
        "util/math/*.cpp",
        "util/noise/fast_noise_lite/*.cpp",
        "util/noise/gd_noise_range.cpp",
        "util/thread/thread.cpp",
        "util/tasks/*.cpp",
        "util/tasks/godot/*.cpp",

        "util/godot/classes/concave_polygon_shape_3d.cpp",
        "util/godot/classes/geometry_2d.cpp",
        "util/godot/classes/mesh.cpp",
        "util/godot/classes/multimesh.cpp",
        "util/godot/classes/node.cpp",
        "util/godot/classes/object.cpp",
        "util/godot/classes/project_settings.cpp",
        "util/godot/classes/rendering_device.cpp",
        "util/godot/classes/rendering_server.cpp",
        "util/godot/classes/resource_loader.cpp",
        "util/godot/classes/shader.cpp",

        "util/godot/core/string.cpp",
        "util/godot/core/variant.cpp",

        "util/godot/direct_mesh_instance.cpp",
        "util/godot/direct_multimesh_instance.cpp",
        "util/godot/direct_static_body.cpp",
        "util/godot/shader_material_pool.cpp",
        "util/godot/funcs.cpp",

        # Thirdparty

        "thirdparty/lz4/*.c",
        # "thirdparty/sqlite/*.c",
        "thirdparty/meshoptimizer/*.cpp"
    ]

    if is_editor_build:
        sources += [
            "editor/*.cpp",
            "editor/terrain/*.cpp",
            "editor/fast_noise_lite/*.cpp",
            "editor/vox/*.cpp",
            "editor/instancer/*.cpp",
            "editor/instance_library/*.cpp",
            "editor/mesh_sdf/*.cpp",
            "editor/graph/*.cpp",

            "util/godot/classes/editor_plugin.cpp",
            "util/godot/classes/editor_import_plugin.cpp",
            "util/godot/classes/editor_inspector_plugin.cpp",
            "util/godot/classes/editor_property.cpp",
            "util/godot/classes/graph_edit.cpp", # Not editor-only, but only used in editor for now
        ]

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

