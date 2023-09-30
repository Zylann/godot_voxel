#ifndef VOXEL_GENERATOR_MULTIPASS_H
#define VOXEL_GENERATOR_MULTIPASS_H

#include "../../engine/ids.h"
#include "../../storage/voxel_buffer_internal.h"
#include "../../storage/voxel_spatial_lock.h"
#include "../../util/ref_count.h"
#include "../voxel_generator.h"

namespace zylann {

class IThreadedTask;

namespace voxel {

// Generator using multiple passes that can be executed in parallel.
//
// Each pass works on chunks and can access results of the previous passes, including neighor chunks.
// In order to be efficient, execution and data structures are fundamentally different than other generators.
// That makes it incompatible with other features of the engine, and better suited for other use cases.
class VoxelGeneratorMultipass : public VoxelGenerator {
	GDCLASS(VoxelGeneratorMultipass, VoxelGenerator)
public:
	// TODO Enforce limits
	// Pass limit is pretty low because in practice not that many should be needed, and it gets expensive really quick
	static const int MAX_PASSES = 4;
	// static const int MAX_PASS_EXTENT = 2;

	// Passes are internally split into subpasses: 1 subpass for pass 0, 2 subpasses for the others. Each subpass
	// depends on the previous. This is to cover side-effect of passes that can modify neighbors.
	static const int MAX_SUBPASSES = MAX_PASSES * 2 - 1; // get_subpass_count_from_pass_count(MAX_PASSES);
	// I would have gladly used a constexpr function but apparently if it's a static method it doesn't work unless it is
	// moved outside of the class, which is inconvenient

	static inline int get_subpass_count_from_pass_count(int pass_count) {
		return pass_count * 2 - 1;
	}

	static inline int get_pass_index_from_subpass(int subpass_index) {
		return (subpass_index + 1) / 2;
	}

	struct Block {
		VoxelBufferInternal voxels;
		RefCount viewers;
		// Index of the last subpass that was executed directly on this chunk.
		// -1 means the chunk just got created and no subpass has run on it yet.
		int8_t subpass_index = -1;
		bool saving = false;
		bool loading = false;

		// Each bit is set to 1 when a task is pending to process this block at a given subpass.
		uint8_t pending_subpass_tasks_mask = 0;

		// If set, this task will be scheduled when the block's generation is complete.
		IThreadedTask *final_pending_task = nullptr;

		// Currently unused, because if chunks get removed from the cache or don't get saved for any reason,
		// it can become out of sync and we wouldn't know. It would be a nice optimization tho...
		//
		// Caches how many chunks in the neighborhood have been processed while having access to this one (included).
		// This is an optimization allowing to reduce chunk map queries. This is an array because it is possible
		// for a subpass to start writing into a block that completed its previous subpass but hasn't started the next
		// subpass.
		FixedArray<uint8_t, MAX_SUBPASSES> subpass_iterations;

		Block() {
			fill(subpass_iterations, uint8_t(0));
		}

		~Block() {
			ZN_ASSERT(final_pending_task == nullptr);
		}
	};

	struct Map {
		// TODO We actually don't need shared_ptrs, as long as we use pointers while holding a write spatial lock.
		// Pointers to elements of an unordered_map remain stable, unless such elements are removed, which should
		// require acquiring a write spatial lock. So as long as we hold a write spatial lock, all blocks in the area
		// are guaranteed to have safe pointers. We only can't keep these pointers after releasing the spatial lock.
		std::unordered_map<Vector3i, std::shared_ptr<Block>> blocks;
		Mutex mutex;
		VoxelSpatialLock spatial_lock;

		~Map();
	};

	struct Pass {
		// How many blocks to load using the previous pass around the generated area, so that neighbors can be accessed
		// in case the pass has effects across blocks.
		// Constraints: the first pass cannot have dependencies; other passes must have dependencies.
		int dependency_extents = 0;
		// If true, the pass must run on the whole column of blocks specified in vertical range instead of just one
		// block.
		bool column = false;
		// Height region within which the pass must run
		int column_min_y;
		int column_max_y;
	};

	VoxelGeneratorMultipass();
	~VoxelGeneratorMultipass();

	Result generate_block(VoxelQueryData &input) override;

	struct PassInput {
		Span<std::shared_ptr<Block>> grid;
		Vector3i grid_size;
		// Position of the grid's lower corner.
		Vector3i grid_origin;
		// Position of the main block or column in world coordinates.
		Vector3i main_block_position;
		int pass_index;
	};

	// Executes a pass on a grid of blocks.
	// The grid contains a central block, or column of blocks, where the main processing must happen.
	// It is surrounded by neighbors, which are accessible in case main processing affects them partially (structures
	// intersecting, light spreading out...). "Main processing" will be invoked only once per block.
	// Each block in the grid is guaranteed to have been processed at least by preceding passes.
	// However, neighbor blocks can also have been processed by the current pass already ("main" or not), which is a
	// side-effect of being able to modify neighbors. This is where you need to be careful with seeded generation: if
	// two neighbor blocks both generate things that overlap each other, one will necessarily generate before the other.
	// That order is unpredictable because of multithreading and player movement. So it's up to you to ensure each
	// order produces the same outcome. If you don't, it might not be a big issue, but generating the same world twice
	// will therefore have some differences.
	virtual void generate_pass(PassInput input);

	const Pass &get_pass(int index) const {
		ZN_ASSERT(index >= 0 && index < int(_passes.size()));
		return _passes[index];
	}

	int get_pass_count() const {
		return _passes.size();
	}

	// Internal

	std::shared_ptr<VoxelGeneratorMultipass::Map> get_map() const {
		return _map;
	}

	void process_viewer_diff(Box3i p_requested_box, Box3i p_prev_requested_box);

private:
	std::vector<Pass> _passes;
	std::shared_ptr<Map> _map;
};

} // namespace voxel
} // namespace zylann

#endif // VOXEL_GENERATOR_MULTIPASS_H
