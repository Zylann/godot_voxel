#include "voxel_generator_waves.h"
#include "../util/utility.h"
#include <cmath>

VoxelGeneratorWaves::VoxelGeneratorWaves() {
	_pattern_size = Vector2(30, 30);
	set_height_range(30);
}

void VoxelGeneratorWaves::generate_block(VoxelBlockRequest &input) {

	VoxelBuffer &out_buffer = **input.voxel_buffer;
	const Vector2 freq(
			Math_PI / static_cast<float>(_pattern_size.x),
			Math_PI / static_cast<float>(_pattern_size.y));
	const Vector2 offset = _pattern_offset;

	VoxelGeneratorHeightmap::generate(out_buffer,
			[freq, offset](int x, int z) {
				return 0.5 + 0.25 * (Math::cos((x + offset.x) * freq.x) + Math::sin((z + offset.y) * freq.y));
			},
			input.origin_in_voxels, input.lod);
}

void VoxelGeneratorWaves::set_pattern_size(Vector2 size) {
	size.x = max(size.x, 0.1f);
	size.y = max(size.y, 0.1f);
	_pattern_size = size;
}

void VoxelGeneratorWaves::set_pattern_offset(Vector2 offset) {
	_pattern_offset = offset;
}

void VoxelGeneratorWaves::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_pattern_size", "size"), &VoxelGeneratorWaves::set_pattern_size);
	ClassDB::bind_method(D_METHOD("get_pattern_size"), &VoxelGeneratorWaves::get_pattern_size);

	ClassDB::bind_method(D_METHOD("set_pattern_offset", "offset"), &VoxelGeneratorWaves::set_pattern_offset);
	ClassDB::bind_method(D_METHOD("get_pattern_offset"), &VoxelGeneratorWaves::get_pattern_offset);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "pattern_size"), "set_pattern_size", "get_pattern_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "pattern_offset"), "set_pattern_offset", "get_pattern_offset");
}
