#ifndef ZN_GODOT_IMAGE_H
#define ZN_GODOT_IMAGE_H

#if defined(ZN_GODOT)
#include <core/io/image.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/image.hpp>
using namespace godot;
#endif

namespace zylann::godot {

inline Ref<Image> create_empty_image(
		const int width,
		const int height,
		const bool mipmaps,
		const Image::Format format
) {
#if defined(ZN_GODOT)
	return Image::create_empty(width, height, mipmaps, format);
#elif defined(ZN_GODOT_EXTENSION)
	return Image::create(width, height, mipmaps, format);
#endif
}

// Not exposed by Godot but present in core. It exists as a method but sometimes you don' want to create an instance for
// that.
inline bool is_format_compressed(const Image::Format p_format) {
	return p_format > Image::FORMAT_RGBE9995;
}

} // namespace zylann::godot

#endif // ZN_GODOT_IMAGE_H
