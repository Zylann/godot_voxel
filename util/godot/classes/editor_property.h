#ifndef ZN_GODOT_EDITOR_PROPERTY_H
#define ZN_GODOT_EDITOR_PROPERTY_H

#if defined(ZN_GODOT)
#include <editor/editor_inspector.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/editor_property.hpp>
using namespace godot;
#endif

#include "../../span.h"

namespace zylann {

// This obscure method is actually used to get the XYZW tinting colors for controls that expose coordinates.
// In modules, this is `_get_property_colors`, but it is not exposed in GDExtension.
Span<const Color> editor_property_get_colors(EditorProperty &self);

} // namespace zylann

#endif // ZN_GODOT_EDITOR_PROPERTY_H
