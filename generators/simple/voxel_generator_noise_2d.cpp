#include "voxel_generator_noise_2d.h"
#include <core/engine.h>

VoxelGeneratorNoise2D::VoxelGeneratorNoise2D() {
	_parameters_lock = RWLock::create();
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		// Have one by default in editor
		Ref<OpenSimplexNoise> noise;
		noise.instance();
		set_noise(noise);
	}
#endif
}

VoxelGeneratorNoise2D::~VoxelGeneratorNoise2D() {
	memdelete(_parameters_lock);
}

void VoxelGeneratorNoise2D::set_noise(Ref<OpenSimplexNoise> noise) {
	if (_noise == noise) {
		return;
	}
	_noise = noise;
	Ref<OpenSimplexNoise> copy;
	if (noise.is_valid()) {
		copy = noise->duplicate();
	}
	RWLockWrite wlock(_parameters_lock);
	_parameters.noise = copy;
}

Ref<OpenSimplexNoise> VoxelGeneratorNoise2D::get_noise() const {
	return _noise;
}

void VoxelGeneratorNoise2D::set_curve(Ref<Curve> curve) {
	if (_curve == curve) {
		return;
	}
	_curve = curve;
	RWLockWrite wlock(_parameters_lock);
	if (_curve.is_valid()) {
		_parameters.curve = _curve->duplicate();
		_parameters.curve->bake();
	} else {
		_parameters.curve.unref();
	}
}

Ref<Curve> VoxelGeneratorNoise2D::get_curve() const {
	return _curve;
}

void VoxelGeneratorNoise2D::generate_block(VoxelBlockRequest &input) {
	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	ERR_FAIL_COND(params.noise.is_null());
	OpenSimplexNoise &noise = **params.noise;

	VoxelBuffer &out_buffer = **input.voxel_buffer;

	if (_curve.is_null()) {
		VoxelGeneratorHeightmap::generate(
				out_buffer,
				[&noise](int x, int z) { return 0.5 + 0.5 * noise.get_noise_2d(x, z); },
				input.origin_in_voxels, input.lod);
	} else {
		Curve &curve = **params.curve;
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
