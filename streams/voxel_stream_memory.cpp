#include "voxel_stream_memory.h"
#include "../storage/voxel_buffer.h"
#include "../util/containers/std_unordered_map.h"
#include "../util/thread/thread.h"
#include "instance_data.h"

namespace zylann::voxel {

void VoxelStreamMemory::load_voxel_blocks(Span<VoxelQueryData> p_blocks) {
	for (VoxelQueryData &q : p_blocks) {
		Lod &lod = _lods[q.lod_index];

		MutexLock mlock(lod.mutex);
		auto it = lod.voxel_blocks.find(q.position_in_blocks);

		if (it == lod.voxel_blocks.end()) {
			q.result = VoxelStream::RESULT_BLOCK_NOT_FOUND;

		} else {
			q.result = VoxelStream::RESULT_BLOCK_FOUND;
			it->second.copy_to(q.voxel_buffer, true);
		}
	}
}

void VoxelStreamMemory::save_voxel_blocks(Span<VoxelQueryData> p_blocks) {
	for (const VoxelQueryData &q : p_blocks) {
		if (_artificial_save_latency_usec > 0) {
			Thread::sleep_usec(_artificial_save_latency_usec);
		}
		Lod &lod = _lods[q.lod_index];
		{
			MutexLock mlock(lod.mutex);
			VoxelBuffer &dst = lod.voxel_blocks[q.position_in_blocks];
			q.voxel_buffer.copy_to(dst, true);
		}
	}
}

void VoxelStreamMemory::load_voxel_block(VoxelQueryData &query_data) {
	load_voxel_blocks(Span<VoxelQueryData>(&query_data, 1));
}

void VoxelStreamMemory::save_voxel_block(VoxelQueryData &query_data) {
	save_voxel_blocks(Span<VoxelQueryData>(&query_data, 1));
}

bool VoxelStreamMemory::supports_instance_blocks() const {
	return true;
}

void VoxelStreamMemory::load_instance_blocks(Span<InstancesQueryData> out_blocks) {
	for (InstancesQueryData &q : out_blocks) {
		Lod &lod = _lods[q.lod_index];

		MutexLock mlock(lod.mutex);
		auto it = lod.instance_blocks.find(q.position_in_blocks);

		if (it == lod.instance_blocks.end()) {
			q.result = VoxelStream::RESULT_BLOCK_NOT_FOUND;

		} else {
			// Copying is required since the cache has ownership on its data
			q.data = make_unique_instance<InstanceBlockData>();
			it->second.copy_to(*q.data);
		}
	}
}

void VoxelStreamMemory::save_instance_blocks(Span<InstancesQueryData> p_blocks) {
	for (const InstancesQueryData &q : p_blocks) {
		Lod &lod = _lods[q.lod_index];
		MutexLock mlock(lod.mutex);

		if (q.data == nullptr) {
			lod.instance_blocks.erase(q.position_in_blocks);
		} else {
			q.data->copy_to(lod.instance_blocks[q.position_in_blocks]);
		}
	}
}

bool VoxelStreamMemory::supports_loading_all_blocks() const {
	return true;
}

void VoxelStreamMemory::load_all_blocks(FullLoadingResult &result) {
	// The return value couples instances and voxels, but our storage is decoupled, so it complicates things a bit
	StdUnorderedMap<Vector3i, unsigned int> bpos_to_index;

	for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		const Lod &lod = _lods[lod_index];
		bpos_to_index.clear();

		MutexLock mlock(lod.mutex);

		for (auto it = lod.voxel_blocks.begin(); it != lod.voxel_blocks.end(); ++it) {
			const Vector3i bpos = it->first;
			const unsigned int index = result.blocks.size();
			bpos_to_index.insert({ bpos, index });
			result.blocks.resize(index + 1);

			FullLoadingResult::Block &block = result.blocks[index];
			block.position = bpos;
			block.lod = lod_index;

			const VoxelBuffer &src = it->second;
			block.voxels = make_shared_instance<VoxelBuffer>();
			src.copy_to(*block.voxels, true);
		}

		for (auto it = lod.instance_blocks.begin(); it != lod.instance_blocks.end(); ++it) {
			const Vector3i bpos = it->first;
			FullLoadingResult::Block *block = nullptr;
			auto pos_it = bpos_to_index.find(bpos);
			if (pos_it == bpos_to_index.end()) {
				const unsigned int index = result.blocks.size();
				result.blocks.resize(index + 1);
				block = &result.blocks.back();
				block->position = bpos;
				block->lod = lod_index;
				// bpos_to_index.insert({ bpos, index });
			} else {
				const unsigned int index = pos_it->second;
				block = &result.blocks[index];
			}

			block->instances_data = make_unique_instance<InstanceBlockData>();
			it->second.copy_to(*block->instances_data);
		}
	}
}

int VoxelStreamMemory::get_used_channels_mask() const {
	return VoxelBuffer::ALL_CHANNELS_MASK;
}

int VoxelStreamMemory::get_lod_count() const {
	return _lods.size();
}

void VoxelStreamMemory::set_artificial_save_latency_usec(int usec) {
	ZN_ASSERT_RETURN(usec >= 0);
	_artificial_save_latency_usec = usec;
}

int VoxelStreamMemory::get_artificial_save_latency_usec() const {
	return _artificial_save_latency_usec;
}

void VoxelStreamMemory::_bind_methods() {
	ClassDB::bind_method(
			D_METHOD("set_artificial_save_latency_usec", "usec"), &VoxelStreamMemory::set_artificial_save_latency_usec);
	ClassDB::bind_method(
			D_METHOD("get_artificial_save_latency_usec"), &VoxelStreamMemory::get_artificial_save_latency_usec);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "artificial_save_latency_usec"), "set_artificial_save_latency_usec",
			"get_artificial_save_latency_usec");
}

} // namespace zylann::voxel
