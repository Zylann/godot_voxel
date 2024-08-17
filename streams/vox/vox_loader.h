#ifndef VOX_LOADER_H
#define VOX_LOADER_H

#include "../../storage/voxel_buffer_gd.h"
#include "../../util/godot/classes/ref_counted.h"

namespace zylann::voxel {

class VoxelColorPalette;

// Simple loader for MagicaVoxel
class VoxelVoxLoader : public RefCounted {
	GDCLASS(VoxelVoxLoader, RefCounted);

public:
	// TODO GDX: Can't bind functions returning a `godot::Error` enum
	static int /*Error*/ load_from_file(
			String fpath,
			Ref<godot::VoxelBuffer> p_voxels,
			Ref<VoxelColorPalette> palette,
			godot::VoxelBuffer::ChannelId dst_channel
	);
	// TODO Have chunked loading for better memory usage
	// TODO Saving

private:
	static void _bind_methods();
};

} // namespace zylann::voxel

#endif // VOX_LOADER_H
