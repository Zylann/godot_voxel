#ifndef VOXEL_GENERATOR_MULTIPASS_H
#define VOXEL_GENERATOR_MULTIPASS_H

#include "../../storage/voxel_buffer_internal.h"
#include "../../storage/voxel_spatial_lock.h"
#include "../voxel_generator.h"

namespace zylann::voxel {

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
	static const unsigned int MAX_PASSES = 4;
	// static const int MAX_PASS_EXTENT = 4;

	struct Block {
		VoxelBufferInternal voxels;
		// Index of the last pass that was executed directly on this chunk.
		// -1 means the chunk just got created and no pass has run on it yet.
		// This does not mean the pass has been completed though: a pass is completed when the chunk and all neighbors
		// that can access it within that pass have been processed. So for example, if a pass has an access distance of
		// 1 neighbor chunks, then a if we run that pass on 3x3x3 chunks, only the middle chunk will be complete.
		int16_t pass_index = -1;

		// Each bit corresponds to a pass, and tells if a task is pending to process this chunk.
		// uint8_t pending_partial_generation_tasks = 0;
		// uint8_t pending_full_generation_tasks = 0;
		// Something more extreme we could do: have a list of tasks waiting for the block to be processed by whatever
		// task is pending? Uses more space though.
		// FixedArray<std::vector<WaitingTask>, MAX_PASSES> tasks_waiting_for_partial;
		// FixedArray<std::vector<WaitingTask>, MAX_PASSES> tasks_waiting_for_full;

		// Caches how many chunks in the neighborhood have been processed while having access to this one (included).
		// This is an optimization allowing to reduce chunk map queries. This is an array because it is possible
		// for a pass to start writing into a block that completed its previous pass but hasn't started the next pass.
		FixedArray<uint16_t, MAX_PASSES> pass_iterations;

		// Set to true if a task is pending to process this block. This is to prevent from spawning too many tasks
		// ending up requesting the same block, due to neighbor dependencies.
		// bool pending_task = false;

		Block() {
			fill(pass_iterations, uint16_t(0));
		}
	};

	struct Map {
		std::unordered_map<Vector3i, std::shared_ptr<Block>> blocks;
		Mutex mutex;
		VoxelSpatialLock spatial_lock;
	};

	struct Pass {
		// How many blocks to load around the generated area, so that neighbors can be accessed in case the pass has
		// effects across blocks.
		BoxBounds3i dependency_extents;
		// If true, the pass must run on the whole column of blocks specified in vertical range instead of just one
		// block.
		bool column = false;
		// Height region within which the pass must run
		int column_min_y;
		int column_max_y;
	};

	VoxelGeneratorMultipass();

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

private:
	std::vector<Pass> _passes;
	// TODO We really have to know about the main map because we must not generate blocks that have already been
	// generated. Using the same map makes the most sense, unfortunately it would cause lock contention on the
	// hashmap... and it will anyways if we give access to the data.
	std::shared_ptr<Map> _map;
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATOR_MULTIPASS_H
