#ifndef ZN_GODOT_VERSION_H
#define ZN_GODOT_VERSION_H

#if defined(ZN_GODOT)

#include <core/version.h>

// In Godot versions prior to 4.5, version macros were mismatching with GodotCpp.
// We define them so we can use the same macros as in extension builds, 
// and I prefer when it's prefixed so it's clear what the macro is referring to.

#ifndef GODOT_VERSION_MAJOR
#define GODOT_VERSION_MAJOR VERSION_MAJOR
#endif

#ifndef GODOT_VERSION_MINOR
#define GODOT_VERSION_MINOR VERSION_MINOR
#endif

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
