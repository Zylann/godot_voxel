#ifndef ZN_GODOT_RECT2I_H
#define ZN_GODOT_RECT2I_H

#if defined(ZN_GODOT)
#include <core/math/rect2i.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/rect2i.hpp>
using namespace godot;
#endif

#include "../string/std_stringstream.h"

namespace zylann {

StdStringStream &operator<<(StdStringStream &ss, const Rect2i &box);

} // namespace zylann

#endif // ZN_GODOT_RECT2I_H
