#!/usr/bin/env python
import os
import sys
import glob

# This is the entry point for SCons to build this engine as a GDExtension.
# To build as a module, see `SCsub`.

LIB_NAME = "libvoxel"
BIN_FOLDER = "bin"

# TODO GDX: Share common stuff with `SCsub`

# Dependency on GodotCpp.
# Use the same cross-platform configurations.
# TODO GDX: Make sure this isn't doing too much?
# TODO GDX: Have GodotCpp in thirdparty/ or allow to specify a custom location
env = SConscript("D:/PROJETS/INFO/GODOT/Engine/godot_cpp_fork/SConstruct")

# TODO GDX: Adding our variables produces a warning when provided.
# "WARNING: Unknown SCons variables were passed and will be ignored"
# This is printed by GodotCpp's SConstruct file, which doesn't recognizes them.
# However they seem to be taken into account in our `SConstruct` file though.
# If such a check should exist, it needs to be HERE, not in GodotCpp.

env_vars = Variables()
env_vars.Add(BoolVariable("tools", "Build library with editor tools", True))
env_vars.Add(BoolVariable("voxel_tests", 
    "Build with tests for the voxel module, which will run on startup of the engine", False))
env_vars.Add(BoolVariable("voxel_fast_noise_2", "Build FastNoise2 support (x86-only)", True))
env_vars.Update(env)
Help(env_vars.GenerateHelpText(env))

env.Append(CPPPATH=["."])

env.Append(CPPDEFINES=[
	# See https://github.com/zeux/meshoptimizer/issues/311
	"MESHOPTIMIZER_ZYLANN_NEVER_COLLAPSE_BORDERS",
	# Because of the above, the MeshOptimizer library in this module is different to an official one.
	# Godot 4 includes an official version, which means they would both conflict at linking time.
	# To prevent this clash we wrap the entire library within an additional namespace.
	# This should be solved either by solving issue #311 or by porting the module to a dynamic library (GDExtension).
	"MESHOPTIMIZER_ZYLANN_WRAP_LIBRARY_IN_NAMESPACE",
	# Tell engine-agnostic code we are using Godot Engine as an extension
	"ZN_GODOT_EXTENSION"
])

if env["tools"]:
    # GodotCpp does not define anything regarding editor builds, so we have to define that ourselves.
    # This is the same symbol used in Godot modules.
    env.Append(CPPDEFINES=["TOOLS_ENABLED"])

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

	"terrain/*.cpp",
	"terrain/instancing/*.cpp",
	"terrain/fixed_lod/*.cpp",
	"terrain/variable_lod/*.cpp",

	"engine/*.cpp",
	"edition/*.cpp",

	"register_types_gdx.cpp",

    # Utility

	"util/*.cpp",
	"util/math/*.cpp",
	"util/godot/*.cpp",
	"util/noise/fast_noise_lite/*.cpp",
	"util/noise/gd_noise_range.cpp",
	"util/thread/thread.cpp",
    "util/thread/godot_thread_helper.cpp",
	"util/tasks/*.cpp",
	"util/tasks/godot/*.cpp",

    # Thirdparty

	"thirdparty/lz4/*.c",
	"thirdparty/sqlite/*.c",
	"thirdparty/meshoptimizer/*.cpp"
]

if env["tools"]:
    sources += [
        "editor/*.cpp",
		"editor/terrain/*.cpp",
		"editor/fast_noise_lite/*.cpp"
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

if env["platform"] == "windows":
	# When compiling SQLite with Godot on Windows with MSVC, it produces the following warning:
	# `sqlite3.c(42754): warning C4996: 'GetVersionExA': was declared deprecated `
	# To fix it, let's indicate to SQLite it should not use this function, even if it is available.
	# https://stackoverflow.com/questions/20031597/error-c4996-received-when-compiling-sqlite-c-in-visual-studio-2013
	env.Append(CPPDEFINES={"SQLITE_WIN32_GETVERSIONEX": 0})

# WARNING
# From a C++ developer point of view, the GodotCpp example and `.gdextension` files are confusing what the
# `debug` and `release` targets usually mean with "editor" and "exported projects".
# In this project, we consider `debug` means "unoptimized + debug symbols, big binary",
# while `release` means "optimized, no debug symbols, small binary".
# "editor" and "exported" should be separate concepts, which we had to define ourselves as `tools`.
# But in `.gdextension` files, `debug` actually means "editor", and `release` means "exported".

bin_name = "{}.{}{}{}".format(
    LIB_NAME,
    env["platform"], 
    ".tools." if env["tools"] else ".",
    env["target"],
)

if env["platform"] == "macos":
    library = env.SharedLibrary(
        "{}/{}.framework/{}".format(
            BIN_FOLDER,
            bin_name,
            bin_name
        ),
        source = sources,
    )
else:
    library = env.SharedLibrary(
        "{}/{}.{}{}".format(
            BIN_FOLDER,
            bin_name,
            env["arch_suffix"],
            env["SHLIBSUFFIX"]
        ),
        source = sources,
    )

Default(library)
