#ifndef VOXEL_GENERATOR_MULTIPASS_CB_STRUCTS_H
#define VOXEL_GENERATOR_MULTIPASS_CB_STRUCTS_H

#include "../../storage/voxel_buffer.h"
#include "../../util/containers/small_vector.h"
#include "../../util/containers/span.h"
#include "../../util/containers/std_unordered_map.h"
#include "../../util/containers/std_vector.h"
#include "../../util/math/vector2i.h"
#include "../../util/math/vector3i.h"
#include "../../util/ref_count.h"
#include "../../util/thread/mutex.h"
#include "../../util/thread/spatial_lock_2d.h"

#include <utility>

// Data structures used internally in multipass generation.

// Had to separate these structs outside `VoxelGeneratorMultipassCB`, because adding Godot virtual method bindings
// required to #include `VoxelToolMultipassGenerator`, which already included the former to access its nested structs,
// so it could not compile. So I moved the nested structs outside the class.

namespace zylann {

class IThreadedTask;

namespace voxel {
namespace VoxelGeneratorMultipassCBStructs {

// Pass limit is pretty low because in practice not that many should be needed, and it gets expensive really quick
static constexpr int MAX_PASSES = 4;
static constexpr int MAX_PASS_EXTENT = 2;

// Passes are internally split into subpasses: 1 subpass for pass 0, 2 subpasses for the others. Each subpass
// depends on the previous. This is to cover side-effect of passes that can modify neighbors.
static constexpr int MAX_SUBPASSES = MAX_PASSES * 2 - 1; // get_subpass_count_from_pass_count(MAX_PASSES);
// I would have gladly used a constexpr function but apparently if it's a static method it doesn't work unless it is
// moved outside of the class, which is inconvenient

struct Block {
	VoxelBuffer voxels;

	// If set, this task will be scheduled when the block's generation is complete.
	// Only a non-scheduled, non-running task can be referenced here.
	// These tasks also must NOT have spawned subtasks. That is, they are only owned by the block while they are
	// here.
	// In other words, we use this pointer to prevent asking columns to generate multiple times, because there can
	// be multiple block requests for one column.
	IThreadedTask *final_pending_task = nullptr;

	Block() {}

	Block(Block &&other) {
		voxels = std::move(other.voxels);
		final_pending_task = other.final_pending_task;
	}

	Block &operator=(Block &&other) {
		voxels = std::move(other.voxels);
		final_pending_task = other.final_pending_task;
		return *this;
	}

	~Block() {
		ZN_ASSERT_RETURN_MSG(final_pending_task == nullptr, "Unhandled task leaked!");
	}
};

struct Column {
	RefCount viewers;
	// Index of the last subpass that was executed directly on this chunk.
	// -1 means the chunk just got created and no subpass has run on it yet.
	int8_t subpass_index = -1;
	bool saving = false;
	bool loading = false;

	// Each bit is set to 1 when a task is pending to process this block at a given subpass.
	uint8_t pending_subpass_tasks_mask = 0;

	// Currently unused, because if chunks get removed from the cache or don't get saved for any reason,
	// it can become out of sync and we wouldn't know. It would be a nice optimization tho...
	//
	// Caches how many chunks in the neighborhood have been processed while having access to this one (included).
	// This is an optimization allowing to reduce chunk map queries. This is an array because it is possible
	// for a subpass to start writing into a block that completed its previous subpass but hasn't started the next
	// subpass.
	// FixedArray<uint8_t, MAX_SUBPASSES> subpass_iterations;

	// Vertical stack of blocks. Must never be resized, except when loaded or unloaded.
	// TODO Maybe replace with a dynamic non-resizeable array?
	StdVector<Block> blocks;

	// Column() {
	// 	fill(subpass_iterations, uint8_t(0));
	// }
};

struct Map {
	StdUnorderedMap<Vector2i, Column> columns;
	// Protects the hashmap itself
	Mutex mutex;
	// Protects columns
	mutable SpatialLock2D spatial_lock;

	~Map() {
		// If the map gets destroyed then we know the last reference to it was removed, which means only one thread had
		// access to it, so we can get away not locking anything if cleanup is needed.

		// Initially I thought this is where we'd re-schedule any task pointers still in blocks, but that can't happen,
		// because if tasks exist referencing the multipass generator, then the generator (and so the map) would not be
		// destroyed, and so we wouldn't be here.
		// Therefore there is only just an assert in ~Block to ensure the only other case where blocks are removed from
		// the map before handling these tasks, or to warn us in case there is a case we didn't expect.
		//
		// That said, that pretty much describes a cyclic reference. This cycle is usually broken by VoxelTerrain, which
		// controls the streaming behavior. If a terrain gets destroyed, it must tell the generator to unload its cache.
	}
};

struct Pass {
	// How many blocks to load using the previous pass around the generated area, so that neighbors can be accessed
	// in case the pass has effects across blocks.
	// Constraints: the first pass cannot have dependencies; other passes must have dependencies.
	int8_t dependency_extents = 0;
};

// Internal state of the generator.
struct Internal {
	// Map used solely for generation purposes. It acts like a cache so we don't recompute the same passes many
	// times. In theory we could generate blocks without caching or streaming anything, but that would mean every
	// block request would have to repeatedly generate so much data around it (column, neighbors, neighbors of
	// neighbors...), it would be prohibitively slow.
	// As a cache, it can be deleted anytime (taking care of thread safety ofc).
	Map map;

	// Params: they should never change. Changing them involves making a whole new instance.
	SmallVector<Pass, MAX_PASSES> passes;
	int column_base_y_blocks = -4;
	int column_height_blocks = 8;

	// Set to `true` if the generator's configuration changed. Means a new instance of Internal has been made.
	// Existing tasks may still finish their work using the old instance, but results will be thrown away. Such
	// tasks can end faster if they check this boolean.
	bool expired = false;

	Internal() {
		// 1 pass minimum
		passes.push_back(Pass());
	}

	inline void copy_params(const Internal &other) {
		passes = other.passes;
		column_base_y_blocks = other.column_base_y_blocks;
		column_height_blocks = other.column_height_blocks;
	}
};

struct PassInput {
	// 3D grid of blocks as a linear sequence, ZXY order
	Span<Block *> grid;
	Vector3i grid_size;
	// Position of the grid's lower corner.
	Vector3i grid_origin;
	// Position of the main block or column in world coordinates.
	Vector3i main_block_position;
	int block_size = 0;
	int pass_index = 0;
};

} // namespace VoxelGeneratorMultipassCBStructs
} // namespace voxel
} // namespace zylann

#endif // VOXEL_GENERATOR_MULTIPASS_CB_STRUCTS_H
