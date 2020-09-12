#ifndef VOX_LOADER_H
#define VOX_LOADER_H

#include "../math/vector3i.h"
#include "../util/fixed_array.h"
#include "../voxel_buffer.h"

namespace vox {

struct Color8 {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
};

struct Data {
	Vector3i size;
	std::vector<uint8_t> color_indexes;
	FixedArray<Color8, 256> palette;
	bool has_palette;
};

// TODO Eventually, will need specialized loaders, because data structures may vary and memory shouldn't be wasted
Error load_vox(const String &fpath, Data &data);

} // namespace vox

// Simple loader for MagicaVoxel
class VoxelVoxLoader : public Reference {
	GDCLASS(VoxelVoxLoader, Reference);

public:
	Error load_from_file(String fpath, Ref<VoxelBuffer> voxels);

private:
	static void _bind_methods();

	vox::Data _data;
};

#endif // VOX_LOADER_H
