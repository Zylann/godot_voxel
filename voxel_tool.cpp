#include "voxel_tool.h"
#include "terrain/voxel_lod_terrain.h"
#include "voxel_buffer.h"

Vector3 VoxelRaycastResult::_b_get_position() const {
	return position.to_vec3();
}

Vector3 VoxelRaycastResult::_b_get_previous_position() const {
	return previous_position.to_vec3();
}

void VoxelRaycastResult::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_position"), &VoxelRaycastResult::_b_get_position);
	ClassDB::bind_method(D_METHOD("get_previous_position"), &VoxelRaycastResult::_b_get_previous_position);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "position"), "", "get_position");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "previous_position"), "", "get_previous_position");
}

//----------------------------------------

void VoxelTool::set_value(int val) {
	_value = val;
}

int VoxelTool::get_value() const {
	return _value;
}

void VoxelTool::set_eraser_value(int value) {
	_eraser_value = value;
}

int VoxelTool::get_eraser_value() const {
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

Ref<VoxelRaycastResult> VoxelTool::raycast(Vector3 pos, Vector3 dir, float max_distance) {
	ERR_PRINT("Not implemented");
	return Ref<VoxelRaycastResult>();
}

int VoxelTool::get_voxel(Vector3i pos) {
	return _get_voxel(pos);
}

float VoxelTool::get_voxel_f(Vector3i pos) {
	return _get_voxel_f(pos);
}

void VoxelTool::set_voxel(Vector3i pos, int v) {
	Rect3i box(pos, Vector3i(1));
	if (!is_area_editable(box)) {
		print_line("Area not editable");
		return;
	}
	_set_voxel(pos, v);
	_post_edit(box);
}

void VoxelTool::set_voxel_f(Vector3i pos, float v) {
	Rect3i box(pos, Vector3i(1));
	if (!is_area_editable(box)) {
		print_line("Area not editable");
		return;
	}
	_set_voxel_f(pos, v);
	_post_edit(box);
}

void VoxelTool::do_point(Vector3i pos) {
	Rect3i box(pos, Vector3i(1));
	if (!is_area_editable(box)) {
		return;
	}
	if (_channel == VoxelBuffer::CHANNEL_ISOLEVEL) {
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

int VoxelTool::_get_voxel(Vector3i pos) {
	ERR_PRINT("Not implemented");
	return 0;
}

float VoxelTool::_get_voxel_f(Vector3i pos) {
	ERR_PRINT("Not implemented");
	return 0;
}

void VoxelTool::_set_voxel(Vector3i pos, int v) {
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
			res = min(src_value, dst_value);
			break;

		case VoxelTool::MODE_REMOVE:
			res = max(1.0 - src_value, dst_value);
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

	Rect3i box(Vector3i(center) - Vector3i(Math::floor(radius)), Vector3i(Math::ceil(radius) * 2));

	if (!is_area_editable(box)) {
		print_line("Area not editable");
		return;
	}

	if (_channel == VoxelBuffer::CHANNEL_ISOLEVEL) {

		box.for_each_cell([this, center, radius](Vector3i pos) {
			float d = pos.to_vec3().distance_to(center) - radius;
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

void VoxelTool::do_box(Vector3i begin, Vector3i end) {
	ERR_PRINT("Not implemented");
}

void VoxelTool::paste(Vector3i pos, Ref<VoxelBuffer> p_voxels, int mask_value) {
	ERR_FAIL_COND(p_voxels.is_null());
	Ref<VoxelBuffer> voxels = Object::cast_to<VoxelBuffer>(*p_voxels);
	ERR_FAIL_COND(voxels.is_null());
	ERR_PRINT("Not implemented");
}

bool VoxelTool::is_area_editable(const Rect3i &box) const {
	ERR_PRINT("Not implemented");
	return false;
}

void VoxelTool::_post_edit(const Rect3i &box) {
	ERR_PRINT("Not implemented");
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

	ClassDB::bind_method(D_METHOD("get_voxel", "pos"), &VoxelTool::_b_get_voxel);
	ClassDB::bind_method(D_METHOD("get_voxel_f", "pos"), &VoxelTool::_b_get_voxel_f);
	ClassDB::bind_method(D_METHOD("set_voxel", "pos", "v"), &VoxelTool::_b_set_voxel);
	ClassDB::bind_method(D_METHOD("set_voxel_f", "pos", "v"), &VoxelTool::_b_set_voxel_f);
	ClassDB::bind_method(D_METHOD("do_point", "pos"), &VoxelTool::_b_do_point);
	ClassDB::bind_method(D_METHOD("do_sphere", "center", "radius"), &VoxelTool::_b_do_sphere);

	ClassDB::bind_method(D_METHOD("raycast", "origin", "direction", "max_distance"), &VoxelTool::_b_raycast, DEFVAL(10.0));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "value"), "set_value", "get_value");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "channel", PROPERTY_HINT_ENUM, VoxelBuffer::CHANNEL_ID_HINT_STRING), "set_channel", "get_channel");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "eraser_value"), "set_eraser_value", "get_eraser_value");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Add,Remove,Set"), "set_mode", "get_mode");

	BIND_ENUM_CONSTANT(MODE_ADD);
	BIND_ENUM_CONSTANT(MODE_REMOVE);
	BIND_ENUM_CONSTANT(MODE_SET);
}
