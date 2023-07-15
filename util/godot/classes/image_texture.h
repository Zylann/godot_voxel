#ifndef ZN_GODOT_IMAGE_TEXTURE_H
#define ZN_GODOT_IMAGE_TEXTURE_H

#if defined(ZN_GODOT)
#include <core/version.h>

#if VERSION_MAJOR == 4 && VERSION_MINOR <= 1
#include <scene/resources/texture.h>
#else
#include <scene/resources/image_texture.h>
#endif

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/image_texture.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_IMAGE_TEXTURE_H
