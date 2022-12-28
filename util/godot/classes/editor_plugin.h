#ifndef ZN_GODOT_EDITOR_PLUGIN_H
#define ZN_GODOT_EDITOR_PLUGIN_H

#if defined(ZN_GODOT)
#include <editor/editor_plugin.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/editor_plugin.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_EDITOR_PLUGIN_H
