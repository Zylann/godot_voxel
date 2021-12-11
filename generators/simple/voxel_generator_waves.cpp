#include "voxel_generator_waves.h"
#include <cmath>

VoxelGeneratorWaves::VoxelGeneratorWaves() {
	_parameters.pattern_size = Vector2(30, 30);
	// This might be a different default value than the base class,
	// because in practice this generator is more discoverable with a small pattern size
	set_height_range(30);
}

VoxelGeneratorWaves::~VoxelGeneratorWaves() {
}

VoxelGenerator::Result VoxelGeneratorWaves::generate_block(VoxelBlockRequest &input) {
	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	VoxelBufferInternal &out_buffer = input.voxel_buffer;
	const Vector2 freq(
			Math_PI / static_cast<float>(params.pattern_size.x),
			Math_PI / static_cast<float>(params.pattern_size.y));
	const Vector2 offset = params.pattern_offset;

	return VoxelGeneratorHeightmap::generate(
			out_buffer,
			[freq, offset](int x, int z) {
				return 0.5 + 0.25 * (Math::cos((x + offset.x) * freq.x) + Math::sin((z + offset.y) * freq.y));
			},
			input.origin_in_voxels, input.lod);
}

Vector2 VoxelGeneratorWaves::get_pattern_size() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.pattern_size;
}

void VoxelGeneratorWaves::set_pattern_size(Vector2 size) {
	RWLockWrite wlock(_parameters_lock);
	size.x = max(size.x, 0.1f);
	size.y = max(size.y, 0.1f);
	_parameters.pattern_size = size;
}

Vector2 VoxelGeneratorWaves::get_pattern_offset() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.pattern_offset;
}

void VoxelGeneratorWaves::set_pattern_offset(Vector2 offset) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.pattern_offset = offset;
}

void VoxelGeneratorWaves::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_pattern_size", "size"), &VoxelGeneratorWaves::set_pattern_size);
	ClassDB::bind_method(D_METHOD("get_pattern_size"), &VoxelGeneratorWaves::get_pattern_size);

	ClassDB::bind_method(D_METHOD("set_pattern_offset", "offset"), &VoxelGeneratorWaves::set_pattern_offset);
	ClassDB::bind_method(D_METHOD("get_pattern_offset"), &VoxelGeneratorWaves::get_pattern_offset);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "pattern_size"), "set_pattern_size", "get_pattern_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "pattern_offset"), "set_pattern_offset", "get_pattern_offset");
}
