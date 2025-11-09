#ifndef ZN_GODOT_EDITOR_RESOURCE_PICKER_H
#define ZN_GODOT_EDITOR_RESOURCE_PICKER_H

#if defined(ZN_GODOT)

#include "../core/version.h"

#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 4
#include <editor/editor_resource_picker.h>
#else
#include <editor/inspector/editor_resource_picker.h>
#endif

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/editor_resource_picker.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_EDITOR_RESOURCE_PICKER_H
