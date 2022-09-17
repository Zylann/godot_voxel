#ifndef ZN_GODOT_CONTROL_H
#define ZN_GODOT_CONTROL_H

#if defined(ZN_GODOT)
#include <scene/gui/control.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/control.hpp>
using namespace godot;
#endif

namespace zylann {
// TODO GDX: `NOTIFICATION_*` (and maybe other constants) are not static, they are supposed to be! Can't be use them in
// `switch`, and they take space in every class instance.
namespace godot_cpp_fix {
#ifdef ZN_GODOT_EXTENSION
#define ZN_GODOT_CONTROL_CONSTANT(m_name) godot_cpp_fix::m_name
static const int NOTIFICATION_THEME_CHANGED = 45;
#else
#define ZN_GODOT_CONTROL_CONSTANT(m_name) m_name
#endif // ZN_GODOT_EXTENSION
} // namespace godot_cpp_fix
} // namespace zylann

#endif // ZN_GODOT_CONTROL_H
