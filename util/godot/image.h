#ifndef ZN_GODOT_IMAGE_H
#define ZN_GODOT_IMAGE_H

#if defined(ZN_GODOT)
#include <core/io/image.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/image.hpp>
using namespace godot;
#endif

namespace zylann {

inline void create_image_from_data(
		Image &image, Vector2i size, bool mipmaps, Image::Format format, const PackedByteArray &data) {
#if defined(ZN_GODOT)
	image.create(size.x, size.y, mipmaps, format, data);
#elif defined(ZN_GODOT_EXTENSION)
	image.create_from_data(size.x, size.y, mipmaps, format, data);
#endif
}

} // namespace zylann

#endif // ZN_GODOT_IMAGE_H
