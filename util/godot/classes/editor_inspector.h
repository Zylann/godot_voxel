#ifndef ZN_GODOT_EDITOR_INSPECTOR_H
#define ZN_GODOT_EDITOR_INSPECTOR_H

#if defined(ZN_GODOT)

#include "../core/version.h"

#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 4
#include <editor/editor_inspector.h>
#else
#include <editor/inspector/editor_inspector.h>
#endif

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/editor_inspector.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_EDITOR_INSPECTOR_H
