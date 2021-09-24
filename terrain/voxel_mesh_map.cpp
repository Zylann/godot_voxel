#include "voxel_mesh_map.h"
#include "../constants/cube_tables.h"
#include "../constants/voxel_constants.h"
#include "../server/voxel_server.h"
#include "../util/macros.h"

#include <limits>

VoxelMeshMap::VoxelMeshMap() :
		_last_accessed_block(nullptr) {
	// TODO Make it configurable in editor (with all necessary notifications and updatings!)
	set_block_size_pow2(VoxelConstants::DEFAULT_BLOCK_SIZE_PO2);
}

VoxelMeshMap::~VoxelMeshMap() {
	clear();
}

void VoxelMeshMap::create(unsigned int block_size_po2, int lod_index) {
	clear();
	set_block_size_pow2(block_size_po2);
	set_lod_index(lod_index);
}

void VoxelMeshMap::set_block_size_pow2(unsigned int p) {
	ERR_FAIL_COND_MSG(p < 1, "Block size is too small");
	ERR_FAIL_COND_MSG(p > 8, "Block size is too big");

	_block_size_pow2 = p;
	_block_size = 1 << _block_size_pow2;
	_block_size_mask = _block_size - 1;
}

void VoxelMeshMap::set_lod_index(int lod_index) {
	ERR_FAIL_COND_MSG(lod_index < 0, "LOD index can't be negative");
	ERR_FAIL_COND_MSG(lod_index >= 32, "LOD index is too big");

	_lod_index = lod_index;
}

unsigned int VoxelMeshMap::get_lod_index() const {
	return _lod_index;
}

// VoxelMeshBlock *VoxelMeshMap::create_default_block(Vector3i bpos) {
// 	VoxelMeshBlock *block = VoxelMeshBlock::create(bpos, _block_size, _lod_index);
// 	set_block(bpos, block);
// 	return block;
// }

// VoxelMeshBlock *VoxelMeshMap::get_or_create_block_at_voxel_pos(Vector3i pos) {
// 	Vector3i bpos = voxel_to_block(pos);
// 	VoxelMeshBlock *block = get_block(bpos);
// 	if (block == nullptr) {
// 		block = create_default_block(bpos);
// 	}
// 	return block;
// }

VoxelMeshBlock *VoxelMeshMap::get_block(Vector3i bpos) {
	if (_last_accessed_block && _last_accessed_block->position == bpos) {
		return _last_accessed_block;
	}
	unsigned int *iptr = _blocks_map.getptr(bpos);
	if (iptr != nullptr) {
		const unsigned int i = *iptr;
#ifdef DEBUG_ENABLED
		CRASH_COND(i >= _blocks.size());
#endif
		VoxelMeshBlock *block = _blocks[i];
		CRASH_COND(block == nullptr); // The map should not contain null blocks
		_last_accessed_block = block;
		return _last_accessed_block;
	}
	return nullptr;
}

const VoxelMeshBlock *VoxelMeshMap::get_block(Vector3i bpos) const {
	if (_last_accessed_block != nullptr && _last_accessed_block->position == bpos) {
		return _last_accessed_block;
	}
	const unsigned int *iptr = _blocks_map.getptr(bpos);
	if (iptr != nullptr) {
		const unsigned int i = *iptr;
#ifdef DEBUG_ENABLED
		CRASH_COND(i >= _blocks.size());
#endif
		// TODO This function can't cache _last_accessed_block, because it's const, so repeated accesses are hashing again...
		const VoxelMeshBlock *block = _blocks[i];
		CRASH_COND(block == nullptr); // The map should not contain null blocks
		return block;
	}
	return nullptr;
}

void VoxelMeshMap::set_block(Vector3i bpos, VoxelMeshBlock *block) {
	ERR_FAIL_COND(block == nullptr);
	CRASH_COND(bpos != block->position);
	if (_last_accessed_block == nullptr || _last_accessed_block->position == bpos) {
		_last_accessed_block = block;
	}
#ifdef DEBUG_ENABLED
	CRASH_COND(_blocks_map.has(bpos));
#endif
	unsigned int i = _blocks.size();
	_blocks.push_back(block);
	_blocks_map.set(bpos, i);
}

void VoxelMeshMap::remove_block_internal(Vector3i bpos, unsigned int index) {
	// TODO `erase` can occasionally be very slow (milliseconds) if the map contains lots of items.
	// This might be caused by internal rehashing/resizing.
	// We should look for a faster container, or reduce the number of entries.

	// This function assumes the block is already freed
	_blocks_map.erase(bpos);

	VoxelMeshBlock *moved_block = _blocks.back();
#ifdef DEBUG_ENABLED
	CRASH_COND(index >= _blocks.size());
#endif
	_blocks[index] = moved_block;
	_blocks.pop_back();

	if (index < _blocks.size()) {
		unsigned int *moved_block_index = _blocks_map.getptr(moved_block->position);
		CRASH_COND(moved_block_index == nullptr);
		*moved_block_index = index;
	}
}

void VoxelMeshMap::queue_free_mesh_block(VoxelMeshBlock *block) {
	struct FreeMeshBlockTask : public IVoxelTimeSpreadTask {
		void run() override {
			memdelete(block);
		}
		VoxelMeshBlock *block = nullptr;
	};
	ERR_FAIL_COND(block == nullptr);
	FreeMeshBlockTask *task = memnew(FreeMeshBlockTask);
	task->block = block;
	VoxelServer::get_singleton()->push_time_spread_task(task);
}

bool VoxelMeshMap::has_block(Vector3i pos) const {
	return /*(_last_accessed_block != nullptr && _last_accessed_block->pos == pos) ||*/ _blocks_map.has(pos);
}

bool VoxelMeshMap::is_block_surrounded(Vector3i pos) const {
	// TODO If that check proves to be too expensive with all blocks we deal with, cache it in VoxelBlocks
	for (unsigned int i = 0; i < Cube::MOORE_NEIGHBORING_3D_COUNT; ++i) {
		Vector3i bpos = pos + Cube::g_moore_neighboring_3d[i];
		if (!has_block(bpos)) {
			return false;
		}
	}
	return true;
}

void VoxelMeshMap::clear() {
	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		VoxelMeshBlock *block = *it;
		if (block == nullptr) {
			ERR_PRINT("Unexpected nullptr in VoxelMap::clear()");
		} else {
			memdelete(block);
		}
	}
	_blocks.clear();
	_blocks_map.clear();
	_last_accessed_block = nullptr;
}

int VoxelMeshMap::get_block_count() const {
#ifdef DEBUG_ENABLED
	const unsigned int blocks_map_size = _blocks_map.size();
	CRASH_COND(_blocks.size() != blocks_map_size);
#endif
	return _blocks.size();
}
