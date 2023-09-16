#ifndef ZN_GODOT_VERSION_H
#define ZN_GODOT_VERSION_H

#if defined(ZN_GODOT)

#include <core/version.h>

// Redefining so we can use the same macros as in extension builds, and I prefer when it's prefixed so it's clear what
// the macro is referring to.
#define GODOT_VERSION_MAJOR VERSION_MAJOR
#define GODOT_VERSION_MINOR VERSION_MINOR

#elif defined(ZN_GODOT_EXTENSION)

// Note, in early versions of GodotCpp, this header might not exist
#include <godot_cpp/core/version.hpp>

#if !defined(GODOT_VERSION_MAJOR)

// We are prior to the version of GodotCpp that had version macros, which was during development of Godot 4.2.
// Assume Godot 4.1, though it's not guaranteed.
#define GODOT_VERSION_MAJOR 4
#define GODOT_VERSION_MINOR 1

#endif

#endif // ZN_GODOT_EXTENSION

#endif // ZN_GODOT_VERSION_H
