#include "voxel_tool.h"
#include "../storage/voxel_buffer.h"
#include "../util/macros.h"
#include "../util/profiling.h"

VoxelTool::VoxelTool() {
	_sdf_scale = VoxelBufferInternal::get_sdf_quantization_scale(VoxelBufferInternal::DEFAULT_SDF_CHANNEL_DEPTH);
}

void VoxelTool::set_value(uint64_t val) {
	_value = val;
}

uint64_t VoxelTool::get_value() const {
	return _value;
}

void VoxelTool::set_eraser_value(uint64_t value) {
	_eraser_value = value;
}

uint64_t VoxelTool::get_eraser_value() const {
	return _eraser_value;
}

void VoxelTool::set_channel(int channel) {
	ERR_FAIL_INDEX(channel, VoxelBuffer::MAX_CHANNELS);
	_channel = channel;
}

int VoxelTool::get_channel() const {
	return _channel;
}

void VoxelTool::set_mode(Mode mode) {
	_mode = mode;
}

VoxelTool::Mode VoxelTool::get_mode() const {
	return _mode;
}

float VoxelTool::get_sdf_scale() const {
	return _sdf_scale;
}

void VoxelTool::set_sdf_scale(float s) {
	_sdf_scale = max(s, 0.00001f);
}

void VoxelTool::set_texture_index(int ti) {
	ERR_FAIL_INDEX(ti, 16);
	_texture_params.index = ti;
}

int VoxelTool::get_texture_index() const {
	return _texture_params.index;
}

void VoxelTool::set_texture_opacity(float opacity) {
	_texture_params.opacity = clamp(opacity, 0.f, 1.f);
}

float VoxelTool::get_texture_opacity() const {
	return _texture_params.opacity;
}

void VoxelTool::set_texture_falloff(float falloff) {
	_texture_params.sharpness = 1.f / clamp(falloff, 0.001f, 1.f);
}

float VoxelTool::get_texture_falloff() const {
	ERR_FAIL_COND_V(_texture_params.sharpness < 1.f, 1.f);
	return 1.f / _texture_params.sharpness;
}

Ref<VoxelRaycastResult> VoxelTool::raycast(Vector3 pos, Vector3 dir, float max_distance, uint32_t collision_mask) {
	ERR_PRINT("Not implemented");
	return Ref<VoxelRaycastResult>();
	// See derived classes for implementations
}

uint64_t VoxelTool::get_voxel(Vector3i pos) const {
	return _get_voxel(pos);
}

float VoxelTool::get_voxel_f(Vector3i pos) const {
	return _get_voxel_f(pos);
}

void VoxelTool::set_voxel(Vector3i pos, uint64_t v) {
	Box3i box(pos, Vector3i(1));
	if (!is_area_editable(box)) {
		PRINT_VERBOSE("Area not editable");
		return;
	}
	_set_voxel(pos, v);
	_post_edit(box);
}

void VoxelTool::set_voxel_f(Vector3i pos, float v) {
	Box3i box(pos, Vector3i(1));
	if (!is_area_editable(box)) {
		PRINT_VERBOSE("Area not editable");
		return;
	}
	_set_voxel_f(pos, v);
	_post_edit(box);
}

void VoxelTool::do_point(Vector3i pos) {
	Box3i box(pos, Vector3i(1));
	if (!is_area_editable(box)) {
		return;
	}
	if (_channel == VoxelBuffer::CHANNEL_SDF) {
		_set_voxel_f(pos, _mode == MODE_REMOVE ? 1.0 : -1.0);
	} else {
		_set_voxel(pos, _mode == MODE_REMOVE ? _eraser_value : _value);
	}
	_post_edit(box);
}

void VoxelTool::do_line(Vector3i begin, Vector3i end) {
	ERR_PRINT("Not implemented");
}

void VoxelTool::do_circle(Vector3i pos, int radius, Vector3i direction) {
	ERR_PRINT("Not implemented");
}

uint64_t VoxelTool::_get_voxel(Vector3i pos) const {
	ERR_PRINT("Not implemented");
	return 0;
}

float VoxelTool::_get_voxel_f(Vector3i pos) const {
	ERR_PRINT("Not implemented");
	return 0;
}

void VoxelTool::_set_voxel(Vector3i pos, uint64_t v) {
	ERR_PRINT("Not implemented");
}

void VoxelTool::_set_voxel_f(Vector3i pos, float v) {
	ERR_PRINT("Not implemented");
}

// TODO May be worth using VoxelBuffer::read_write_action() in the future with a lambda,
// so we avoid the burden of going through get/set, validation and rehash access to blocks.
// Would work well by avoiding virtual as well using a specialized implementation.

namespace {
inline float sdf_blend(float src_value, float dst_value, VoxelTool::Mode mode) {
	float res;
	switch (mode) {
		case VoxelTool::MODE_ADD:
			res = sdf_union(src_value, dst_value);
			break;

		case VoxelTool::MODE_REMOVE:
			// Relative complement (or difference)
			res = sdf_subtract(dst_value, src_value);
			break;

		case VoxelTool::MODE_SET:
			res = src_value;
			break;

		default:
			res = 0;
			break;
	}
	return res;
}
} // namespace

void VoxelTool::do_sphere(Vector3 center, float radius) {
	VOXEL_PROFILE_SCOPE();

	const Box3i box(Vector3i::from_floored(center) - Vector3i(Math::floor(radius)), Vector3i(Math::ceil(radius) * 2));

	if (!is_area_editable(box)) {
		PRINT_VERBOSE("Area not editable");
		return;
	}

	if (_channel == VoxelBuffer::CHANNEL_SDF) {
		box.for_each_cell([this, center, radius](Vector3i pos) {
			float d = _sdf_scale * sdf_sphere(pos.to_vec3(), center, radius);
			_set_voxel_f(pos, sdf_blend(d, get_voxel_f(pos), _mode));
		});

	} else {
		int value = _mode == MODE_REMOVE ? _eraser_value : _value;

		box.for_each_cell([this, center, radius, value](Vector3i pos) {
			float d = pos.to_vec3().distance_to(center);
			if (d <= radius) {
				_set_voxel(pos, value);
			}
		});
	}

	_post_edit(box);
}

// Erases matter in every voxel where the provided buffer has matter.
void VoxelTool::sdf_stamp_erase(Ref<VoxelBuffer> stamp, Vector3i pos) {
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND_MSG(get_channel() != VoxelBuffer::CHANNEL_SDF, "This function only works when channel is set to SDF");

	const Box3i box(pos, stamp->get_buffer().get_size());
	if (!is_area_editable(box)) {
		PRINT_VERBOSE("Area not editable");
		return;
	}

	box.for_each_cell_zxy([this, stamp, pos](Vector3i pos_in_volume) {
		const Vector3i pos_in_stamp = pos_in_volume - pos;
		const float dst_sdf = stamp->get_voxel_f(
				pos_in_stamp.x, pos_in_stamp.y, pos_in_stamp.z, VoxelBuffer::CHANNEL_SDF);
		if (dst_sdf <= 0.f) {
			_set_voxel_f(pos_in_volume, 1.f);
		}
	});

	_post_edit(box);
}

void VoxelTool::do_box(Vector3i begin, Vector3i end) {
	VOXEL_PROFILE_SCOPE();
	Vector3i::sort_min_max(begin, end);
	Box3i box = Box3i::from_min_max(begin, end + Vector3i(1, 1, 1));

	if (!is_area_editable(box)) {
		PRINT_VERBOSE("Area not editable");
		return;
	}

	if (_channel == VoxelBuffer::CHANNEL_SDF) {
		// TODO Better quality
		box.for_each_cell([this](Vector3i pos) {
			_set_voxel_f(pos, sdf_blend(-1.0, get_voxel_f(pos), _mode));
		});

	} else {
		int value = _mode == MODE_REMOVE ? _eraser_value : _value;
		box.for_each_cell([this, value](Vector3i pos) {
			_set_voxel(pos, value);
		});
	}

	_post_edit(box);
}

void VoxelTool::copy(Vector3i pos, Ref<VoxelBuffer> dst, uint8_t channel_mask) const {
	ERR_FAIL_COND(dst.is_null());
	ERR_PRINT("Not implemented");
}

void VoxelTool::paste(Vector3i p_pos, Ref<VoxelBuffer> p_voxels, uint8_t channels_mask, bool use_mask,
		uint64_t mask_value) {
	ERR_FAIL_COND(p_voxels.is_null());
	ERR_PRINT("Not implemented");
}

bool VoxelTool::is_area_editable(const Box3i &box) const {
	ERR_PRINT("Not implemented");
	return false;
}

void VoxelTool::_post_edit(const Box3i &box) {
	ERR_PRINT("Not implemented");
}

void VoxelTool::set_voxel_metadata(Vector3i pos, Variant meta) {
	ERR_PRINT("Not implemented");
}

Variant VoxelTool::get_voxel_metadata(Vector3i pos) const {
	ERR_PRINT("Not implemented");
	return Variant();
}

void VoxelTool::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_value", "v"), &VoxelTool::set_value);
	ClassDB::bind_method(D_METHOD("get_value"), &VoxelTool::get_value);

	ClassDB::bind_method(D_METHOD("set_channel", "v"), &VoxelTool::set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &VoxelTool::get_channel);

	ClassDB::bind_method(D_METHOD("set_mode", "m"), &VoxelTool::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &VoxelTool::get_mode);

	ClassDB::bind_method(D_METHOD("set_eraser_value", "v"), &VoxelTool::set_eraser_value);
	ClassDB::bind_method(D_METHOD("get_eraser_value"), &VoxelTool::get_eraser_value);

	ClassDB::bind_method(D_METHOD("set_sdf_scale", "scale"), &VoxelTool::set_sdf_scale);
	ClassDB::bind_method(D_METHOD("get_sdf_scale"), &VoxelTool::get_sdf_scale);

	ClassDB::bind_method(D_METHOD("set_texture_index", "index"), &VoxelTool::set_texture_index);
	ClassDB::bind_method(D_METHOD("get_texture_index"), &VoxelTool::get_texture_index);

	ClassDB::bind_method(D_METHOD("set_texture_opacity", "opacity"), &VoxelTool::set_texture_opacity);
	ClassDB::bind_method(D_METHOD("get_texture_opacity"), &VoxelTool::get_texture_opacity);

	ClassDB::bind_method(D_METHOD("set_texture_falloff", "falloff"), &VoxelTool::set_texture_falloff);
	ClassDB::bind_method(D_METHOD("get_texture_falloff"), &VoxelTool::get_texture_falloff);

	ClassDB::bind_method(D_METHOD("get_voxel", "pos"), &VoxelTool::_b_get_voxel);
	ClassDB::bind_method(D_METHOD("get_voxel_f", "pos"), &VoxelTool::_b_get_voxel_f);
	ClassDB::bind_method(D_METHOD("set_voxel", "pos", "v"), &VoxelTool::_b_set_voxel);
	ClassDB::bind_method(D_METHOD("set_voxel_f", "pos", "v"), &VoxelTool::_b_set_voxel_f);
	ClassDB::bind_method(D_METHOD("do_point", "pos"), &VoxelTool::_b_do_point);
	ClassDB::bind_method(D_METHOD("do_sphere", "center", "radius"), &VoxelTool::_b_do_sphere);
	ClassDB::bind_method(D_METHOD("do_box", "begin", "end"), &VoxelTool::_b_do_box);

	ClassDB::bind_method(D_METHOD("set_voxel_metadata", "pos", "meta"), &VoxelTool::_b_set_voxel_metadata);
	ClassDB::bind_method(D_METHOD("get_voxel_metadata", "pos"), &VoxelTool::_b_get_voxel_metadata);

	ClassDB::bind_method(D_METHOD("copy", "src_pos", "dst_buffer", "channels_mask"), &VoxelTool::_b_copy);
	ClassDB::bind_method(D_METHOD("paste", "dst_pos", "src_buffer", "channels_mask", "src_mask_value"),
			&VoxelTool::_b_paste);

	ClassDB::bind_method(D_METHOD("raycast", "origin", "direction", "max_distance", "collision_mask"),
			&VoxelTool::_b_raycast, DEFVAL(10.0), DEFVAL(0xffffffff));

	ClassDB::bind_method(D_METHOD("is_area_editable", "box"), &VoxelTool::_b_is_area_editable);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "value"), "set_value", "get_value");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "channel", PROPERTY_HINT_ENUM, VoxelBuffer::CHANNEL_ID_HINT_STRING),
			"set_channel", "get_channel");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "eraser_value"), "set_eraser_value", "get_eraser_value");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Add,Remove,Set"), "set_mode", "get_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sdf_scale"), "set_sdf_scale", "get_sdf_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_index"), "set_texture_index", "get_texture_index");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "texture_opacity"), "set_texture_opacity", "get_texture_opacity");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "texture_falloff"), "set_texture_falloff", "get_texture_falloff");

	BIND_ENUM_CONSTANT(MODE_ADD);
	BIND_ENUM_CONSTANT(MODE_REMOVE);
	BIND_ENUM_CONSTANT(MODE_SET);
	BIND_ENUM_CONSTANT(MODE_TEXTURE_PAINT);
}
