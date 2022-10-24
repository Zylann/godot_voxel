#ifndef ZN_GODOT_IMAGE_H
#define ZN_GODOT_IMAGE_H

#if defined(ZN_GODOT)
#include <core/io/image.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/image.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_IMAGE_H
