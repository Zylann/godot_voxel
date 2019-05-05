#ifndef VOXEL_PROVIDER_THREAD_H
#define VOXEL_PROVIDER_THREAD_H

#include "../math/vector3i.h"
#include <core/resource.h>

class VoxelProvider;
class VoxelBuffer;
class Thread;
class Semaphore;

class VoxelProviderThread {
public:
	struct ImmergeInput {
		Vector3i origin;
		Ref<VoxelBuffer> voxels;
		int lod = 0;
	};

	struct EmergeInput {
		Vector3i block_position;
		int lod = 0;
	};

	struct InputData {
		Vector<ImmergeInput> blocks_to_immerge;
		Vector<EmergeInput> blocks_to_emerge;
		Vector3i priority_block_position;

		inline bool is_empty() {
			return blocks_to_emerge.empty() && blocks_to_immerge.empty();
		}
	};

	struct EmergeOutput {
		Ref<VoxelBuffer> voxels;
		Vector3i origin_in_voxels; // TODO Remove this, redundant now
		Vector3i block_position;
		int lod = 0;
	};

	struct Stats {
		bool first = true;
		uint64_t min_time = 0;
		uint64_t max_time = 0;
		int remaining_blocks = 0;
	};

	struct OutputData {
		Vector<EmergeOutput> emerged_blocks;
		Stats stats;
	};

	VoxelProviderThread(Ref<VoxelProvider> provider, int block_size_pow2);
	~VoxelProviderThread();

	void push(const InputData &input);
	void pop(OutputData &out_data);

	static Dictionary to_dictionary(const Stats &stats);

private:
	static void _thread_func(void *p_self);

	void thread_func();
	void thread_sync(int emerge_index, Stats stats);

private:
	InputData _shared_input;
	Mutex *_input_mutex;

	Vector<EmergeOutput> _shared_output;
	Stats _shared_stats;
	Mutex *_output_mutex;

	Semaphore *_semaphore;
	bool _thread_exit;
	Thread *_thread;
	InputData _input;
	Vector<EmergeOutput> _output;
	int _block_size_pow2;

	Ref<VoxelProvider> _voxel_provider;
};

#endif // VOXEL_PROVIDER_THREAD_H
