#ifndef ZN_GODOT_EDITOR_HELP_H
#define ZN_GODOT_EDITOR_HELP_H

#if defined(ZN_GODOT)
#include "editor/editor_help.h"
#elif defined(ZN_GODOT_EXTENSION)
#include "../core/string.h"
using namespace godot;
#endif

namespace zylann::godot {
namespace EditorHelpUtility {

String get_method_description(const String &class_name, const String &method_name);

} // namespace EditorHelpUtility
} // namespace zylann::godot

#endif // ZN_GODOT_EDITOR_HELP_H
