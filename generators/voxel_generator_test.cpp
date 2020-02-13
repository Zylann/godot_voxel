#include "voxel_generator_test.h"

VARIANT_ENUM_CAST(VoxelGeneratorTest::Mode)

VoxelGeneratorTest::VoxelGeneratorTest() {
	_mode = MODE_WAVES;
	_voxel_type = 1;
	_pattern_size = Vector3i(10, 10, 10);
}

void VoxelGeneratorTest::set_mode(Mode mode) {
	ERR_FAIL_INDEX(mode, MODE_COUNT);
	_mode = mode;
}

void VoxelGeneratorTest::set_voxel_type(int t) {
	_voxel_type = t;
}

int VoxelGeneratorTest::get_voxel_type() const {
	return _voxel_type;
}

void VoxelGeneratorTest::set_pattern_size(Vector3i size) {
	ERR_FAIL_COND(size.x < 1 || size.y < 1 || size.z < 1);
	_pattern_size = size;
}

void VoxelGeneratorTest::set_pattern_offset(Vector3i offset) {
	_pattern_offset = offset;
}

void VoxelGeneratorTest::generate_block(VoxelBlockRequest &input) {
	ERR_FAIL_COND(input.voxel_buffer.is_null());

	if (input.lod != 0) {
		// TODO Handle higher lods
		return;
	}

	switch (_mode) {

		case MODE_FLAT:
			generate_block_flat(**input.voxel_buffer, input.origin_in_voxels, input.lod);
			break;

		default:
		case MODE_WAVES:
			generate_block_waves(**input.voxel_buffer, input.origin_in_voxels, input.lod);
			break;
	}
}

void VoxelGeneratorTest::generate_block_flat(VoxelBuffer &out_buffer, Vector3i origin, int lod) {

	// TODO Don't expect a block pos, but a voxel pos!
	Vector3i size = out_buffer.get_size();

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

void VoxelGeneratorTest::generate_block_waves(VoxelBuffer &out_buffer, Vector3i origin, int lod) {

	// TODO Don't expect a block pos, but a voxel pos!
	Vector3i size = out_buffer.get_size();
	//origin += _pattern_offset;

	float amplitude = static_cast<float>(_pattern_size.y);
	float period_x = 1.f / static_cast<float>(_pattern_size.x);
	float period_z = 1.f / static_cast<float>(_pattern_size.z);

	//out_buffer.fill(0, 1); // TRANSVOXEL TEST

	if (origin.y + size.y < Math::floor(_pattern_offset.y - 1.5 * amplitude)) {
		// Everything is ground
		out_buffer.fill(_voxel_type);

	} else if (origin.y > Math::ceil(_pattern_offset.y + 1.5 * amplitude)) {
		// Everything is air
		return;

	} else {
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
					//out_buffer.set_voxel(255, rx, ry, rz, 1); // TRANSVOXEL TEST
				}
			}
		}
	}
}

void VoxelGeneratorTest::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &VoxelGeneratorTest::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &VoxelGeneratorTest::get_mode);

	ClassDB::bind_method(D_METHOD("set_voxel_type", "id"), &VoxelGeneratorTest::set_voxel_type);
	ClassDB::bind_method(D_METHOD("get_voxel_type"), &VoxelGeneratorTest::get_voxel_type);

	ClassDB::bind_method(D_METHOD("set_pattern_size", "size"), &VoxelGeneratorTest::_set_pattern_size);
	ClassDB::bind_method(D_METHOD("get_pattern_size"), &VoxelGeneratorTest::_get_pattern_size);

	ClassDB::bind_method(D_METHOD("set_pattern_offset", "offset"), &VoxelGeneratorTest::_set_pattern_offset);
	ClassDB::bind_method(D_METHOD("get_pattern_offset"), &VoxelGeneratorTest::_get_pattern_offset);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Flat,Waves"), "set_mode", "get_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "voxel_type", PROPERTY_HINT_RANGE, "0,255,1"), "set_voxel_type", "get_voxel_type");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "pattern_size"), "set_pattern_size", "get_pattern_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "pattern_offset"), "set_pattern_offset", "get_pattern_offset");

	BIND_ENUM_CONSTANT(MODE_FLAT);
	BIND_ENUM_CONSTANT(MODE_WAVES);
}
