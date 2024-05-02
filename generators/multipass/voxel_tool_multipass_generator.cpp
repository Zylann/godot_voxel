#include "voxel_tool_multipass_generator.h"
#include "../../edition/funcs.h"
#include "../../util/containers/dynamic_bitset.h"
#include "../../util/godot/core/packed_arrays.h"
#include "../../util/string/format.h"

namespace zylann::voxel {

using namespace VoxelGeneratorMultipassCBStructs;

// Ref<VoxelToolMultipassGenerator> VoxelToolMultipassGenerator::create_offline(
// 		Vector3i grid_origin_blocks, Vector3i grid_size_blocks, Vector3i main_block_position, int block_size_po2) {
// 	Ref<VoxelToolMultipassGenerator> tool;
// 	tool.instantiate();
// 	tool->_offline_blocks.resize(Vector3iUtil::get_volume(grid_size_blocks));

// 	tool->_offline_block_pointers.reserve(tool->_offline_blocks.size());
// 	for (VoxelGeneratorMultipassCBStructs::Block &block : tool->_offline_blocks) {
// 		block.voxels.create(Vector3iUtil::create(1 << block_size_po2));
// 		tool->_offline_block_pointers.push_back(&block);
// 	}

// 	PassInput input;
// 	input.grid = to_span(tool->_offline_block_pointers);
// 	input.block_size = 1 << block_size_po2;
// 	input.main_block_position = main_block_position;
// 	input.grid_origin = grid_origin_blocks;
// 	input.grid_size = grid_size_blocks;
// 	tool->_pass_input = input;

// 	tool->_is_offline = true;

// 	return tool;
// }

void VoxelToolMultipassGenerator::set_pass_input(PassInput &pass_input) {
	// ZN_ASSERT(!_is_offline);

	_pass_input = pass_input;

	// TODO Perhaps we should always pass block size as a power of two index
	ZN_ASSERT(math::is_power_of_two(pass_input.block_size));
	_block_size_po2 = math::get_shift_from_power_of_two_32(pass_input.block_size);
	_block_size_mask = pass_input.block_size - 1;

	_editable_voxel_box =
			Box3i(_pass_input.grid_origin * _pass_input.block_size, _pass_input.grid_size * _pass_input.block_size);
}

bool VoxelToolMultipassGenerator::is_area_editable(const Box3i &box) const {
	return _editable_voxel_box.encloses(box);
}

namespace {
inline VoxelBuffer *get_pass_input_block(PassInput &pi, const Vector3i &bpos) {
	const Vector3i rbpos = bpos - pi.grid_origin;
	const Vector3i gs = pi.grid_size;
	if (rbpos.x < 0 || rbpos.y < 0 || rbpos.z < 0 || rbpos.x >= gs.x || rbpos.y >= gs.y || rbpos.z >= gs.z) {
		return nullptr;
	}
	const unsigned int loc = Vector3iUtil::get_zxy_index(rbpos, gs);
	return &pi.grid[loc]->voxels;
}

VoxelBuffer *get_pass_input_block_w(void *ctx, Vector3i bpos) {
	PassInput *pi = static_cast<PassInput *>(ctx);
	return get_pass_input_block(*pi, bpos);
}
// Just wrapping up for const...
const VoxelBuffer *get_pass_input_block_r(void *ctx, Vector3i bpos) {
	return get_pass_input_block_w(ctx, bpos);
}

struct GetPassInputBlock {
	PassInput &pi;
	VoxelBuffer *operator()(const Vector3i &bpos) const {
		return get_pass_input_block(pi, bpos);
	}
};

} // namespace

void VoxelToolMultipassGenerator::copy(Vector3i pos, VoxelBuffer &dst, uint8_t channels_mask) const {
	PassInput pass_input = _pass_input;
	copy_from_chunked_storage(dst, pos, _block_size_po2, channels_mask, &get_pass_input_block_r, &pass_input);
}

void VoxelToolMultipassGenerator::paste(Vector3i pos, const VoxelBuffer &src, uint8_t channels_mask) {
	paste_to_chunked_storage_tp(
			src, //
			pos,
			_block_size_po2,
			channels_mask,
			GetPassInputBlock{ _pass_input },
			paste_functors::Default()
	);
}

void VoxelToolMultipassGenerator::paste_masked(
		Vector3i pos,
		Ref<godot::VoxelBuffer> p_voxels,
		uint8_t channels_mask,
		uint8_t mask_channel,
		uint64_t mask_value
) {
	ZN_ASSERT_RETURN(p_voxels.is_valid());
	const VoxelBuffer &src = p_voxels->get_buffer();

	paste_to_chunked_storage_tp(
			src,
			pos,
			_block_size_po2,
			channels_mask,
			GetPassInputBlock{ _pass_input },
			paste_functors::SrcMasked{ mask_channel, mask_value }
	);
}

void VoxelToolMultipassGenerator::paste_masked_writable_list(
		Vector3i pos,
		Ref<godot::VoxelBuffer> p_voxels,
		uint8_t channels_mask,
		uint8_t src_mask_channel,
		uint64_t src_mask_value,
		uint8_t dst_mask_channel,
		PackedInt32Array dst_mask_values
) {
	ZN_ASSERT_RETURN(p_voxels.is_valid());
	ZN_ASSERT_RETURN(dst_mask_values.size() > 0);

	const VoxelBuffer &src = p_voxels->get_buffer();

	paste_to_chunked_storage_masked_writable_list(
			src,
			pos,
			_block_size_po2,
			channels_mask,
			GetPassInputBlock{ _pass_input },
			src_mask_channel,
			src_mask_value,
			dst_mask_channel,
			to_span(dst_mask_values)
	);
}

Block *VoxelToolMultipassGenerator::get_block_and_relative_position(
		Vector3i terrain_voxel_pos,
		Vector3i &out_voxel_rpos
) const {
	if (!_editable_voxel_box.contains(terrain_voxel_pos)) {
		return nullptr;
	}
	const Vector3i block_pos_world = terrain_voxel_pos >> _block_size_po2;
	const Vector3i block_pos_grid = block_pos_world - _pass_input.grid_origin;
	const int block_loc = Vector3iUtil::get_zxy_index(block_pos_grid, _pass_input.grid_size);
	Block *block = _pass_input.grid[block_loc];
	if (block == nullptr) {
		return nullptr;
	}
	out_voxel_rpos = terrain_voxel_pos & _block_size_mask;
	return block;
}

// These methods never go alone, but may be used in others.
// They don't represent an edit, they only abstract the lower-level API
uint64_t VoxelToolMultipassGenerator::_get_voxel(Vector3i pos) const {
	Vector3i rpos;
	Block *block = get_block_and_relative_position(pos, rpos);
	if (block == nullptr) {
		return 0;
	}
	return block->voxels.get_voxel(rpos, _channel);
}

float VoxelToolMultipassGenerator::_get_voxel_f(Vector3i pos) const {
	Vector3i rpos;
	Block *block = get_block_and_relative_position(pos, rpos);
	if (block == nullptr) {
		return constants::SDF_FAR_OUTSIDE;
	}
	return block->voxels.get_voxel_f(rpos, _channel);
}

void VoxelToolMultipassGenerator::_set_voxel(Vector3i pos, uint64_t v) {
	Vector3i rpos;
	Block *block = get_block_and_relative_position(pos, rpos);
	if (block == nullptr) {
		return;
	}
	block->voxels.set_voxel(v, rpos, _channel);
}

void VoxelToolMultipassGenerator::_set_voxel_f(Vector3i pos, float v) {
	Vector3i rpos;
	Block *block = get_block_and_relative_position(pos, rpos);
	if (block == nullptr) {
		return;
	}
	block->voxels.set_voxel_f(v, rpos, _channel);
}

void VoxelToolMultipassGenerator::_post_edit(const Box3i &box) {
	// Nothing to do
}

Vector3i VoxelToolMultipassGenerator::get_editable_area_min() const {
	return _editable_voxel_box.position;
}

Vector3i VoxelToolMultipassGenerator::get_editable_area_max() const {
	return _editable_voxel_box.position + _editable_voxel_box.size;
}

Vector3i VoxelToolMultipassGenerator::get_main_area_min() const {
	return _pass_input.main_block_position << _block_size_po2;
}

Vector3i VoxelToolMultipassGenerator::get_main_area_max() const {
	return (_pass_input.main_block_position + Vector3i(1, _pass_input.grid_size.y, 1)) << _block_size_po2;
}

void VoxelToolMultipassGenerator::do_path(Span<const Vector3> positions, Span<const float> radii) {
	struct GridAccess {
		PassInput *pass_input;
		unsigned int block_size_po2;
		inline VoxelBuffer *get_block(Vector3i bpos) {
			return get_pass_input_block(*pass_input, bpos);
		}
		inline unsigned int get_block_size_po2() const {
			return block_size_po2;
		}
	};

	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(positions.size() >= 2);
	ZN_ASSERT_RETURN(positions.size() == radii.size());

	// TODO Increase margin a bit with smooth voxels?
	const int margin = 1;

	// Compute total bounding box

	const AABB total_aabb = get_path_aabb(positions, radii).grow(margin);
	const Box3i total_voxel_box(to_vec3i(math::floor(total_aabb.position)), to_vec3i(math::ceil(total_aabb.size)));
	const Box3i clipped_voxel_box = total_voxel_box.clipped(_editable_voxel_box);

	// Rasterize

	for (unsigned int point_index = 1; point_index < positions.size(); ++point_index) {
		// TODO Could run this in local space so we dont need doubles
		// TODO Apply terrain scale
		const Vector3 p0 = positions[point_index - 1];
		const Vector3 p1 = positions[point_index];

		const float r0 = radii[point_index - 1];
		const float r1 = radii[point_index];

		ops::DoShapeChunked<ops::SdfRoundCone, GridAccess> op;
		op.shape.cone.a = p0;
		op.shape.cone.b = p1;
		op.shape.cone.r1 = r0;
		op.shape.cone.r2 = r1;
		op.box = op.shape.get_box().padded(margin);

		if (!op.box.intersects(clipped_voxel_box)) {
			continue;
		}

		op.shape.cone.update();
		op.shape.sdf_scale = get_sdf_scale();
		op.block_access.pass_input = &_pass_input;
		op.block_access.block_size_po2 = _block_size_po2;
		op.mode = ops::Mode(get_mode());
		op.texture_params = _texture_params;
		op.blocky_value = _value;
		op.channel = get_channel();
		op.strength = get_sdf_strength();

		op();
	}
}

void VoxelToolMultipassGenerator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_editable_area_min"), &VoxelToolMultipassGenerator::get_editable_area_min);
	ClassDB::bind_method(D_METHOD("get_editable_area_max"), &VoxelToolMultipassGenerator::get_editable_area_max);

	ClassDB::bind_method(D_METHOD("get_main_area_min"), &VoxelToolMultipassGenerator::get_main_area_min);
	ClassDB::bind_method(D_METHOD("get_main_area_max"), &VoxelToolMultipassGenerator::get_main_area_max);

	// ClassDB::bind_static_method(VoxelToolMultipassGenerator::get_class_static(),
	// 		D_METHOD("create_offline", "grid_origin_blocks", "grid_size_blocks", "main_block_position",
	// 				"block_size_po2"),
	// 		&VoxelToolMultipassGenerator::create_offline);
}

} // namespace zylann::voxel
