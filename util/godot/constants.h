#ifndef ZN_GODOT_CONSTANTS_H
#define ZN_GODOT_CONSTANTS_H

// TODO GDX: `PROPERTY_USAGE_READ_ONLY` is missing from exposed API.

namespace godot {

#if defined(ZN_GODOT)
//#include <core/object/object.h>
//static constexpr int PROPERTY_USAGE_READ_ONLY = ::PROPERTY_USAGE_READ_ONLY;
#elif defined(ZN_GODOT_EXTENSION)
static constexpr int PROPERTY_USAGE_READ_ONLY = 1 << 28;
#endif

} // namespace godot

using namespace godot;

#endif // ZN_GODOT_CONSTANTS_H
