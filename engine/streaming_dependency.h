#ifndef VOXEL_STREAMING_DEPENDENCY_H
#define VOXEL_STREAMING_DEPENDENCY_H

#include "../generators/voxel_generator.h"
#include "../streams/voxel_stream.h"

namespace zylann::voxel {

// Shared dependency needed by some asynchronous tasks.
// It may be passed with a shared_ptr.
// Pointers inside should not change. If they do, a new instance will be made and old ones will be marked invalid,
// rather than risking a bad pointer read or having to use (many) mutexes.
struct StreamingDependency {
	Ref<VoxelStream> stream;
	Ref<VoxelGenerator> generator;
	bool valid = true;

	static void reset(
			std::shared_ptr<StreamingDependency> &ref, Ref<VoxelStream> stream, Ref<VoxelGenerator> generator) {
		if (ref != nullptr) {
			ref->valid = false;
		}
		ref = make_shared_instance<StreamingDependency>();
		ref->stream = stream;
		ref->generator = generator;
		ref->valid = true;
	}
};

} // namespace zylann::voxel

#endif // VOXEL_STREAMING_DEPENDENCY_H
