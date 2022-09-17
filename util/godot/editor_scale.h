#ifndef ZN_GODOT_EDITOR_SCALE_H
#define ZN_GODOT_EDITOR_SCALE_H

#if defined(ZN_GODOT)
#include <editor/editor_scale.h>
#elif defined(ZN_GODOT_EXTENSION)
// TODO GDX: No `EDSCALE` equivalent for porting editor tools, DPI-awareness will not work
#define EDSCALE 1
#endif

#endif // ZN_GODOT_EDITOR_SCALE_H
