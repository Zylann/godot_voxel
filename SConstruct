#!/usr/bin/env python

# This is the entry point for SCons to build this engine as a GDExtension.
# To build as a module, see `SCsub`.

import os
import sys

import common

LIB_NAME = "libvoxel"
BIN_FOLDER = "bin"

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
# TODO Share options between module and extension?
env_vars.Add(BoolVariable("voxel_tests", 
    "Build with tests for the voxel module, which will run on startup of the engine", False))
# FastNoise2 is disabled by default, may want to integrate as dynamic library
env_vars.Add(BoolVariable("voxel_fast_noise_2", "Build FastNoise2 support (x86-only)", True))
env_vars.Update(env)
Help(env_vars.GenerateHelpText(env))

env.Append(CPPDEFINES=[
	# Tell engine-agnostic code we are using Godot Engine as an extension
	"ZN_GODOT_EXTENSION"
])

if env["tools"]:
    # GodotCpp does not define anything regarding editor builds, so we have to define that ourselves.
    # This is the same symbol used in Godot modules.
    env.Append(CPPDEFINES=["TOOLS_ENABLED"])

sources = common.get_sources(env)

sources += [
	"util/thread/godot_thread_helper.cpp",

	# GodotCpp doesn't come with RandomPCG
	"util/godot/pcg.cpp",
	"util/godot/random_pcg.cpp"
]

if env["tools"]:
    sources += [
        "util/godot/editor_scale.cpp"
    ]

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
