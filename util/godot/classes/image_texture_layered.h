#ifndef ZN_GODOT_IMAGE_TEXTURE_LAYERED_H
#define ZN_GODOT_IMAGE_TEXTURE_LAYERED_H

#if defined(ZN_GODOT)
#include <scene/resources/image_texture.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/image_texture_layered.hpp>
using namespace godot;
#endif

namespace zylann::godot {

void create_from_images(ImageTextureLayered &tex, const TypedArray<Image> &images);

} // namespace zylann::godot

#endif // ZN_GODOT_IMAGE_TEXTURE_LAYERED_H
