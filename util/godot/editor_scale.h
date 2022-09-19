#ifndef ZN_GODOT_EDITOR_SCALE_H
#define ZN_GODOT_EDITOR_SCALE_H

#if defined(ZN_GODOT)
#include <editor/editor_scale.h>
#elif defined(ZN_GODOT_EXTENSION)
#define EDSCALE zylann::get_editor_scale()
namespace zylann {
float get_editor_scale();
}
#endif

#endif // ZN_GODOT_EDITOR_SCALE_H
