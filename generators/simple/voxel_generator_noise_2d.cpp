#include "voxel_generator_noise_2d.h"
#include <core/engine.h>

VoxelGeneratorNoise2D::VoxelGeneratorNoise2D() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		// Have one by default in editor
		_noise.instance();
	}
#endif
}

void VoxelGeneratorNoise2D::set_noise(Ref<OpenSimplexNoise> noise) {
	_noise = noise;
}

Ref<OpenSimplexNoise> VoxelGeneratorNoise2D::get_noise() const {
	return _noise;
}

void VoxelGeneratorNoise2D::set_curve(Ref<Curve> curve) {
	_curve = curve;
}

Ref<Curve> VoxelGeneratorNoise2D::get_curve() const {
	return _curve;
}

void VoxelGeneratorNoise2D::generate_block(VoxelBlockRequest &input) {

	ERR_FAIL_COND(_noise.is_null());

	VoxelBuffer &out_buffer = **input.voxel_buffer;
	OpenSimplexNoise &noise = **_noise;

	if (_curve.is_null()) {
		VoxelGeneratorHeightmap::generate(
				out_buffer,
				[&noise](int x, int z) { return 0.5 + 0.5 * noise.get_noise_2d(x, z); },
				input.origin_in_voxels, input.lod);
	} else {
		Curve &curve = **_curve;
		VoxelGeneratorHeightmap::generate(
				out_buffer,
				[&noise, &curve](int x, int z) { return curve.interpolate_baked(0.5 + 0.5 * noise.get_noise_2d(x, z)); },
				input.origin_in_voxels, input.lod);
	}

	out_buffer.compress_uniform_channels();
}

void VoxelGeneratorNoise2D::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_noise", "noise"), &VoxelGeneratorNoise2D::set_noise);
	ClassDB::bind_method(D_METHOD("get_noise"), &VoxelGeneratorNoise2D::get_noise);

	ClassDB::bind_method(D_METHOD("set_curve", "curve"), &VoxelGeneratorNoise2D::set_curve);
	ClassDB::bind_method(D_METHOD("get_curve"), &VoxelGeneratorNoise2D::get_curve);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, "OpenSimplexNoise"), "set_noise", "get_noise");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_curve", "get_curve");
}
