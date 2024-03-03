#ifndef VOXEL_BLOCK_TASK_SEQUENCER_H
#define VOXEL_BLOCK_TASK_SEQUENCER_H

#include <queue>
#include <unordered_map>

#include "../constants/voxel_constants.h"
#include "../util/containers/fixed_array.h"
#include "../util/math/vector3i.h"
#include "../util/thread/mutex.h"

namespace zylann {

class IThreadedTask;

namespace voxel {

// Utility to order execution of threaded tasks per chunk. This is intented to be used with saving and loading I/O
// tasks which need this consistency.
class BlockTaskSequencer {
public:
	~BlockTaskSequencer();

	// After creating a task, call this before scheduling it.
	// If this returns `true`, the sequencer will take ownership of the task and it must not be scheduled yet.
	bool enqueue(IThreadedTask *task, Vector3i position, unsigned int lod_index);

	// Call this at the end of every task for which `enqueue` has been called, and schedule or execute the
	// returned task if it isn't null.
	IThreadedTask *dequeue(Vector3i position, unsigned int lod_index);

private:
	struct Block {
		std::queue<IThreadedTask *> queue;
	};

	struct Lod {
		std::unordered_map<Vector3i, Block> blocks;
		Mutex mutex;
	};

	FixedArray<Lod, constants::MAX_LOD> _lods;
};

} // namespace voxel
} // namespace zylann

#endif // VOXEL_BLOCK_TASK_SEQUENCER_H
