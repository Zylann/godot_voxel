#ifndef ZN_GODOT_BUTTON_H
#define ZN_GODOT_BUTTON_H

#if defined(ZN_GODOT)
#include <scene/gui/button.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/button.hpp>
using namespace godot;
#endif

namespace zylann {

inline void set_button_icon(Button &button, Ref<Texture2D> icon) {
#if defined(ZN_GODOT)
	button.set_icon(icon);
#elif defined(ZN_GODOT_EXTENSION)
	button.set_button_icon(icon);
#endif
}

} // namespace zylann

#endif // ZN_GODOT_BUTTON_H
