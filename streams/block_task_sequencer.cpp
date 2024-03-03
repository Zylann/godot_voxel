#include "block_task_sequencer.h"
#include "../util/memory.h"
#include "../util/profiling.h"
#include "../util/tasks/threaded_task.h"

namespace zylann::voxel {

BlockTaskSequencer::~BlockTaskSequencer() {
	// It is assumed that no other thread is still holding on the object.
	// If tasks remain at this point, it means nothing needs them to run. Because when tasks are stored, it means
	// at least one is already scheduled, referencing the sequencer (shared ownership), therefore not going to get
	// destroyed.
	for (Lod &lod : _lods) {
		for (auto it = lod.blocks.begin(); it != lod.blocks.end(); ++it) {
			Block &block = it->second;
			while (!block.queue.empty()) {
				IThreadedTask *task = block.queue.front();
				block.queue.pop();
				ZN_ASSERT(task != nullptr);
				ZN_DELETE(task);
			}
		}
	}
}

bool BlockTaskSequencer::enqueue(IThreadedTask *task, Vector3i position, unsigned int lod_index) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN_V(task != nullptr, false);
	Lod &lod = _lods[lod_index];
	MutexLock mlock(lod.mutex);
	auto it = lod.blocks.find(position);
	if (it == lod.blocks.end()) {
		lod.blocks.insert({ position, Block() });
		return false;
	} else {
		Block &block = it->second;
		block.queue.push(task);
		return true;
	}
}

IThreadedTask *BlockTaskSequencer::dequeue(Vector3i position, unsigned int lod_index) {
	ZN_PROFILE_SCOPE();
	Lod &lod = _lods[lod_index];
	MutexLock mlock(lod.mutex);
	auto it = lod.blocks.find(position);
	ZN_ASSERT_RETURN_V_MSG(it != lod.blocks.end(), nullptr, "Bug! Dequeueing too much, or never enqueued?");
	Block &block = it->second;
	if (block.queue.empty()) {
		// We are done with this chunk
		lod.blocks.erase(position);
		return nullptr;
	} else {
		// There is a pending task to execute next
		IThreadedTask *task = block.queue.front();
		block.queue.pop();
		return task;
	}
}

} // namespace zylann::voxel
