#ifndef VOX_LOADER_H
#define VOX_LOADER_H

#include <core/object/ref_counted.h>

class VoxelBuffer;
class VoxelColorPalette;

// Simple loader for MagicaVoxel
class VoxelVoxLoader : public RefCounted {
	GDCLASS(VoxelVoxLoader, RefCounted);

public:
	Error load_from_file(String fpath, Ref<VoxelBuffer> p_voxels, Ref<VoxelColorPalette> palette);
	// TODO Have chunked loading for better memory usage
	// TODO Saving

private:
	static void _bind_methods();
};

#endif // VOX_LOADER_H
