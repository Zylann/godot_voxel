#ifndef ZN_GODOT_FONT_H
#define ZN_GODOT_FONT_H

#if defined(ZN_GODOT)
#include <scene/resources/font.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/font.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_FONT_H
