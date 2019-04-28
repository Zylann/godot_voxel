#include "voxel_mesh_updater.h"
#include "utility.h"
#include <core/os/os.h>

VoxelMeshUpdater::VoxelMeshUpdater(Ref<VoxelLibrary> library, MeshingParams params) {

	CRASH_COND(library.is_null());
	//CRASH_COND(params.materials.size() == 0);

	_blocky_mesher.instance();
	_blocky_mesher->set_library(library);
	_blocky_mesher->set_occlusion_enabled(params.baked_ao);
	_blocky_mesher->set_occlusion_darkness(params.baked_ao_darkness);

	if (params.smooth_surface) {
		_dmc_mesher.instance();
		_dmc_mesher->set_geometric_error(0.05);
		_dmc_mesher->set_octree_mode(VoxelMesherDMC::OCTREE_NONE);
	}

	_input_mutex = Mutex::create();
	_output_mutex = Mutex::create();

	_thread_exit = false;
	_semaphore = Semaphore::create();
	_thread = Thread::create(_thread_func, this);

	_needs_sort = true;
}

VoxelMeshUpdater::~VoxelMeshUpdater() {

	_thread_exit = true;
	_semaphore->post();
	Thread::wait_to_finish(_thread);
	memdelete(_thread);
	memdelete(_semaphore);
	memdelete(_input_mutex);
	memdelete(_output_mutex);
}

void VoxelMeshUpdater::push(const Input &input) {

	bool should_run = false;
	int replaced_blocks = 0;

	{
		MutexLock lock(_input_mutex);

		for (int i = 0; i < input.blocks.size(); ++i) {

			Vector3i pos = input.blocks[i].position;

			// If a block is exactly on the priority position, update it instantly on the main thread
			// This is to eliminate latency for player's actions, assuming updating a block isn't slower than a frame
			/*if (pos == _shared_input.priority_position) {

				OutputBlock ob;
				process_block(_shared_input.blocks[i], ob);

				{
					MutexLock lock2(_output_mutex);
					_shared_output.blocks.push_back(ob);
				}

				continue;
			}*/

			int *index = _block_indexes.getptr(pos);

			if (index) {
				// The block is already in the update queue, replace it
				++replaced_blocks;
				_shared_input.blocks.write[*index] = input.blocks[i];

			} else {

				int j = _shared_input.blocks.size();
				_shared_input.blocks.push_back(input.blocks[i]);
				_block_indexes[pos] = j;
			}
		}

		if (_shared_input.priority_position != input.priority_position || input.blocks.size() > 0) {
			_needs_sort = true;
		}

		_shared_input.priority_position = input.priority_position;
		should_run = !_shared_input.is_empty();
	}

	if (replaced_blocks > 0) {
		print_line(String("VoxelMeshUpdater: {0} blocks already in queue were replaced").format(varray(replaced_blocks)));
	}

	if (should_run) {
		_semaphore->post();
	}
}

void VoxelMeshUpdater::pop(Output &output) {

	MutexLock lock(_output_mutex);

	output.blocks.append_array(_shared_output.blocks);
	output.stats = _shared_output.stats;
	_shared_output.blocks.clear();
}

void VoxelMeshUpdater::_thread_func(void *p_self) {
	VoxelMeshUpdater *self = reinterpret_cast<VoxelMeshUpdater *>(p_self);
	self->thread_func();
}

void VoxelMeshUpdater::thread_func() {

	while (!_thread_exit) {

		uint32_t sync_interval = 50.0; // milliseconds
		uint32_t sync_time = OS::get_singleton()->get_ticks_msec() + sync_interval;

		int queue_index = 0;
		Stats stats;

		thread_sync(queue_index, stats);

		while (!_input.blocks.empty() && !_thread_exit) {

			if (!_input.blocks.empty()) {

				InputBlock block = _input.blocks[queue_index];
				++queue_index;

				if (queue_index >= _input.blocks.size()) {
					_input.blocks.clear();
				}

				uint64_t time_before = OS::get_singleton()->get_ticks_usec();

				OutputBlock ob;
				process_block(block, ob);

				uint64_t time_taken = OS::get_singleton()->get_ticks_usec() - time_before;

				// Do some stats
				if (stats.first) {
					stats.first = false;
					stats.min_time = time_taken;
					stats.max_time = time_taken;
				} else {
					if (time_taken < stats.min_time)
						stats.min_time = time_taken;
					if (time_taken > stats.max_time)
						stats.max_time = time_taken;
				}

				_output.blocks.push_back(ob);
			}

			uint32_t time = OS::get_singleton()->get_ticks_msec();
			if (time >= sync_time || _input.blocks.empty()) {

				thread_sync(queue_index, stats);

				sync_time = OS::get_singleton()->get_ticks_msec() + sync_interval;
				queue_index = 0;
				stats = Stats();
			}
		}

		if (_thread_exit) {
			break;
		}

		// Wait for future wake-up
		_semaphore->wait();
	}
}

void VoxelMeshUpdater::process_block(const InputBlock &block, OutputBlock &output) {

	CRASH_COND(block.voxels.is_null());

	int padding = 1;
	if (_dmc_mesher.is_valid()) {
		padding = 2;
	}

	// Build cubic parts of the mesh
	output.model_surfaces = _blocky_mesher->build(**block.voxels, Voxel::CHANNEL_TYPE, padding);

	if (_dmc_mesher.is_valid()) {
		// Build smooth parts of the mesh
		output.smooth_surfaces = _dmc_mesher->build(**block.voxels);
	}

	output.position = block.position;
}

// Sorts distance to viewer
// The closest block will be the first one in the array
struct BlockUpdateComparator {
	Vector3i center;
	inline bool operator()(const VoxelMeshUpdater::InputBlock &a, const VoxelMeshUpdater::InputBlock &b) const {
		return a.position.distance_sq(center) < b.position.distance_sq(center);
	}
};

void VoxelMeshUpdater::thread_sync(int queue_index, Stats stats) {

	if (!_input.blocks.empty()) {
		// Cleanup input vector

		if (queue_index >= _input.blocks.size()) {
			_input.blocks.clear();

		} else if (queue_index > 0) {

			// Shift up remaining items since we use a Vector
			shift_up(_input.blocks, queue_index);
		}
	}

	stats.remaining_blocks = _input.blocks.size();
	bool needs_sort;

	{
		// Get input
		MutexLock lock(_input_mutex);

		_input.blocks.append_array(_shared_input.blocks);
		_input.priority_position = _shared_input.priority_position;

		_shared_input.blocks.clear();
		_block_indexes.clear();

		needs_sort = _needs_sort;
		_needs_sort = false;
	}

	if (!_output.blocks.empty()) {

		//		print_line(String("VoxelMeshUpdater: posting {0} blocks, {1} remaining ; cost [{2}..{3}] usec")
		//				   .format(varray(_output.blocks.size(), _input.blocks.size(), stats.min_time, stats.max_time)));

		// Post output
		MutexLock lock(_output_mutex);
		_shared_output.blocks.append_array(_output.blocks);
		_shared_output.stats = stats;
		_output.blocks.clear();
	}

	if (!_input.blocks.empty() && needs_sort) {
		// Re-sort priority

		SortArray<VoxelMeshUpdater::InputBlock, BlockUpdateComparator> sorter;
		sorter.compare.center = _input.priority_position;
		sorter.sort(_input.blocks.ptrw(), _input.blocks.size());
	}
}
