#include "voxel_mesh_updater.h"
#include "../util/utility.h"
#include "voxel_lod_terrain.h"
#include <core/os/os.h>

VoxelMeshUpdater::VoxelMeshUpdater(Ref<VoxelLibrary> library, MeshingParams params) {

	if (library.is_valid()) {
		_blocky_mesher.instance();
		_blocky_mesher->set_library(library);
		_blocky_mesher->set_occlusion_enabled(params.baked_ao);
		_blocky_mesher->set_occlusion_darkness(params.baked_ao_darkness);
	}

	if (params.smooth_surface) {
		_dmc_mesher.instance();
		_dmc_mesher->set_geometric_error(0.05);
		_dmc_mesher->set_octree_mode(VoxelMesherDMC::OCTREE_NONE);
		_dmc_mesher->set_seam_mode(VoxelMesherDMC::SEAM_MARCHING_SQUARE_SKIRTS);
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

			const InputBlock &block = input.blocks[i];

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

			CRASH_COND(block.lod >= MAX_LOD)
			int *index = _block_indexes[block.lod].getptr(block.position);

			if (index) {
				// The block is already in the update queue, replace it
				++replaced_blocks;
				_shared_input.blocks.write[*index] = block;

			} else {

				int j = _shared_input.blocks.size();
				_shared_input.blocks.push_back(block);
				_block_indexes[block.lod][block.position] = j;
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

int VoxelMeshUpdater::get_required_padding() const {

	int padding = 0;

	if (_blocky_mesher.is_valid()) {
		padding = max(padding, _blocky_mesher->get_minimum_padding());
	}

	if (_dmc_mesher.is_valid()) {
		padding = max(padding, _dmc_mesher->get_minimum_padding());
	}

	return padding;
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

static void scale_mesh_data(VoxelMesher::Output &data, float factor) {

	for (int i = 0; i < data.surfaces.size(); ++i) {
		Array &surface = data.surfaces.write[i]; // There is COW here too but should not happen, hopefully

		if (surface.empty()) {
			continue;
		}

		PoolVector3Array positions = surface[Mesh::ARRAY_VERTEX]; // Array of Variants here, implicit cast going on

		// Now dear COW, let's make sure there is only ONE ref to that PoolVector,
		// so you won't trash performance with pointless allocations
		surface[Mesh::ARRAY_VERTEX] = Variant();

		PoolVector3Array::Write w = positions.write();
		int len = positions.size();
		for (int j = 0; j < len; ++j) {
			w[j] = w[j] * factor;
		}

		// Thank you
		surface[Mesh::ARRAY_VERTEX] = positions;
	}
}

void VoxelMeshUpdater::process_block(const InputBlock &block, OutputBlock &output) {

	CRASH_COND(block.voxels.is_null());

	int padding = get_required_padding();

	if (_blocky_mesher.is_valid()) {
		_blocky_mesher->build(output.blocky_surfaces, **block.voxels, padding);
	}

	if (_dmc_mesher.is_valid()) {
		_dmc_mesher->build(output.smooth_surfaces, **block.voxels, padding);
	}

	output.position = block.position;
	output.lod = block.lod;

	if (block.lod > 0) {
		float factor = 1 << block.lod;
		scale_mesh_data(output.blocky_surfaces, factor);
		scale_mesh_data(output.smooth_surfaces, factor);
	}
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

		for (unsigned int lod_index = 0; lod_index < MAX_LOD; ++lod_index) {
			_block_indexes[lod_index].clear();
		}

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

Dictionary VoxelMeshUpdater::to_dictionary(const Stats &stats) {
	Dictionary d;
	d["min_time"] = stats.min_time;
	d["max_time"] = stats.max_time;
	d["remaining_blocks"] = stats.remaining_blocks;
	return d;
}
