#include "voxel_buffer.h"
#include "../edition/voxel_tool_buffer.h"
#include "../util/godot/funcs.h"

#include <core/func_ref.h>
#include <core/image.h>

const char *VoxelBuffer::CHANNEL_ID_HINT_STRING = "Type,Sdf,Color,Indices,Weights,Data5,Data6,Data7";

VoxelBuffer::VoxelBuffer() {
	_buffer = gd_make_shared<VoxelBufferInternal>();
}

VoxelBuffer::VoxelBuffer(std::shared_ptr<VoxelBufferInternal> &other) {
	CRASH_COND(other == nullptr);
	_buffer = other;
}

VoxelBuffer::~VoxelBuffer() {
}

void VoxelBuffer::clear() {
	_buffer->clear();
}

real_t VoxelBuffer::get_voxel_f(int x, int y, int z, unsigned int channel_index) const {
	return _buffer->get_voxel_f(x, y, z, channel_index);
}

void VoxelBuffer::set_voxel_f(real_t value, int x, int y, int z, unsigned int channel_index) {
	return _buffer->set_voxel_f(value, x, y, z, channel_index);
}

void VoxelBuffer::copy_channel_from(Ref<VoxelBuffer> other, unsigned int channel) {
	ERR_FAIL_COND(other.is_null());
	_buffer->copy_from(other->get_buffer(), channel);
}

void VoxelBuffer::copy_channel_from_area(Ref<VoxelBuffer> other, Vector3 src_min, Vector3 src_max, Vector3 dst_min,
		unsigned int channel) {
	ERR_FAIL_COND(other.is_null());
	_buffer->copy_from(other->get_buffer(),
			Vector3i::from_floored(src_min),
			Vector3i::from_floored(src_max),
			Vector3i::from_floored(dst_min), channel);
}

void VoxelBuffer::fill(uint64_t defval, unsigned int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	_buffer->fill(defval, channel_index);
}

void VoxelBuffer::fill_f(real_t value, unsigned int channel) {
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

void VoxelBuffer::downscale_to(Ref<VoxelBuffer> dst, Vector3 src_min, Vector3 src_max, Vector3 dst_min) const {
	ERR_FAIL_COND(dst.is_null());
	_buffer->downscale_to(dst->get_buffer(),
			Vector3i::from_floored(src_min),
			Vector3i::from_floored(src_max),
			Vector3i::from_floored(dst_min));
}

Ref<VoxelBuffer> VoxelBuffer::duplicate(bool include_metadata) const {
	Ref<VoxelBuffer> d;
	d.instance();
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

void VoxelBuffer::set_block_metadata(Variant meta) {
	_buffer->set_block_metadata(meta);
}

void VoxelBuffer::for_each_voxel_metadata(Ref<FuncRef> callback) const {
	ERR_FAIL_COND(callback.is_null());
	_buffer->for_each_voxel_metadata(callback);
}

void VoxelBuffer::for_each_voxel_metadata_in_area(Ref<FuncRef> callback, Vector3 min_pos, Vector3 max_pos) {
	ERR_FAIL_COND(callback.is_null());
	_buffer->for_each_voxel_metadata_in_area(callback,
			Box3i::from_min_max(Vector3i::from_floored(min_pos), Vector3i::from_floored(max_pos)));
}

void VoxelBuffer::copy_voxel_metadata_in_area(Ref<VoxelBuffer> src_buffer, Vector3 src_min_pos, Vector3 src_max_pos,
		Vector3 dst_pos) {
	ERR_FAIL_COND(src_buffer.is_null());
	_buffer->copy_voxel_metadata_in_area(
			src_buffer->get_buffer(),
			Box3i::from_min_max(Vector3i::from_floored(src_min_pos), Vector3i::from_floored(src_max_pos)),
			Vector3i::from_floored(dst_pos));
}

void VoxelBuffer::clear_voxel_metadata_in_area(Vector3 min_pos, Vector3 max_pos) {
	_buffer->clear_voxel_metadata_in_area(
			Box3i::from_min_max(Vector3i::from_floored(min_pos), Vector3i::from_floored(max_pos)));
}

void VoxelBuffer::clear_voxel_metadata() {
	_buffer->clear_voxel_metadata();
}

Ref<Image> VoxelBuffer::debug_print_sdf_to_image_top_down() {
	return _buffer->debug_print_sdf_to_image_top_down();
}

void VoxelBuffer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create", "sx", "sy", "sz"), &VoxelBuffer::create);
	ClassDB::bind_method(D_METHOD("clear"), &VoxelBuffer::clear);

	ClassDB::bind_method(D_METHOD("get_size"), &VoxelBuffer::get_size);
	ClassDB::bind_method(D_METHOD("get_size_x"), &VoxelBuffer::get_size_x);
	ClassDB::bind_method(D_METHOD("get_size_y"), &VoxelBuffer::get_size_y);
	ClassDB::bind_method(D_METHOD("get_size_z"), &VoxelBuffer::get_size_z);

	ClassDB::bind_method(D_METHOD("set_voxel", "value", "x", "y", "z", "channel"),
			&VoxelBuffer::set_voxel, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_voxel_f", "value", "x", "y", "z", "channel"),
			&VoxelBuffer::set_voxel_f, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_voxel_v", "value", "pos", "channel"), &VoxelBuffer::set_voxel_v, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_voxel", "x", "y", "z", "channel"), &VoxelBuffer::get_voxel, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_voxel_f", "x", "y", "z", "channel"), &VoxelBuffer::get_voxel_f, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_voxel_tool"), &VoxelBuffer::get_voxel_tool);

	ClassDB::bind_method(D_METHOD("get_channel_depth", "channel"), &VoxelBuffer::get_channel_depth);
	ClassDB::bind_method(D_METHOD("set_channel_depth", "channel", "depth"), &VoxelBuffer::set_channel_depth);

	ClassDB::bind_method(D_METHOD("fill", "value", "channel"), &VoxelBuffer::fill, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("fill_f", "value", "channel"), &VoxelBuffer::fill_f, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("fill_area", "value", "min", "max", "channel"),
			&VoxelBuffer::fill_area, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("copy_channel_from", "other", "channel"), &VoxelBuffer::copy_channel_from);
	ClassDB::bind_method(D_METHOD("copy_channel_from_area", "other", "src_min", "src_max", "dst_min", "channel"),
			&VoxelBuffer::copy_channel_from_area);
	ClassDB::bind_method(D_METHOD("downscale_to", "dst", "src_min", "src_max", "dst_min"),
			&VoxelBuffer::downscale_to);

	ClassDB::bind_method(D_METHOD("is_uniform", "channel"), &VoxelBuffer::is_uniform);
	// TODO Rename `compress_uniform_channels`
	ClassDB::bind_method(D_METHOD("optimize"), &VoxelBuffer::compress_uniform_channels);
	ClassDB::bind_method(D_METHOD("get_channel_compression", "channel"), &VoxelBuffer::get_channel_compression);

	ClassDB::bind_method(D_METHOD("get_block_metadata"), &VoxelBuffer::get_block_metadata);
	ClassDB::bind_method(D_METHOD("set_block_metadata", "meta"), &VoxelBuffer::set_block_metadata);
	ClassDB::bind_method(D_METHOD("get_voxel_metadata", "pos"), &VoxelBuffer::get_voxel_metadata);
	ClassDB::bind_method(D_METHOD("set_voxel_metadata", "pos", "value"), &VoxelBuffer::set_voxel_metadata);
	ClassDB::bind_method(D_METHOD("for_each_voxel_metadata", "callback"), &VoxelBuffer::for_each_voxel_metadata);
	ClassDB::bind_method(D_METHOD("for_each_voxel_metadata_in_area", "callback", "min_pos", "max_pos"),
			&VoxelBuffer::for_each_voxel_metadata_in_area);
	ClassDB::bind_method(D_METHOD("clear_voxel_metadata"), &VoxelBuffer::clear_voxel_metadata);
	ClassDB::bind_method(D_METHOD("clear_voxel_metadata_in_area", "min_pos", "max_pos"),
			&VoxelBuffer::clear_voxel_metadata_in_area);
	ClassDB::bind_method(
			D_METHOD("copy_voxel_metadata_in_area", "src_buffer", "src_min_pos", "src_max_pos", "dst_min_pos"),
			&VoxelBuffer::copy_voxel_metadata_in_area);

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
