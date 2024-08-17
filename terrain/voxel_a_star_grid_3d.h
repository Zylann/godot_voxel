#ifndef VOXEL_A_STAR_GRID_3D_H
#define VOXEL_A_STAR_GRID_3D_H

#include "../storage/voxel_buffer.h"
#include "../storage/voxel_data.h"
#include "../util/a_star_grid_3d.h"
#include "../util/containers/dynamic_bitset.h"
#include "../util/containers/std_vector.h"
#include <atomic>

namespace zylann::voxel {

class VoxelTerrain;

class VoxelAStarGrid3DInternal : public AStarGrid3D {
public:
	VoxelAStarGrid3DInternal();

	// Referring to VoxelData instead of using a VoxelTool because it allows to run the search in a threaded task.
	// VoxelTool can't be used yet in threads because it holds a pointer to a terrain node, which could get deleted at
	// any time.
	std::shared_ptr<VoxelData> data;

	void init_cache();

protected:
	bool is_solid(Vector3i pos) override;

private:
	// We store a cache of solid bits for the whole pathfindable region.
	// To minimize multithreaded access to the main voxel data, we only load chunks of bits as they are needed.

	struct Chunk {
		static const int SIZE_PO2 = 2;
		static const int SIZE = 1 << SIZE_PO2;
		static const int SIZE_MASK = SIZE - 1;

		// 4x4x4 bits in ZXY order
		uint64_t solid_bits = 0;

		inline bool get_solid_bit(Vector3i rel) const {
			const uint64_t i = Vector3iUtil::get_zxy_index(rel, Vector3i(SIZE, SIZE, SIZE));
			// const unsigned int i = rel.y + (rel.x << 2) + (rel.z << 4);
			return ((solid_bits >> i) & uint64_t(1)) != 0;
		}
	};

	// Cached 3D bitmap
	StdVector<Chunk> _grid_cache;
	Vector3i _grid_cache_size;

	// Tracks which chunks are loaded
	DynamicBitset _grid_chunk_states;

	// Temporary buffer used to read voxels from the main voxel storage
	VoxelBuffer _voxel_buffer;
};

// Godot-facing API for voxel grid A* pathfinding. Suitable for blocky terrains.
class VoxelAStarGrid3D : public RefCounted {
	GDCLASS(VoxelAStarGrid3D, RefCounted)
public:
	// Bare bones at the moment. May need more configurations and customization.
	// Also, it does not cache data between queries.

	void set_terrain(VoxelTerrain *node);

	void set_region(Box3i region);
	Box3i get_region();

	TypedArray<Vector3i> find_path(Vector3i from_position, Vector3i to_position);

	void find_path_async(Vector3i from_position, Vector3i to_position);
	bool is_running_async() const;

	TypedArray<Vector3i> debug_get_visited_positions() const;

private:
	TypedArray<Vector3i> find_path_internal(Vector3i from_position, Vector3i to_position);
#ifdef DEBUG_ENABLED
	void check_params(Vector3i from_position, Vector3i to_position);
#endif

	void _b_set_region(AABB aabb);
	AABB _b_get_region();
	void _b_on_async_search_completed(TypedArray<Vector3i> path);

	static void _bind_methods();

	VoxelAStarGrid3DInternal _path_finder;
	std::atomic_bool _is_running_async = { false };
};

} // namespace zylann::voxel

#endif // VOXEL_A_STAR_GRID_3D_H
