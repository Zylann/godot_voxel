#ifndef ZN_GODOT_H_SEPARATOR_H
#define ZN_GODOT_H_SEPARATOR_H

#if defined(ZN_GODOT)
#include <scene/gui/separator.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/h_separator.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_H_SEPARATOR_H
