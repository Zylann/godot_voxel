#include "voxel_provider_test.h"
#include "voxel_map.h"

VARIANT_ENUM_CAST(VoxelProviderTest::Mode)

VoxelProviderTest::VoxelProviderTest() {
	_mode = MODE_FLAT;
	_voxel_type = 1;
	_pattern_size = Vector3i(10, 10, 10);
}

void VoxelProviderTest::set_mode(Mode mode) {
	_mode = mode;
}

void VoxelProviderTest::set_voxel_type(int t) {
	_voxel_type = t;
}

int VoxelProviderTest::get_voxel_type() const {
	return _voxel_type;
}

void VoxelProviderTest::set_pattern_size(Vector3i size) {
	ERR_FAIL_COND(size.x < 1 || size.y < 1 || size.z < 1);
	_pattern_size = size;
}

void VoxelProviderTest::set_pattern_offset(Vector3i offset) {
	_pattern_offset = offset;
}

void VoxelProviderTest::emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i block_pos) {
	ERR_FAIL_COND(out_buffer.is_null());

	switch (_mode) {

		case MODE_FLAT:
			generate_block_flat(**out_buffer, block_pos);
			break;

		case MODE_WAVES:
			generate_block_waves(**out_buffer, block_pos);
			break;
	}
}

void VoxelProviderTest::generate_block_flat(VoxelBuffer &out_buffer, Vector3i block_pos) {

	// TODO Don't expect a block pos, but a voxel pos!
	Vector3i size = out_buffer.get_size();
	Vector3i origin = VoxelMap::block_to_voxel(block_pos);

	int rh = _pattern_offset.y - origin.y;
	if (rh > size.y)
		rh = size.y;

	for (int rz = 0; rz < size.z; ++rz) {
		for (int rx = 0; rx < size.x; ++rx) {
			for (int ry = 0; ry < rh; ++ry) {
				out_buffer.set_voxel(_voxel_type, rx, ry, rz, 0);
			}
		}
	}
}

void VoxelProviderTest::generate_block_waves(VoxelBuffer &out_buffer, Vector3i block_pos) {

	Vector3i size = out_buffer.get_size();
	Vector3i origin = VoxelMap::block_to_voxel(block_pos) + _pattern_offset;
	float amplitude = static_cast<float>(_pattern_size.y);
	float period_x = 1.f / static_cast<float>(_pattern_size.x);
	float period_z = 1.f / static_cast<float>(_pattern_size.z);

	for (int rz = 0; rz < size.z; ++rz) {
		for (int rx = 0; rx < size.x; ++rx) {

			float x = origin.x + rx;
			float z = origin.z + rz;

			int h = _pattern_offset.y + amplitude * (Math::cos(x * period_x) + Math::sin(z * period_z));
			int rh = h - origin.y;
			if (rh > size.y)
				rh = size.y;

			for (int ry = 0; ry < rh; ++ry) {
				out_buffer.set_voxel(_voxel_type, rx, ry, rz, 0);
			}
		}
	}
}

void VoxelProviderTest::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &VoxelProviderTest::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &VoxelProviderTest::get_mode);

	ClassDB::bind_method(D_METHOD("set_voxel_type", "id"), &VoxelProviderTest::set_voxel_type);
	ClassDB::bind_method(D_METHOD("get_voxel_type"), &VoxelProviderTest::get_voxel_type);

	ClassDB::bind_method(D_METHOD("set_pattern_size", "size"), &VoxelProviderTest::_set_pattern_size);
	ClassDB::bind_method(D_METHOD("get_pattern_size"), &VoxelProviderTest::_get_pattern_size);

	ClassDB::bind_method(D_METHOD("set_pattern_offset", "offset"), &VoxelProviderTest::_set_pattern_offset);
	ClassDB::bind_method(D_METHOD("get_pattern_offset"), &VoxelProviderTest::_get_pattern_offset);

	BIND_CONSTANT(MODE_FLAT);
	BIND_CONSTANT(MODE_WAVES);
}
