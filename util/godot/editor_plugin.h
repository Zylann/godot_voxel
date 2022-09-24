#ifndef ZN_GODOT_EDITOR_PLUGIN_H
#define ZN_GODOT_EDITOR_PLUGIN_H

#if defined(ZN_GODOT)
#include <editor/editor_plugin.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/editor_plugin.hpp>
using namespace godot;
#endif

#ifdef ZN_GODOT_EXTENSION
// TODO GDX: `EditorPlugin::AfterGUIInput` enum is missing from the API
namespace zylann::godot_cpp_fix {
static const int AFTER_GUI_INPUT_PASS = 0;
static const int AFTER_GUI_INPUT_STOP = 1;
static const int AFTER_GUI_INPUT_CUSTOM = 2;
} // namespace zylann::godot_cpp_fix
#endif

#endif // ZN_GODOT_EDITOR_PLUGIN_H
