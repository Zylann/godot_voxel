#ifndef ZN_GODOT_IMAGE_H
#define ZN_GODOT_IMAGE_H

#if defined(ZN_GODOT)
#include <core/io/image.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/image.hpp>
using namespace godot;
#endif

namespace zylann {

inline Ref<Image> create_empty_image(int width, int height, bool mipmaps, Image::Format format) {
#if defined(ZN_GODOT)
	return Image::create_empty(width, height, mipmaps, format);
#elif defined(ZN_GODOT_EXTENSION)
	return Image::create(width, height, mipmaps, format);
#endif
}

} // namespace zylann

#endif // ZN_GODOT_IMAGE_H
