#include "voxel_buffer_gd.h"
#include "../edition/voxel_tool_buffer.h"
#include "../util/dstack.h"
#include "../util/math/color.h"
#include "../util/memory.h"
#include "voxel_metadata_variant.h"

#include <core/io/image.h>

namespace zylann::voxel::gd {

const char *VoxelBuffer::CHANNEL_ID_HINT_STRING = "Type,Sdf,Color,Indices,Weights,Data5,Data6,Data7";
static thread_local bool s_create_shared = false;

VoxelBuffer::VoxelBuffer() {
	if (!s_create_shared) {
		_buffer = make_shared_instance<VoxelBufferInternal>();
	}
}

VoxelBuffer::VoxelBuffer(std::shared_ptr<VoxelBufferInternal> &other) {
	CRASH_COND(other == nullptr);
	_buffer = other;
}

Ref<VoxelBuffer> VoxelBuffer::create_shared(std::shared_ptr<VoxelBufferInternal> &other) {
	Ref<VoxelBuffer> vb;
	s_create_shared = true;
	vb.instantiate();
	s_create_shared = false;
	vb->_buffer = other;
	return vb;
}

VoxelBuffer::~VoxelBuffer() {}

void VoxelBuffer::clear() {
	_buffer->clear();
}

real_t VoxelBuffer::get_voxel_f(int x, int y, int z, unsigned int channel_index) const {
	return _buffer->get_voxel_f(x, y, z, channel_index);
}

void VoxelBuffer::set_voxel_f(real_t value, int x, int y, int z, unsigned int channel_index) {
	ZN_DSTACK();
	return _buffer->set_voxel_f(value, x, y, z, channel_index);
}

void VoxelBuffer::copy_channel_from(Ref<VoxelBuffer> other, unsigned int channel) {
	ZN_DSTACK();
	ERR_FAIL_COND(other.is_null());
	_buffer->copy_from(other->get_buffer(), channel);
}

void VoxelBuffer::copy_channel_from_area(
		Ref<VoxelBuffer> other, Vector3i src_min, Vector3i src_max, Vector3i dst_min, unsigned int channel) {
	ZN_DSTACK();
	ERR_FAIL_COND(other.is_null());
	_buffer->copy_from(other->get_buffer(), src_min, src_max, dst_min, channel);
}

void VoxelBuffer::fill(uint64_t defval, unsigned int channel_index) {
	ZN_DSTACK();
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	_buffer->fill(defval, channel_index);
}

void VoxelBuffer::fill_f(real_t value, unsigned int channel) {
	ZN_DSTACK();
	ERR_FAIL_INDEX(channel, MAX_CHANNELS);
	_buffer->fill_f(value, channel);
}

bool VoxelBuffer::is_uniform(unsigned int channel_index) const {
	ERR_FAIL_INDEX_V(channel_index, MAX_CHANNELS, true);
	return _buffer->is_uniform(channel_index);
}

void VoxelBuffer::compress_uniform_channels() {
	_buffer->compress_uniform_channels();
}

VoxelBuffer::Compression VoxelBuffer::get_channel_compression(unsigned int channel_index) const {
	ERR_FAIL_INDEX_V(channel_index, MAX_CHANNELS, VoxelBuffer::COMPRESSION_NONE);
	return VoxelBuffer::Compression(_buffer->get_channel_compression(channel_index));
}

void VoxelBuffer::downscale_to(Ref<VoxelBuffer> dst, Vector3i src_min, Vector3i src_max, Vector3i dst_min) const {
	ZN_DSTACK();
	ERR_FAIL_COND(dst.is_null());
	_buffer->downscale_to(dst->get_buffer(), src_min, src_max, dst_min);
}

Ref<VoxelBuffer> VoxelBuffer::duplicate(bool include_metadata) const {
	Ref<VoxelBuffer> d;
	d.instantiate();
	_buffer->duplicate_to(d->get_buffer(), include_metadata);
	return d;
}

Ref<VoxelTool> VoxelBuffer::get_voxel_tool() {
	// I can't make this function `const`, because `Ref<T>` has no constructor taking a `const T*`.
	// The compiler would then choose Ref<T>(const Variant&), which fumbles `this` into a null pointer
	Ref<VoxelBuffer> vb(this);
	return Ref<VoxelTool>(memnew(VoxelToolBuffer(vb)));
}

void VoxelBuffer::set_channel_depth(unsigned int channel_index, Depth new_depth) {
	_buffer->set_channel_depth(channel_index, VoxelBufferInternal::Depth(new_depth));
}

VoxelBuffer::Depth VoxelBuffer::get_channel_depth(unsigned int channel_index) const {
	return VoxelBuffer::Depth(_buffer->get_channel_depth(channel_index));
}

Variant VoxelBuffer::get_block_metadata() const {
	return get_as_variant(_buffer->get_block_metadata());
}

void VoxelBuffer::set_block_metadata(Variant meta) {
	set_as_variant(_buffer->get_block_metadata(), meta);
}

Variant VoxelBuffer::get_voxel_metadata(Vector3i pos) const {
	VoxelMetadata *meta = _buffer->get_voxel_metadata(pos);
	if (meta == nullptr) {
		return Variant();
	}
	return get_as_variant(*meta);
}

void VoxelBuffer::set_voxel_metadata(Vector3i pos, Variant meta) {
	if (meta.get_type() == Variant::NIL) {
		_buffer->erase_voxel_metadata(pos);
	} else {
		VoxelMetadata *mv = _buffer->get_or_create_voxel_metadata(pos);
		ZN_ASSERT_RETURN(mv != nullptr);
		set_as_variant(*mv, meta);
	}
}

void VoxelBuffer::for_each_voxel_metadata(const Callable &callback) const {
	ERR_FAIL_COND(callback.is_null());
	//_buffer->for_each_voxel_metadata(callback);

	const FlatMapMoveOnly<Vector3i, VoxelMetadata> &metadata = _buffer->get_voxel_metadata();

	for (auto it = metadata.begin(); it != metadata.end(); ++it) {
		Variant v = get_as_variant(it->value);
		const Variant key = it->key;
		const Variant *args[2] = { &key, &v };
		Callable::CallError err;
		Variant retval; // We don't care about the return value, Callable API requires it
		callback.call(args, 2, retval, err);

		ERR_FAIL_COND_MSG(
				err.error != Callable::CallError::CALL_OK, String("Callable failed at {0}").format(varray(key)));

		// TODO Can't provide detailed error because FuncRef doesn't give us access to the object
		// ERR_FAIL_COND_MSG(err.error != Variant::CallError::CALL_OK, false,
		// 		Variant::get_call_error_text(callback->get_object(), method_name, nullptr, 0, err));
	}
}

void VoxelBuffer::for_each_voxel_metadata_in_area(const Callable &callback, Vector3i min_pos, Vector3i max_pos) {
	ERR_FAIL_COND(callback.is_null());

	const Box3i box = Box3i::from_min_max(min_pos, max_pos);

	_buffer->for_each_voxel_metadata_in_area(box, [&callback](Vector3i rel_pos, const VoxelMetadata &meta) {
		Variant v = get_as_variant(meta);
		const Variant key = rel_pos;
		const Variant *args[2] = { &key, &v };
		Callable::CallError err;
		Variant retval; // We don't care about the return value, Callable API requires it
		callback.call(args, 2, retval, err);

		ERR_FAIL_COND_MSG(
				err.error != Callable::CallError::CALL_OK, String("Callable failed at {0}").format(varray(key)));

		// TODO Can't provide detailed error because FuncRef doesn't give us access to the object
		// ERR_FAIL_COND_MSG(err.error != Variant::CallError::CALL_OK, false,
		// 		Variant::get_call_error_text(callback->get_object(), method_name, nullptr, 0, err));
	});
}

void VoxelBuffer::copy_voxel_metadata_in_area(
		Ref<VoxelBuffer> src_buffer, Vector3i src_min_pos, Vector3i src_max_pos, Vector3i dst_pos) {
	ERR_FAIL_COND(src_buffer.is_null());
	_buffer->copy_voxel_metadata_in_area(
			src_buffer->get_buffer(), Box3i::from_min_max(src_min_pos, src_max_pos), dst_pos);
}

void VoxelBuffer::clear_voxel_metadata_in_area(Vector3i min_pos, Vector3i max_pos) {
	_buffer->clear_voxel_metadata_in_area(Box3i::from_min_max(min_pos, max_pos));
}

void VoxelBuffer::clear_voxel_metadata() {
	_buffer->clear_voxel_metadata();
}

Ref<Image> VoxelBuffer::debug_print_sdf_to_image_top_down() {
	return debug_print_sdf_to_image_top_down(*_buffer);
}

Ref<Image> VoxelBuffer::debug_print_sdf_to_image_top_down(const VoxelBufferInternal &vb) {
	Ref<Image> im;
	im.instantiate();
	const Vector3i size = vb.get_size();
	im->create(size.x, size.z, false, Image::FORMAT_RGB8);
	Vector3i pos;
	for (pos.z = 0; pos.z < size.z; ++pos.z) {
		for (pos.x = 0; pos.x < size.x; ++pos.x) {
			for (pos.y = size.y - 1; pos.y >= 0; --pos.y) {
				float v = vb.get_voxel_f(pos.x, pos.y, pos.z, VoxelBufferInternal::CHANNEL_SDF);
				if (v < 0.0) {
					break;
				}
			}
			float h = pos.y;
			float c = h / size.y;
			im->set_pixel(pos.x, pos.z, Color(c, c, c));
		}
	}
	return im;
}

Ref<Image> VoxelBuffer::debug_print_sdf_y_slice(float scale, int y) const {
	const VoxelBufferInternal &buffer = *_buffer;
	const Vector3i res = buffer.get_size();
	ERR_FAIL_COND_V(y < 0 || y >= res.y, Ref<Image>());

	Ref<Image> im;
	im.instantiate();
	im->create(res.x, res.z, false, Image::FORMAT_RGB8);

	const Color nega_col(0.5f, 0.5f, 1.0f);
	const Color posi_col(1.0f, 0.6f, 0.1f);
	const Color black(0.f, 0.f, 0.f);

	for (int z = 0; z < res.z; ++z) {
		for (int x = 0; x < res.x; ++x) {
			const float sd = scale * buffer.get_voxel_f(x, y, z, VoxelBufferInternal::CHANNEL_SDF);

			const float nega = math::clamp(-sd, 0.0f, 1.0f);
			const float posi = math::clamp(sd, 0.0f, 1.0f);
			const Color col = math::lerp(black, nega_col, nega) + math::lerp(black, posi_col, posi);

			im->set_pixel(x, z, col);
		}
	}

	return im;
}

Array VoxelBuffer::debug_print_sdf_y_slices(float scale) const {
	Array images;

	const VoxelBufferInternal &buffer = *_buffer;
	const Vector3i res = buffer.get_size();

	for (int y = 0; y < res.y; ++y) {
		images.append(debug_print_sdf_y_slice(scale, y));
	}

	return images;
}

void VoxelBuffer::_b_deprecated_optimize() {
	ERR_PRINT_ONCE("VoxelBuffer.optimize() is deprecated. Use compress_uniform_channels() instead.");
	compress_uniform_channels();
}

void VoxelBuffer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create", "sx", "sy", "sz"), &VoxelBuffer::create);
	ClassDB::bind_method(D_METHOD("clear"), &VoxelBuffer::clear);

	ClassDB::bind_method(D_METHOD("get_size"), &VoxelBuffer::get_size);
	ClassDB::bind_method(D_METHOD("get_size_x"), &VoxelBuffer::get_size_x);
	ClassDB::bind_method(D_METHOD("get_size_y"), &VoxelBuffer::get_size_y);
	ClassDB::bind_method(D_METHOD("get_size_z"), &VoxelBuffer::get_size_z);

	ClassDB::bind_method(D_METHOD("set_voxel", "value", "x", "y", "z", "channel"), &VoxelBuffer::set_voxel, DEFVAL(0));
	ClassDB::bind_method(
			D_METHOD("set_voxel_f", "value", "x", "y", "z", "channel"), &VoxelBuffer::set_voxel_f, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_voxel_v", "value", "pos", "channel"), &VoxelBuffer::set_voxel_v, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_voxel", "x", "y", "z", "channel"), &VoxelBuffer::get_voxel, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_voxel_f", "x", "y", "z", "channel"), &VoxelBuffer::get_voxel_f, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_voxel_tool"), &VoxelBuffer::get_voxel_tool);

	ClassDB::bind_method(D_METHOD("get_channel_depth", "channel"), &VoxelBuffer::get_channel_depth);
	ClassDB::bind_method(D_METHOD("set_channel_depth", "channel", "depth"), &VoxelBuffer::set_channel_depth);

	ClassDB::bind_method(D_METHOD("fill", "value", "channel"), &VoxelBuffer::fill, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("fill_f", "value", "channel"), &VoxelBuffer::fill_f, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("fill_area", "value", "min", "max", "channel"), &VoxelBuffer::fill_area, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("copy_channel_from", "other", "channel"), &VoxelBuffer::copy_channel_from);
	ClassDB::bind_method(D_METHOD("copy_channel_from_area", "other", "src_min", "src_max", "dst_min", "channel"),
			&VoxelBuffer::copy_channel_from_area);
	ClassDB::bind_method(D_METHOD("downscale_to", "dst", "src_min", "src_max", "dst_min"), &VoxelBuffer::downscale_to);

	ClassDB::bind_method(D_METHOD("is_uniform", "channel"), &VoxelBuffer::is_uniform);
	ClassDB::bind_method(D_METHOD("optimize"), &VoxelBuffer::_b_deprecated_optimize);
	ClassDB::bind_method(D_METHOD("compress_uniform_channels"), &VoxelBuffer::compress_uniform_channels);
	ClassDB::bind_method(D_METHOD("get_channel_compression", "channel"), &VoxelBuffer::get_channel_compression);

	ClassDB::bind_method(D_METHOD("get_block_metadata"), &VoxelBuffer::get_block_metadata);
	ClassDB::bind_method(D_METHOD("set_block_metadata", "meta"), &VoxelBuffer::set_block_metadata);
	ClassDB::bind_method(D_METHOD("get_voxel_metadata", "pos"), &VoxelBuffer::get_voxel_metadata);
	ClassDB::bind_method(D_METHOD("set_voxel_metadata", "pos", "value"), &VoxelBuffer::set_voxel_metadata);
	ClassDB::bind_method(D_METHOD("for_each_voxel_metadata", "callback"), &VoxelBuffer::for_each_voxel_metadata);
	ClassDB::bind_method(D_METHOD("for_each_voxel_metadata_in_area", "callback", "min_pos", "max_pos"),
			&VoxelBuffer::for_each_voxel_metadata_in_area);
	ClassDB::bind_method(D_METHOD("clear_voxel_metadata"), &VoxelBuffer::clear_voxel_metadata);
	ClassDB::bind_method(
			D_METHOD("clear_voxel_metadata_in_area", "min_pos", "max_pos"), &VoxelBuffer::clear_voxel_metadata_in_area);
	ClassDB::bind_method(
			D_METHOD("copy_voxel_metadata_in_area", "src_buffer", "src_min_pos", "src_max_pos", "dst_min_pos"),
			&VoxelBuffer::copy_voxel_metadata_in_area);
	ClassDB::bind_method(D_METHOD("debug_print_sdf_y_slices", "scale"), &VoxelBuffer::debug_print_sdf_y_slices);

	BIND_ENUM_CONSTANT(CHANNEL_TYPE);
	BIND_ENUM_CONSTANT(CHANNEL_SDF);
	BIND_ENUM_CONSTANT(CHANNEL_COLOR);
	BIND_ENUM_CONSTANT(CHANNEL_INDICES);
	BIND_ENUM_CONSTANT(CHANNEL_WEIGHTS);
	BIND_ENUM_CONSTANT(CHANNEL_DATA5);
	BIND_ENUM_CONSTANT(CHANNEL_DATA6);
	BIND_ENUM_CONSTANT(CHANNEL_DATA7);
	BIND_ENUM_CONSTANT(MAX_CHANNELS);

	BIND_ENUM_CONSTANT(DEPTH_8_BIT);
	BIND_ENUM_CONSTANT(DEPTH_16_BIT);
	BIND_ENUM_CONSTANT(DEPTH_32_BIT);
	BIND_ENUM_CONSTANT(DEPTH_64_BIT);
	BIND_ENUM_CONSTANT(DEPTH_COUNT);

	BIND_ENUM_CONSTANT(COMPRESSION_NONE);
	BIND_ENUM_CONSTANT(COMPRESSION_UNIFORM);
	BIND_ENUM_CONSTANT(COMPRESSION_COUNT);

	BIND_CONSTANT(MAX_SIZE);
}

} //namespace zylann::voxel::gd
