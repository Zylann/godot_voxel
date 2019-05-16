#include "voxel_provider_thread.h"
#include "../providers/voxel_provider.h"
#include "../util/utility.h"
#include "voxel_map.h"

#include <core/os/os.h>
#include <core/os/semaphore.h>
#include <core/os/thread.h>

VoxelProviderThread::VoxelProviderThread(Ref<VoxelProvider> provider, int block_size_pow2) {

	CRASH_COND(provider.is_null());
	CRASH_COND(block_size_pow2 <= 0);

	_voxel_provider = provider;
	_block_size_pow2 = block_size_pow2;
	_input_mutex = Mutex::create();
	_output_mutex = Mutex::create();
	_semaphore = Semaphore::create();
	_thread_exit = false;
	_thread = Thread::create(_thread_func, this);
}

VoxelProviderThread::~VoxelProviderThread() {

	_thread_exit = true;
	_semaphore->post();
	Thread::wait_to_finish(_thread);

	memdelete(_thread);
	memdelete(_semaphore);
	memdelete(_input_mutex);
	memdelete(_output_mutex);
}

void VoxelProviderThread::push(const InputData &input) {

	bool should_run = false;

	{
		MutexLock lock(_input_mutex);

		// TODO If the same request is sent twice, keep only the latest one

		append_array(_shared_input.blocks_to_emerge, input.blocks_to_emerge);
		append_array(_shared_input.blocks_to_immerge, input.blocks_to_immerge);
		_shared_input.priority_block_position = input.priority_block_position;

		if (input.use_exclusive_region) {
			_shared_input.use_exclusive_region = true;
			_shared_input.exclusive_region_extent = input.exclusive_region_extent;
		}

		should_run = !_shared_input.is_empty();
	}

	// Notify the thread it should run
	if (should_run) {
		_semaphore->post();
	}
}

void VoxelProviderThread::pop(OutputData &out_data) {

	MutexLock lock(_output_mutex);

	out_data.emerged_blocks.append_array(_shared_output);
	out_data.stats = _shared_stats;
	_shared_output.clear();
}

void VoxelProviderThread::_thread_func(void *p_self) {
	VoxelProviderThread *self = reinterpret_cast<VoxelProviderThread *>(p_self);
	self->thread_func();
}

void VoxelProviderThread::thread_func() {

	while (!_thread_exit) {

		uint32_t sync_interval = 500.0; // milliseconds
		uint32_t sync_time = OS::get_singleton()->get_ticks_msec() + sync_interval;

		int emerge_index = 0;
		Stats stats;

		thread_sync(emerge_index, stats, stats.sort_time);

		while (!_input.is_empty() && !_thread_exit) {
			//print_line(String("Thread runs: {0}").format(varray(_input.blocks_to_emerge.size())));

			// TODO Block saving
			_input.blocks_to_immerge.clear();

			if (!_input.blocks_to_emerge.empty()) {

				EmergeInput block = _input.blocks_to_emerge[emerge_index];
				++emerge_index;

				if (emerge_index >= _input.blocks_to_emerge.size()) {
					_input.blocks_to_emerge.clear();
				}

				int bs = 1 << _block_size_pow2;
				Ref<VoxelBuffer> buffer = Ref<VoxelBuffer>(memnew(VoxelBuffer));
				buffer->create(bs, bs, bs);

				// Query voxel provider
				Vector3i block_origin_in_voxels = block.block_position * (bs << block.lod);
				uint64_t time_before = OS::get_singleton()->get_ticks_usec();
				_voxel_provider->emerge_block(buffer, block_origin_in_voxels, block.lod);
				uint64_t time_taken = OS::get_singleton()->get_ticks_usec() - time_before;

				// Do some stats
				if (stats.first) {
					stats.first = false;
					stats.min_time = time_taken;
					stats.max_time = time_taken;
				} else {
					if (time_taken < stats.min_time) {
						stats.min_time = time_taken;
					}
					if (time_taken > stats.max_time) {
						stats.max_time = time_taken;
					}
				}

				EmergeOutput eo;
				eo.origin_in_voxels = block_origin_in_voxels;
				eo.block_position = block.block_position;
				eo.voxels = buffer;
				eo.lod = block.lod;
				_output.push_back(eo);
			}

			uint32_t time = OS::get_singleton()->get_ticks_msec();
			if (time >= sync_time || _input.is_empty()) {

				uint64_t sort_time;
				thread_sync(emerge_index, stats, sort_time);

				sync_time = OS::get_singleton()->get_ticks_msec() + sync_interval;
				emerge_index = 0;
				stats = Stats();
				stats.sort_time = sort_time;
			}
		}

		if (_thread_exit) {
			break;
		}

		// Wait for future wake-up
		_semaphore->wait();
	}

	print_line("Thread exits");
}

// Sorts distance to viewer
// The closest block will be the first one in the array
struct BlockPositionComparator {
	// In LOD0 block coordinates
	Vector3i center;
	inline bool operator()(const VoxelProviderThread::EmergeInput &a, const VoxelProviderThread::EmergeInput &b) const {
		if (a.lod == b.lod) {
			int da = (a.block_position * (1 << a.lod)).distance_sq(center);
			int db = (b.block_position * (1 << b.lod)).distance_sq(center);
			return da < db;
		} else {
			// Load highest lods first because they are needed for the octree to subdivide
			return a.lod > b.lod;
		}
	}
};

void VoxelProviderThread::thread_sync(int emerge_index, Stats stats, uint64_t &out_sort_time) {

	if (!_input.blocks_to_emerge.empty()) {
		// Cleanup emerge vector

		if (emerge_index >= _input.blocks_to_emerge.size()) {
			_input.blocks_to_emerge.clear();

		} else if (emerge_index > 0) {

			// Shift up remaining items since we use a Vector
			shift_up(_input.blocks_to_emerge, emerge_index);
		}
	}

	{
		// Get input
		MutexLock lock(_input_mutex);

		append_array(_input.blocks_to_emerge, _shared_input.blocks_to_emerge);
		append_array(_input.blocks_to_immerge, _shared_input.blocks_to_immerge);
		_input.priority_block_position = _shared_input.priority_block_position;

		if (_shared_input.use_exclusive_region) {
			_input.use_exclusive_region = true;
			_input.exclusive_region_extent = _shared_input.exclusive_region_extent;
		}

		_shared_input.blocks_to_emerge.clear();
		_shared_input.blocks_to_immerge.clear();
		_shared_input.use_exclusive_region = false;
	}

	stats.remaining_blocks = _input.blocks_to_emerge.size();

	//	print_line(String("VoxelProviderThread: posting {0} blocks, {1} remaining ; cost [{2}..{3}] usec")
	//			   .format(varray(_output.size(), _input.blocks_to_emerge.size(), stats.min_time, stats.max_time)));

	{
		// Post output
		MutexLock lock(_output_mutex);
		_shared_output.append_array(_output);
		_shared_stats = stats;
		_output.clear();
	}

	// Cancel blocks outside exclusive region
	//int dropped_count = 0;
	if (_input.use_exclusive_region) {
		for (int i = 0; i < _input.blocks_to_emerge.size(); ++i) {
			const EmergeInput &ei = _input.blocks_to_emerge[i];

			Rect3i box = Rect3i::from_center_extents(_input.priority_block_position >> ei.lod, Vector3i(_input.exclusive_region_extent));

			if (!box.contains(ei.block_position)) {
				_input.blocks_to_emerge[i] = _input.blocks_to_emerge.back();
				_input.blocks_to_emerge.pop_back();
				//++dropped_count;
			}
		}
	}

	//	if (dropped_count > 0) {
	//		print_line(String("Dropped {0} blocks to load from thread").format(varray(dropped_count)));
	//	}

	uint64_t time_before = OS::get_singleton()->get_ticks_usec();

	if (!_input.blocks_to_emerge.empty()) {
		// Re-sort priority

		SortArray<EmergeInput, BlockPositionComparator> sorter;
		sorter.compare.center = _input.priority_block_position;
		sorter.sort(_input.blocks_to_emerge.data(), _input.blocks_to_emerge.size());
	}

	out_sort_time = OS::get_singleton()->get_ticks_usec() - time_before;
}

Dictionary VoxelProviderThread::to_dictionary(const Stats &stats) {
	Dictionary d;
	d["min_time"] = stats.min_time;
	d["max_time"] = stats.max_time;
	d["remaining_blocks"] = stats.remaining_blocks;
	d["sort_time"] = stats.sort_time;
	return d;
}
