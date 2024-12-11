#ifndef ZN_GODOT_BUTTON_H
#define ZN_GODOT_BUTTON_H

#if defined(ZN_GODOT)
#include <core/version.h>
#include <scene/gui/button.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/button.hpp>
using namespace godot;
#endif

namespace zylann::godot {

inline void set_button_icon(Button &button, Ref<Texture2D> icon) {
#if defined(ZN_GODOT)

#if VERSION_MAJOR == 4 && VERSION_MINOR <= 3
	button.set_icon(icon);
#else
	button.set_button_icon(icon);
#endif

#elif defined(ZN_GODOT_EXTENSION)
	button.set_button_icon(icon);
#endif
}

} // namespace zylann::godot

#endif // ZN_GODOT_BUTTON_H
