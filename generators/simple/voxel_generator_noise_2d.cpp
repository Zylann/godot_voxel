#include "voxel_generator_noise_2d.h"
#include "../../constants/voxel_string_names.h"
#include "../../util/godot/classes/curve.h"
#include "../../util/godot/classes/fast_noise_lite.h"
#include "../../util/godot/core/callable.h"

namespace zylann::voxel {

VoxelGeneratorNoise2D::VoxelGeneratorNoise2D() {}

VoxelGeneratorNoise2D::~VoxelGeneratorNoise2D() {}

void VoxelGeneratorNoise2D::set_noise(Ref<Noise> noise) {
	if (_noise == noise) {
		return;
	}

	if (_noise.is_valid()) {
		_noise->disconnect(VoxelStringNames::get_singleton().changed,
				ZN_GODOT_CALLABLE_MP(this, VoxelGeneratorNoise2D, _on_noise_changed));
	}
	_noise = noise;
	Ref<Noise> copy;
	if (_noise.is_valid()) {
		_noise->connect(VoxelStringNames::get_singleton().changed,
				ZN_GODOT_CALLABLE_MP(this, VoxelGeneratorNoise2D, _on_noise_changed));
		// The OpenSimplexNoise resource is not thread-safe so we make a copy of it for use in threads
		copy = _noise->duplicate();
	}
	RWLockWrite wlock(_parameters_lock);
	_parameters.noise = copy;
}

Ref<Noise> VoxelGeneratorNoise2D::get_noise() const {
	return _noise;
}

void VoxelGeneratorNoise2D::set_curve(Ref<Curve> curve) {
	if (_curve == curve) {
		return;
	}
	if (_curve.is_valid()) {
		_curve->disconnect(VoxelStringNames::get_singleton().changed,
				ZN_GODOT_CALLABLE_MP(this, VoxelGeneratorNoise2D, _on_curve_changed));
	}
	_curve = curve;
	RWLockWrite wlock(_parameters_lock);
	if (_curve.is_valid()) {
		_curve->connect(VoxelStringNames::get_singleton().changed,
				ZN_GODOT_CALLABLE_MP(this, VoxelGeneratorNoise2D, _on_curve_changed));
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

VoxelGenerator::Result VoxelGeneratorNoise2D::generate_block(VoxelGenerator::VoxelQueryData &input) {
	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	Result result;

	ERR_FAIL_COND_V(params.noise.is_null(), result);
	Noise &noise = **params.noise;

	VoxelBufferInternal &out_buffer = input.voxel_buffer;

	if (_curve.is_null()) {
		result = VoxelGeneratorHeightmap::generate(
				out_buffer, [&noise](int x, int z) { return 0.5 + 0.5 * noise.get_noise_2d(x, z); },
				input.origin_in_voxels, input.lod);
	} else {
		Curve &curve = **params.curve;
		result = VoxelGeneratorHeightmap::generate(
				out_buffer,
				[&noise, &curve](int x, int z) { return curve.sample_baked(0.5 + 0.5 * noise.get_noise_2d(x, z)); },
				input.origin_in_voxels, input.lod);
	}

	out_buffer.compress_uniform_channels();
	return result;
}

void VoxelGeneratorNoise2D::generate_series(Span<const float> positions_x, Span<const float> positions_y,
		Span<const float> positions_z, unsigned int channel, Span<float> out_values, Vector3f min_pos,
		Vector3f max_pos) {
	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	Result result;

	ERR_FAIL_COND(params.noise.is_null());
	Noise &noise = **params.noise;

	if (_curve.is_null()) {
		generate_series_template(
				[&noise](float x, float z) { //
					return 0.5 + 0.5 * noise.get_noise_2d(x, z);
				},
				positions_x, positions_y, positions_z, channel, out_values, min_pos, max_pos);
	} else {
		Curve &curve = **params.curve;
		generate_series_template(
				[&noise, &curve](float x, float z) { //
					return curve.sample_baked(0.5 + 0.5 * noise.get_noise_2d(x, z));
				},
				positions_x, positions_y, positions_z, channel, out_values, min_pos, max_pos);
	}
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

#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_noise_changed"), &VoxelGeneratorNoise2D::_on_noise_changed);
	ClassDB::bind_method(D_METHOD("_on_curve_changed"), &VoxelGeneratorNoise2D::_on_curve_changed);
#endif

	// TODO Accept `Noise` instead of `FastNoiseLite`?
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, FastNoiseLite::get_class_static(),
						 PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT),
			"set_noise", "get_noise");
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_curve", "get_curve");
}

} // namespace zylann::voxel
