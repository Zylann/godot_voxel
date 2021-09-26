#include "voxel_generator_noise_2d.h"
#include <core/core_string_names.h>
#include <core/engine.h>

VoxelGeneratorNoise2D::VoxelGeneratorNoise2D() {
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
}

void VoxelGeneratorNoise2D::set_noise(Ref<OpenSimplexNoise> noise) {
	if (_noise == noise) {
		return;
	}
	if (_noise.is_valid()) {
		_noise->disconnect(CoreStringNames::get_singleton()->changed, this, "_on_noise_changed");
	}
	_noise = noise;
	Ref<OpenSimplexNoise> copy;
	if (_noise.is_valid()) {
		_noise->connect(CoreStringNames::get_singleton()->changed, this, "_on_noise_changed");
		// The OpenSimplexNoise resource is not thread-safe so we make a copy of it for use in threads
		copy = _noise->duplicate();
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
	if (_curve.is_valid()) {
		_curve->disconnect(CoreStringNames::get_singleton()->changed, this, "_on_curve_changed");
	}
	_curve = curve;
	RWLockWrite wlock(_parameters_lock);
	if (_curve.is_valid()) {
		_curve->connect(CoreStringNames::get_singleton()->changed, this, "_on_curve_changed");
		// The Curve resource is not thread-safe so we make a copy of it for use in threads
		_parameters.curve = _curve->duplicate();
		_parameters.curve->bake();
	} else {
		_parameters.curve.unref();
	}
}

Ref<Curve> VoxelGeneratorNoise2D::get_curve() const {
	return _curve;
}

VoxelGenerator::Result VoxelGeneratorNoise2D::generate_block(VoxelBlockRequest &input) {
	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	Result result;

	ERR_FAIL_COND_V(params.noise.is_null(), result);
	OpenSimplexNoise &noise = **params.noise;

	VoxelBufferInternal &out_buffer = input.voxel_buffer;

	if (_curve.is_null()) {
		result = VoxelGeneratorHeightmap::generate(
				out_buffer,
				[&noise](int x, int z) { return 0.5 + 0.5 * noise.get_noise_2d(x, z); },
				input.origin_in_voxels, input.lod);
	} else {
		Curve &curve = **params.curve;
		result = VoxelGeneratorHeightmap::generate(
				out_buffer,
				[&noise, &curve](int x, int z) {
					return curve.interpolate_baked(0.5 + 0.5 * noise.get_noise_2d(x, z));
				},
				input.origin_in_voxels, input.lod);
	}

	out_buffer.compress_uniform_channels();
	return result;
}

void VoxelGeneratorNoise2D::_on_noise_changed() {
	ERR_FAIL_COND(_noise.is_null());
	RWLockWrite wlock(_parameters_lock);
	_parameters.noise = _noise->duplicate();
}

void VoxelGeneratorNoise2D::_on_curve_changed() {
	ERR_FAIL_COND(_curve.is_null());
	RWLockWrite wlock(_parameters_lock);
	_parameters.curve = _curve->duplicate();
	_parameters.curve->bake();
}

void VoxelGeneratorNoise2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_noise", "noise"), &VoxelGeneratorNoise2D::set_noise);
	ClassDB::bind_method(D_METHOD("get_noise"), &VoxelGeneratorNoise2D::get_noise);

	ClassDB::bind_method(D_METHOD("set_curve", "curve"), &VoxelGeneratorNoise2D::set_curve);
	ClassDB::bind_method(D_METHOD("get_curve"), &VoxelGeneratorNoise2D::get_curve);

	ClassDB::bind_method(D_METHOD("_on_noise_changed"), &VoxelGeneratorNoise2D::_on_noise_changed);
	ClassDB::bind_method(D_METHOD("_on_curve_changed"), &VoxelGeneratorNoise2D::_on_curve_changed);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, "OpenSimplexNoise"), "set_noise", "get_noise");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_curve", "get_curve");
}
