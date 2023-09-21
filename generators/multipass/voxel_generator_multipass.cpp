#include "voxel_generator_multipass.h"

#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"

namespace zylann::voxel {

VoxelGeneratorMultipass::VoxelGeneratorMultipass() {
	_map = make_shared_instance<VoxelGeneratorMultipass::Map>();

	// PLACEHOLDER
	{
		Pass pass;
		_passes.push_back(pass);
	}
	{
		Pass pass;
		pass.dependency_extents = BoxBounds3i(Vector3i(-1, -1, -1), Vector3i(1, 1, 1));
		_passes.push_back(pass);
	}
	{
		Pass pass;
		pass.dependency_extents = BoxBounds3i(Vector3i(-1, -1, -1), Vector3i(1, 1, 1));
		_passes.push_back(pass);
	}
}

VoxelGenerator::Result VoxelGeneratorMultipass::generate_block(VoxelQueryData &input) {
	// TODO Fallback on an expensive single-threaded dependency generation, which we might throw away after
	ZN_PRINT_ERROR("Not implemented");
	return { false };
}

namespace {
// TODO Placeholder utility to emulate what would be the final use case
struct GridEditor {
	Span<std::shared_ptr<VoxelGeneratorMultipass::Block>> blocks;
	Box3i world_box;

	inline std::shared_ptr<VoxelGeneratorMultipass::Block> get_block_and_relative_position(
			Vector3i voxel_pos_world, Vector3i &voxel_pos_block) {
		const Vector3i block_pos_world = voxel_pos_world >> 4;
		ZN_ASSERT(world_box.contains(block_pos_world));
		const Vector3i block_pos_grid = block_pos_world - world_box.pos;
		const unsigned int grid_loc = Vector3iUtil::get_zxy_index(block_pos_grid, world_box.size);
		std::shared_ptr<VoxelGeneratorMultipass::Block> block = blocks[grid_loc];
		ZN_ASSERT(block != nullptr);
		voxel_pos_block = voxel_pos_world & 15;
		return block;
	}

	int get_voxel(Vector3i voxel_pos_world, int value, int channel) {
		Vector3i voxel_pos_block;
		std::shared_ptr<VoxelGeneratorMultipass::Block> block =
				get_block_and_relative_position(voxel_pos_world, voxel_pos_block);
		return block->voxels.get_voxel(voxel_pos_block, channel);
	}

	void set_voxel(Vector3i voxel_pos_world, int value, int channel) {
		Vector3i voxel_pos_block;
		std::shared_ptr<VoxelGeneratorMultipass::Block> block =
				get_block_and_relative_position(voxel_pos_world, voxel_pos_block);
		block->voxels.set_voxel(value, voxel_pos_block, channel);
	}
};
} // namespace

void VoxelGeneratorMultipass::generate_pass(PassInput input) {
	// PLACEHOLDER
	const Vector3i origin_in_voxels = input.main_block_position * 16;
	static const VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_TYPE;

	if (input.pass_index == 0) {
		Ref<ZN_FastNoiseLite> fnl;
		fnl.instantiate();
		// Pass 0 has no neighbor access so we have just one block
		std::shared_ptr<Block> block = input.grid[0];
		ZN_ASSERT(block != nullptr);
		VoxelBufferInternal &voxels = block->voxels;
		Vector3i rpos;
		const Vector3i bs = voxels.get_size();

		for (rpos.z = 0; rpos.z < bs.z; ++rpos.z) {
			for (rpos.x = 0; rpos.x < bs.x; ++rpos.x) {
				for (rpos.y = 0; rpos.y < bs.y; ++rpos.y) {
					Vector3i pos = origin_in_voxels + rpos;
					const float sd = pos.y + 5.f * fnl->get_noise_2d(pos.x, pos.z);

					int v = 0;
					if (sd < 0.f) {
						v = 1;
					}

					voxels.set_voxel(v, rpos, channel);
				}
			}
		}

		voxels.compress_uniform_channels();

	} else if (input.pass_index == 1) {
		GridEditor editor;
		editor.blocks = input.grid;
		editor.world_box = Box3i(input.grid_origin, input.grid_size);

		const uint32_t h = Variant(input.grid_origin).hash();

		Vector3i structure_center;
		structure_center.x = h & 15;
		structure_center.y = (h >> 4) & 15;
		structure_center.z = (h >> 8) & 15;
		const int structure_size = (h >> 12) & 15;

		const Box3i structure_box(structure_center - Vector3iUtil::create(structure_size / 2) + origin_in_voxels,
				Vector3iUtil::create(structure_size));

		structure_box.for_each_cell_zxy([&editor](Vector3i pos) { //
			editor.set_voxel(pos, 1, channel);
		});

	} else if (input.pass_index == 2) {
		// blep
		Thread::sleep_usec(1000);
	}
}

} // namespace zylann::voxel
