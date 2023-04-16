#include "voxel_tool.h"
#include "../storage/voxel_buffer_gd.h"
#include "../util/log.h"
#include "../util/math/color8.h"
#include "../util/math/conv.h"
#include "../util/profiling.h"

namespace zylann::voxel {

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

void VoxelTool::set_channel(VoxelBufferInternal::ChannelId p_channel) {
	ERR_FAIL_INDEX(p_channel, VoxelBufferInternal::MAX_CHANNELS);
	_channel = p_channel;
}

VoxelBufferInternal::ChannelId VoxelTool::get_channel() const {
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
	_sdf_scale = math::max(s, 0.00001f);
}

void VoxelTool::set_texture_index(int ti) {
	ERR_FAIL_INDEX(ti, 16);
	_texture_params.index = ti;
}

int VoxelTool::get_texture_index() const {
	return _texture_params.index;
}

void VoxelTool::set_texture_opacity(float opacity) {
	_texture_params.opacity = math::clamp(opacity, 0.f, 1.f);
}

float VoxelTool::get_texture_opacity() const {
	return _texture_params.opacity;
}

void VoxelTool::set_texture_falloff(float falloff) {
	_texture_params.sharpness = 1.f / math::clamp(falloff, 0.001f, 1.f);
}

void VoxelTool::set_sdf_strength(float strength) {
	_sdf_strength = math::clamp(strength, 0.f, 1.f);
}

float VoxelTool::get_sdf_strength() const {
	return _sdf_strength;
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
	Box3i box(pos, Vector3i(1, 1, 1));
	if (!is_area_editable(box)) {
		ZN_PRINT_VERBOSE("Area not editable");
		return;
	}
	_set_voxel(pos, v);
	_post_edit(box);
}

void VoxelTool::set_voxel_f(Vector3i pos, float v) {
	Box3i box(pos, Vector3i(1, 1, 1));
	if (!is_area_editable(box)) {
		ZN_PRINT_VERBOSE("Area not editable");
		return;
	}
	_set_voxel_f(pos, v);
	_post_edit(box);
}

void VoxelTool::do_point(Vector3i pos) {
	Box3i box(pos, Vector3i(1, 1, 1));
	if (!is_area_editable(box)) {
		return;
	}
	if (_channel == VoxelBufferInternal::CHANNEL_SDF) {
		_set_voxel_f(pos, _mode == MODE_REMOVE ? 1.0 : -1.0);
	} else {
		_set_voxel(pos, _mode == MODE_REMOVE ? _eraser_value : _value);
	}
	_post_edit(box);
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
			res = zylann::math::sdf_union(src_value, dst_value);
			break;

		case VoxelTool::MODE_REMOVE:
			// Relative complement (or difference)
			res = zylann::math::sdf_subtract(dst_value, src_value);
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

// The following are default legacy implementations. They may be slower than specialized ones, so they can often be
// defined in subclasses of VoxelTool. Ideally, a function may be exposed on the base class only if it has an optimal
// definition in all specialized classes.

void VoxelTool::do_sphere(Vector3 center, float radius) {
	ZN_PROFILE_SCOPE();

	const Box3i box(math::floor_to_int(center) - Vector3iUtil::create(Math::floor(radius)),
			Vector3iUtil::create(Math::ceil(radius) * 2));

	if (!is_area_editable(box)) {
		ZN_PRINT_VERBOSE("Area not editable");
		return;
	}

	if (_channel == VoxelBufferInternal::CHANNEL_SDF) {
		box.for_each_cell([this, center, radius](Vector3i pos) {
			float d = _sdf_scale * zylann::math::sdf_sphere(pos, center, radius);
			_set_voxel_f(pos, sdf_blend(d, get_voxel_f(pos), _mode));
		});

	} else {
		int value = _mode == MODE_REMOVE ? _eraser_value : _value;

		box.for_each_cell([this, center, radius, value](Vector3i pos) {
			float d = Vector3(pos).distance_to(center);
			if (d <= radius) {
				_set_voxel(pos, value);
			}
		});
	}

	_post_edit(box);
}

// Erases matter in every voxel where the provided buffer has matter.
void VoxelTool::sdf_stamp_erase(Ref<gd::VoxelBuffer> stamp, Vector3i pos) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND_MSG(
			get_channel() != VoxelBufferInternal::CHANNEL_SDF, "This function only works when channel is set to SDF");

	const Box3i box(pos, stamp->get_buffer().get_size());
	if (!is_area_editable(box)) {
		ZN_PRINT_VERBOSE("Area not editable");
		return;
	}

	box.for_each_cell_zxy([this, stamp, pos](Vector3i pos_in_volume) {
		const Vector3i pos_in_stamp = pos_in_volume - pos;
		const float dst_sdf =
				stamp->get_voxel_f(pos_in_stamp.x, pos_in_stamp.y, pos_in_stamp.z, VoxelBufferInternal::CHANNEL_SDF);
		if (dst_sdf <= 0.f) {
			_set_voxel_f(pos_in_volume, 1.f);
		}
	});

	_post_edit(box);
}

void VoxelTool::do_box(Vector3i begin, Vector3i end) {
	ZN_PROFILE_SCOPE();
	Vector3iUtil::sort_min_max(begin, end);
	Box3i box = Box3i::from_min_max(begin, end + Vector3i(1, 1, 1));

	if (!is_area_editable(box)) {
		ZN_PRINT_VERBOSE("Area not editable");
		return;
	}

	if (_channel == VoxelBufferInternal::CHANNEL_SDF) {
		// TODO Better quality
		box.for_each_cell([this](Vector3i pos) { _set_voxel_f(pos, sdf_blend(-1.0, get_voxel_f(pos), _mode)); });

	} else {
		int value = _mode == MODE_REMOVE ? _eraser_value : _value;
		box.for_each_cell([this, value](Vector3i pos) { _set_voxel(pos, value); });
	}

	_post_edit(box);
}

void VoxelTool::copy(Vector3i pos, Ref<gd::VoxelBuffer> dst, uint8_t channel_mask) const {
	ERR_FAIL_COND(dst.is_null());
	ERR_PRINT("Not implemented");
}

void VoxelTool::paste(Vector3i p_pos, Ref<gd::VoxelBuffer> p_voxels, uint8_t channels_mask) {
	ERR_FAIL_COND(p_voxels.is_null());
	ERR_PRINT("Not implemented");
}

void VoxelTool::paste_masked(Vector3i p_pos, Ref<gd::VoxelBuffer> p_voxels, uint8_t channels_mask, uint8_t mask_channel,
		uint64_t mask_value) {
	ERR_FAIL_COND(p_voxels.is_null());
	ERR_PRINT("Not implemented");
}

void VoxelTool::smooth_sphere(Vector3 sphere_center, float sphere_radius, int blur_radius) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(blur_radius >= 1 && blur_radius <= 64);
	ZN_ASSERT_RETURN(sphere_radius >= 0.01f);

	const Box3i voxel_box = Box3i::from_min_max(
			math::floor_to_int(sphere_center - Vector3(sphere_radius, sphere_radius, sphere_radius)),
			math::ceil_to_int(sphere_center + Vector3(sphere_radius, sphere_radius, sphere_radius)));

	const Box3i padded_voxel_box = voxel_box.padded(blur_radius);

	// TODO Perhaps should implement `copy` and `paste` with `VoxelBufferInternal` so Godot object wrappers wouldn't be
	// necessary
	Ref<gd::VoxelBuffer> buffer;
	buffer.instantiate();
	buffer->create(padded_voxel_box.size.x, padded_voxel_box.size.y, padded_voxel_box.size.z);

	if (_channel == VoxelBufferInternal::CHANNEL_SDF) {
		// Note, this only applies to SDF. It won't blur voxel texture data.

		copy(padded_voxel_box.pos, buffer, (1 << VoxelBufferInternal::CHANNEL_SDF));

		std::shared_ptr<VoxelBufferInternal> smooth_buffer = make_shared_instance<VoxelBufferInternal>();
		const Vector3f relative_sphere_center = to_vec3f(sphere_center - to_vec3(voxel_box.pos));
		ops::box_blur(buffer->get_buffer(), *smooth_buffer, blur_radius, relative_sphere_center, sphere_radius);

		paste(voxel_box.pos, gd::VoxelBuffer::create_shared(smooth_buffer), (1 << VoxelBufferInternal::CHANNEL_SDF));

	} else {
		ERR_PRINT("Not implemented");
	}
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

uint64_t VoxelTool::_b_get_voxel(Vector3i pos) {
	return get_voxel(pos);
}

float VoxelTool::_b_get_voxel_f(Vector3i pos) {
	return get_voxel_f(pos);
}

void VoxelTool::_b_set_voxel(Vector3i pos, uint64_t v) {
	set_voxel(pos, v);
}

void VoxelTool::_b_set_voxel_f(Vector3i pos, float v) {
	set_voxel_f(pos, v);
}

Ref<VoxelRaycastResult> VoxelTool::_b_raycast(Vector3 pos, Vector3 dir, float max_distance, uint32_t collision_mask) {
	return raycast(pos, dir, max_distance, collision_mask);
}

void VoxelTool::_b_do_point(Vector3i pos) {
	do_point(pos);
}

void VoxelTool::_b_do_sphere(Vector3 pos, float radius) {
	do_sphere(pos, radius);
}

void VoxelTool::_b_do_box(Vector3i begin, Vector3i end) {
	do_box(begin, end);
}

void VoxelTool::_b_copy(Vector3i pos, Ref<gd::VoxelBuffer> voxels, int channel_mask) {
	copy(pos, voxels, channel_mask);
}

void VoxelTool::_b_paste(Vector3i pos, Ref<gd::VoxelBuffer> voxels, int channels_mask) {
	paste(pos, voxels, channels_mask);
}

void VoxelTool::_b_paste_masked(
		Vector3i pos, Ref<gd::VoxelBuffer> voxels, int channels_mask, int mask_channel, int64_t mask_value) {
	paste_masked(pos, voxels, channels_mask, mask_channel, mask_value);
}

Variant VoxelTool::_b_get_voxel_metadata(Vector3i pos) const {
	return get_voxel_metadata(pos);
}

void VoxelTool::_b_set_voxel_metadata(Vector3i pos, Variant meta) {
	return set_voxel_metadata(pos, meta);
}

bool VoxelTool::_b_is_area_editable(AABB box) const {
	return is_area_editable(Box3i(math::floor_to_int(box.position), math::floor_to_int(box.size)));
}

static int _b_color_to_u16(Color col) {
	return Color8(col).to_u16();
}

static int _b_vec4i_to_u16_indices(Vector4i v) {
	return encode_indices_to_packed_u16(v.x, v.y, v.z, v.w);
}

static int _b_color_to_u16_weights(Color cf) {
	const Color8 c(cf);
	return encode_weights_to_packed_u16_lossy(c.r, c.g, c.b, c.a);
}

static Vector4i _b_u16_indices_to_vec4i(int e) {
	FixedArray<uint8_t, 4> indices = decode_indices_from_packed_u16(e);
	return Vector4i(indices[0], indices[1], indices[2], indices[3]);
}

static Color _b_u16_weights_to_color(int e) {
	FixedArray<uint8_t, 4> indices = decode_weights_from_packed_u16(e);
	return Color(indices[0] / 255.f, indices[1] / 255.f, indices[2] / 255.f, indices[3] / 255.f);
}

static Color _b_normalize_color(Color c) {
	const float sum = c.r + c.g + c.b + c.a;
	if (sum < 0.00001f) {
		return Color();
	}
	return c / sum;
}

void VoxelTool::_b_set_channel(gd::VoxelBuffer::ChannelId p_channel) {
	set_channel(VoxelBufferInternal::ChannelId(p_channel));
}

gd::VoxelBuffer::ChannelId VoxelTool::_b_get_channel() const {
	return gd::VoxelBuffer::ChannelId(get_channel());
}

void VoxelTool::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_value", "v"), &VoxelTool::set_value);
	ClassDB::bind_method(D_METHOD("get_value"), &VoxelTool::get_value);

	ClassDB::bind_method(D_METHOD("set_channel", "v"), &VoxelTool::_b_set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &VoxelTool::_b_get_channel);

	ClassDB::bind_method(D_METHOD("set_mode", "m"), &VoxelTool::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &VoxelTool::get_mode);

	ClassDB::bind_method(D_METHOD("set_eraser_value", "v"), &VoxelTool::set_eraser_value);
	ClassDB::bind_method(D_METHOD("get_eraser_value"), &VoxelTool::get_eraser_value);

	ClassDB::bind_method(D_METHOD("set_sdf_scale", "scale"), &VoxelTool::set_sdf_scale);
	ClassDB::bind_method(D_METHOD("get_sdf_scale"), &VoxelTool::get_sdf_scale);

	ClassDB::bind_method(D_METHOD("set_sdf_strength", "strength"), &VoxelTool::set_sdf_strength);
	ClassDB::bind_method(D_METHOD("get_sdf_strength"), &VoxelTool::get_sdf_strength);

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

	ClassDB::bind_method(
			D_METHOD("smooth_sphere", "sphere_center", "sphere_radius", "blur_radius"), &VoxelTool::smooth_sphere);

	ClassDB::bind_method(D_METHOD("set_voxel_metadata", "pos", "meta"), &VoxelTool::_b_set_voxel_metadata);
	ClassDB::bind_method(D_METHOD("get_voxel_metadata", "pos"), &VoxelTool::_b_get_voxel_metadata);

	ClassDB::bind_method(D_METHOD("copy", "src_pos", "dst_buffer", "channels_mask"), &VoxelTool::_b_copy);
	ClassDB::bind_method(D_METHOD("paste", "dst_pos", "src_buffer", "channels_mask"), &VoxelTool::_b_paste);
	ClassDB::bind_method(
			D_METHOD("paste_masked", "dst_pos", "src_buffer", "channels_mask", "mask_channel", "mask_value"),
			&VoxelTool::_b_paste_masked);

	ClassDB::bind_method(D_METHOD("raycast", "origin", "direction", "max_distance", "collision_mask"),
			&VoxelTool::_b_raycast, DEFVAL(10.0), DEFVAL(0xffffffff));

	ClassDB::bind_method(D_METHOD("is_area_editable", "box"), &VoxelTool::_b_is_area_editable);

	// Encoding helpers
	ClassDB::bind_static_method(VoxelTool::get_class_static(), D_METHOD("color_to_u16", "color"), &_b_color_to_u16);
	ClassDB::bind_static_method(
			VoxelTool::get_class_static(), D_METHOD("vec4i_to_u16_indices"), &_b_vec4i_to_u16_indices);
	ClassDB::bind_static_method(
			VoxelTool::get_class_static(), D_METHOD("color_to_u16_weights"), &_b_color_to_u16_weights);
	ClassDB::bind_static_method(
			VoxelTool::get_class_static(), D_METHOD("u16_indices_to_vec4i"), &_b_u16_indices_to_vec4i);
	ClassDB::bind_static_method(
			VoxelTool::get_class_static(), D_METHOD("u16_weights_to_color"), &_b_u16_weights_to_color);
	ClassDB::bind_static_method(VoxelTool::get_class_static(), D_METHOD("normalize_color"), &_b_normalize_color);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "value"), "set_value", "get_value");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "channel", PROPERTY_HINT_ENUM, gd::VoxelBuffer::CHANNEL_ID_HINT_STRING),
			"set_channel", "get_channel");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "eraser_value"), "set_eraser_value", "get_eraser_value");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Add,Remove,Set"), "set_mode", "get_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sdf_scale"), "set_sdf_scale", "get_sdf_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sdf_strength"), "set_sdf_strength", "get_sdf_strength");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_index"), "set_texture_index", "get_texture_index");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "texture_opacity"), "set_texture_opacity", "get_texture_opacity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "texture_falloff"), "set_texture_falloff", "get_texture_falloff");

	BIND_ENUM_CONSTANT(MODE_ADD);
	BIND_ENUM_CONSTANT(MODE_REMOVE);
	BIND_ENUM_CONSTANT(MODE_SET);
	BIND_ENUM_CONSTANT(MODE_TEXTURE_PAINT);
}

} // namespace zylann::voxel
