#include "image_texture_layered.h"

namespace zylann::godot {

void create_from_images(ImageTextureLayered &tex, const TypedArray<Image> &images) {
#if defined(ZN_GODOT)
	Vector<Ref<Image>> images_vec;
	images_vec.resize(images.size());
	for (int i = 0; i < images.size(); ++i) {
		images_vec.write[i] = images[i];
	}
	tex.create_from_images(images_vec);

#elif defined(ZN_GODOT_EXTENSION)
	tex.create_from_images(images);
#endif
}

} // namespace zylann::godot
