#!/usr/bin/env python

# This is the entry point for SCons to build this engine as a GDExtension.
# To build as a module, see `SCsub`.

import os
import sys

import common
import voxel_version

LIB_NAME = "libvoxel"
BIN_FOLDER = "project/addons/zylann.voxel/bin"

voxel_version.generate_version_header(False)

# TODO Enhancement: not sure how to provide this as a SCons option since we get our environment *by running GodotCpp*...
#env_vars.Add(PathVariable("godot_cpp_path", "Path to the GodotCpp library source code", None, PathVariable.PathIsDir))
# TODO GDX: Have GodotCpp in thirdparty/ eventually
godot_cpp_path = os.environ.get("GODOT_CPP_PATH", "D:/PROJETS/INFO/GODOT/Engine/godot_cpp_fork")

# Dependency on GodotCpp.
# Use the same cross-platform configurations.
# TODO GDX: Make sure this isn't doing too much?
env = SConscript(godot_cpp_path + "/SConstruct")

# TODO GDX: Adding our variables produces a warning when provided.
# "WARNING: Unknown SCons variables were passed and will be ignored"
# This is printed by GodotCpp's SConstruct file, which doesn't recognizes them.
# However they seem to be taken into account in our `SConstruct` file though.
# If such a check should exist, it needs to be HERE, not in GodotCpp.

common.register_scons_options(env, True)

env.Append(CPPDEFINES=[
	# Tell engine-agnostic code we are using Godot Engine as an extension
	"ZN_GODOT_EXTENSION"
])

is_editor_build = (env["target"] == "editor")

sources = common.get_sources(env, is_editor_build)

if env["voxel_sqlite"]:
    # TODO Enhancement: the way SQLite is integrated should not be duplicated between Godot and GodotCpp targets.
    # It cannot be in the common script...
    # Because when compiling with warnings=extra, SQLite produces warnings, so we have to turn them off only for SQLite.
    # But doing this requires specific code from Godot's build system, and no idea what code to use with GodotCpp...
    # Ideally I'd like to use the same code for both targets.
    # FastNoise2 has the same problem!
    sources += [
        "thirdparty/sqlite/sqlite3.c"
    ]

sources += [
	"util/thread/godot_thread_helper.cpp",

	# GodotCpp doesn't come with RandomPCG
	"util/godot/core/pcg.cpp",
	"util/godot/core/random_pcg.cpp"
]

if is_editor_build:
    sources += [
        "util/godot/editor_scale.cpp"
    ]

    try:
        doc_data = env.GodotCPPDocData("doc_data.gen.cpp", source=Glob("doc/classes/*.xml"))
        sources.append(doc_data)
    except AttributeError:
        print("Not including class reference as we're targeting a pre-4.3 baseline.")


if env["platform"] == "macos":
    library = env.SharedLibrary(
        "{}/{}{}.framework/{}{}".format(
            BIN_FOLDER,
            LIB_NAME,
            env['suffix'],
            LIB_NAME,
            env['suffix']
        ),
        source = sources
    )
else:
    library = env.SharedLibrary(
        "{}/{}{}{}".format(
            BIN_FOLDER,
            LIB_NAME,
            env["suffix"],
            env["SHLIBSUFFIX"]
        ),
        source = sources
    )

Default(library)
