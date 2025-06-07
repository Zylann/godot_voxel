#ifndef ZN_GODOT_IMAGE_TEXTURE_3D_H
#define ZN_GODOT_IMAGE_TEXTURE_3D_H

#if defined(ZN_GODOT)
#include <core/version.h>

#if VERSION_MAJOR == 4 && VERSION_MINOR <= 1
#include <scene/resources/texture.h>
#else
#include <scene/resources/image_texture.h>
#endif

#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/image_texture3d.hpp>
using namespace godot;
#endif

#include "../core/typed_array.h"

namespace zylann::godot {

Ref<ImageTexture3D> create_image_texture_3d(
		const Image::Format p_format,
		const Vector3i resolution,
		const bool p_mipmaps,
		const TypedArray<Image> &p_data
);

void update_image_texture_3d(ImageTexture3D &p_texture, const TypedArray<Image> p_data);

} // namespace zylann::godot

#endif // ZN_GODOT_IMAGE_TEXTURE_3D_H
