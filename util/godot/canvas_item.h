#ifndef ZN_GODOT_CANVAS_ITEM_H
#define ZN_GODOT_CANVAS_ITEM_H

#if defined(ZN_GODOT)
#include <scene/main/canvas_item.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/canvas_item.hpp>
using namespace godot;
#endif

namespace zylann {

// TODO GDX: `NOTIFICATION_*` are not static, they are supposed to be! Can't be use them in
// `switch`, and they take space in every class instance.
namespace godot_cpp_canvas_item_fix {
#ifdef ZN_GODOT_EXTENSION
#define ZN_GODOT_CANVAS_ITEM_CONSTANT(m_name) godot_cpp_canvas_item_fix::m_name
static const int NOTIFICATION_VISIBILITY_CHANGED = 31;
#else
#define ZN_GODOT_CANVAS_ITEM_CONSTANT(m_name) m_name
#endif // ZN_GODOT_EXTENSION
} // namespace godot_cpp_canvas_item_fix

} // namespace zylann

#endif // ZN_GODOT_CANVAS_ITEM_H
