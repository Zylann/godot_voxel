#include "voxel_stream_noise_2d.h"

VoxelStreamNoise2D::VoxelStreamNoise2D() {
}

void VoxelStreamNoise2D::set_noise(Ref<OpenSimplexNoise> noise) {
	_noise = noise;
}

Ref<OpenSimplexNoise> VoxelStreamNoise2D::get_noise() const {
	return _noise;
}

void VoxelStreamNoise2D::set_curve(Ref<Curve> curve) {
	_curve = curve;
}

Ref<Curve> VoxelStreamNoise2D::get_curve() const {
	return _curve;
}

void VoxelStreamNoise2D::emerge_block(Ref<VoxelBuffer> p_out_buffer, Vector3i origin_in_voxels, int lod) {

	ERR_FAIL_COND(_noise.is_null());

	VoxelBuffer &out_buffer = **p_out_buffer;
	OpenSimplexNoise &noise = **_noise;

	if (_curve.is_null()) {
		VoxelStreamHeightmap::generate(out_buffer,
				[&noise](int x, int z) { return 0.5 + 0.5 * noise.get_noise_2d(x, z); },
				origin_in_voxels.x, origin_in_voxels.y, origin_in_voxels.z, lod);
	} else {
		Curve &curve = **_curve;
		VoxelStreamHeightmap::generate(out_buffer,
				[&noise, &curve](int x, int z) { return curve.interpolate_baked(0.5 + 0.5 * noise.get_noise_2d(x, z)); },
				origin_in_voxels.x, origin_in_voxels.y, origin_in_voxels.z, lod);
	}

	out_buffer.compress_uniform_channels();
}

void VoxelStreamNoise2D::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_noise", "noise"), &VoxelStreamNoise2D::set_noise);
	ClassDB::bind_method(D_METHOD("get_noise"), &VoxelStreamNoise2D::get_noise);

	ClassDB::bind_method(D_METHOD("set_curve", "curve"), &VoxelStreamNoise2D::set_curve);
	ClassDB::bind_method(D_METHOD("get_curve"), &VoxelStreamNoise2D::get_curve);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, "OpenSimplexNoise"), "set_noise", "get_noise");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_curve", "get_curve");
}
