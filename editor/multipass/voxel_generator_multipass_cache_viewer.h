#ifndef VOXEL_GENERATOR_MULTIPASS_CACHE_VIEWER_H
#define VOXEL_GENERATOR_MULTIPASS_CACHE_VIEWER_H

#include "../../generators/multipass/voxel_generator_multipass_cb.h"
#include "../../util/godot/classes/control.h"
#include "../../util/godot/classes/image.h"
#include "../../util/godot/classes/image_texture.h"

namespace zylann::voxel {

class VoxelGeneratorMultipassCacheViewer : public Control {
	GDCLASS(VoxelGeneratorMultipassCacheViewer, Control)
public:
	VoxelGeneratorMultipassCacheViewer();

	void set_generator(Ref<VoxelGeneratorMultipassCB> generator);

private:
	void _notification(int p_what);
	void process();
	void draw();
	void update_image();

	// When compiling with GodotCpp, `_bind_methods` is not optional.
	static void _bind_methods() {}

	Ref<VoxelGeneratorMultipassCB> _generator;
	std::vector<VoxelGeneratorMultipassCB::DebugColumnState> _debug_column_states;
	Ref<Image> _image;
	Ref<ImageTexture> _texture;
	uint64_t _next_update_time = 0;
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATOR_MULTIPASS_CACHE_VIEWER_H
