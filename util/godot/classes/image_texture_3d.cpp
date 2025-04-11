#include "image_texture_3d.h"
#include "../../profiling.h"

namespace zylann::godot {

Ref<ImageTexture3D> create_image_texture_3d(
		const Image::Format p_format,
		const Vector3i resolution,
		const bool p_mipmaps,
		const TypedArray<Image> &p_data
) {
	ZN_PROFILE_SCOPE();

	Ref<ImageTexture3D> texture;
	texture.instantiate();

#if defined(ZN_GODOT)
	Vector<Ref<Image>> images = to_ref_vector(p_data);
	texture->create(p_format, resolution.x, resolution.y, resolution.z, p_mipmaps, images);

#elif defined(ZN_GODOT_EXTENSION)
	texture->create(p_format, resolution.x, resolution.y, resolution.z, p_mipmaps, p_data);
#endif

	return texture;
}

void update_image_texture_3d(ImageTexture3D &p_texture, const TypedArray<Image> p_data) {
	ZN_PROFILE_SCOPE();

#if defined(ZN_GODOT)
	Vector<Ref<Image>> images = to_ref_vector(p_data);
	p_texture.update(images);

#elif defined(ZN_GODOT_EXTENSION)
	p_texture.update(p_data);
#endif
}

} // namespace zylann::godot
