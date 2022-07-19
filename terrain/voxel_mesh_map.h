#ifndef VOXEL_MESH_MAP_H
#define VOXEL_MESH_MAP_H

#include "../engine/voxel_engine.h"
#include "../util/macros.h"

#include <unordered_map>
#include <vector>

namespace zylann::voxel {

// Stores meshes and colliders in an infinite sparse grid of chunks (aka blocks).
template <typename MeshBlock_T>
class VoxelMeshMap {
public:
	VoxelMeshMap() : _last_accessed_block(nullptr) {}

	~VoxelMeshMap() {
		clear();
	}

	struct NoAction {
		inline void operator()(MeshBlock_T &block) {}
	};

	template <typename Action_T>
	void remove_block(Vector3i bpos, Action_T pre_delete) {
		if (_last_accessed_block && _last_accessed_block->position == bpos) {
			_last_accessed_block = nullptr;
		}
		auto it = _blocks_map.find(bpos);
		if (it != _blocks_map.end()) {
			const unsigned int i = it->second.index;
#ifdef DEBUG_ENABLED
			CRASH_COND(i >= _blocks.size());
#endif
			MeshBlock_T *block = _blocks[i];
			ERR_FAIL_COND(block == nullptr);
			pre_delete(*block);
			queue_free_mesh_block(block);
			remove_block_internal(it, i);
		}
	}

	MeshBlock_T *get_block(Vector3i bpos) {
		if (_last_accessed_block && _last_accessed_block->position == bpos) {
			return _last_accessed_block;
		}
		auto it = _blocks_map.find(bpos);
		if (it != _blocks_map.end()) {
#ifdef DEBUG_ENABLED
			const unsigned int i = it->second.index;
			CRASH_COND(i >= _blocks.size());
			MeshBlock_T *block = _blocks[i];
			CRASH_COND(block == nullptr); // The map should not contain null blocks
			CRASH_COND(it->second.block == nullptr);
#endif
			_last_accessed_block = it->second.block;
			return _last_accessed_block;
		}
		return nullptr;
	}

	const MeshBlock_T *get_block(Vector3i bpos) const {
		if (_last_accessed_block != nullptr && _last_accessed_block->position == bpos) {
			return _last_accessed_block;
		}
		auto it = _blocks_map.find(bpos);
		if (it != _blocks_map.end()) {
#ifdef DEBUG_ENABLED
			const unsigned int i = it->second.index;
			CRASH_COND(i >= _blocks.size());
			MeshBlock_T *block = _blocks[i];
			CRASH_COND(block == nullptr); // The map should not contain null blocks
			CRASH_COND(it->second.block == nullptr);
#endif
			// This function can't cache _last_accessed_block, because it's const, so repeated accesses are hashing
			// again...
			return it->second.block;
		}
		return nullptr;
	}

	void set_block(Vector3i bpos, MeshBlock_T *block) {
		ERR_FAIL_COND(block == nullptr);
		CRASH_COND(bpos != block->position);
		if (_last_accessed_block == nullptr || _last_accessed_block->position == bpos) {
			_last_accessed_block = block;
		}
#ifdef DEBUG_ENABLED
		CRASH_COND(has_block(bpos));
#endif
		unsigned int i = _blocks.size();
		_blocks.push_back(block);
		_blocks_map.insert({ bpos, { block, i } });
	}

	bool has_block(Vector3i pos) const {
		//(_last_accessed_block != nullptr && _last_accessed_block->pos == pos) ||
		return _blocks_map.find(pos) != _blocks_map.end();
	}

	void clear() {
		for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
			MeshBlock_T *block = *it;
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

	unsigned int get_block_count() const {
#ifdef DEBUG_ENABLED
		const unsigned int blocks_map_size = _blocks_map.size();
		CRASH_COND(_blocks.size() != blocks_map_size);
#endif
		return _blocks.size();
	}

	template <typename Op_T>
	inline void for_each_block(Op_T op) {
		for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
			MeshBlock_T *block = *it;
#ifdef DEBUG_ENABLED
			CRASH_COND(block == nullptr);
#endif
			op(*block);
		}
	}

	template <typename Op_T>
	inline void for_each_block(Op_T op) const {
		for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
			const MeshBlock_T *block = *it;
#ifdef DEBUG_ENABLED
			CRASH_COND(block == nullptr);
#endif
			op(*block);
		}
	}

private:
	struct MapItem {
		MeshBlock_T *block;
		// Index of the block within the vector storage
		unsigned int index;
	};

	void remove_block_internal(typename std::unordered_map<Vector3i, MapItem>::iterator rm_it, unsigned int index) {
		// TODO `erase` can occasionally be very slow (milliseconds) if the map contains lots of items.
		// This might be caused by internal rehashing/resizing.
		// We should look for a faster container, or reduce the number of entries.

		// This function assumes the block is already freed
		_blocks_map.erase(rm_it);

		MeshBlock_T *moved_block = _blocks.back();
#ifdef DEBUG_ENABLED
		CRASH_COND(index >= _blocks.size());
#endif
		_blocks[index] = moved_block;
		_blocks.pop_back();

		if (index < _blocks.size()) {
			auto moved_block_index_it = _blocks_map.find(moved_block->position);
			CRASH_COND(moved_block_index_it == _blocks_map.end());
			moved_block_index_it->second.index = index;
		}
	}

	static void queue_free_mesh_block(MeshBlock_T *block) {
		// We spread this out because of physics
		// TODO Could it be enough to do both render and physic deallocation with the task in ~MeshBlock_T()?
		struct FreeMeshBlockTask : public zylann::ITimeSpreadTask {
			void run(TimeSpreadTaskContext &ctx) override {
				memdelete(block);
			}
			MeshBlock_T *block = nullptr;
		};
		ERR_FAIL_COND(block == nullptr);
		FreeMeshBlockTask *task = memnew(FreeMeshBlockTask);
		task->block = block;
		VoxelEngine::get_singleton().push_main_thread_time_spread_task(task);
	}

private:
	// Blocks stored with a spatial hash in all 3D directions.
	std::unordered_map<Vector3i, MapItem> _blocks_map;
	// Blocks are stored in a vector to allow faster iteration over all of them.
	// Use cases for this include updating the transform of the meshes
	std::vector<MeshBlock_T *> _blocks;

	// Voxel access will most frequently be in contiguous areas, so the same blocks are accessed.
	// To prevent too much hashing, this reference is checked before.
	mutable MeshBlock_T *_last_accessed_block;
};

} // namespace zylann::voxel

#endif // VOXEL_MESH_BLOCK_MAP_H
