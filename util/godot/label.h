#ifndef ZN_GODOT_LABEL_H
#define ZN_GODOT_LABEL_H

#if defined(ZN_GODOT)
#include <scene/gui/label.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/label.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_LABEL
