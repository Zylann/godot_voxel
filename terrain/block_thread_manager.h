#ifndef VOXEL_BLOCK_THREAD_MANAGER_H
#define VOXEL_BLOCK_THREAD_MANAGER_H

#include "../math/rect3i.h"
#include "../math/vector3i.h"
#include "../util/utility.h"
#include <core/os/os.h>
#include <core/os/semaphore.h>
#include <vector>

// Base structure for an asynchronous block processing manager using threads.
// It is the same for block loading and rendering, hence made a generic one.
// - Push requests and pop requests in batch
// - One or more threads can be used
// - Minimizes sync points
// - Orders blocks to process the closest ones first
// - Merges duplicate requests
// - Cancels requests that become out of range
// - Takes some stats
template <typename InputBlockData_T, typename OutputBlockData_T, typename Processor_T>
class VoxelBlockThreadManager {
public:
	static const int MAX_LOD = 32; // Like VoxelLodTerrain
	static const int MAX_JOBS = 8; // Arbitrary, should be enough

	// Specialization must be copyable
	struct InputBlock {
		InputBlockData_T data;
		Vector3i position; // In LOD0 block coordinates
		unsigned int lod = 0;
	};

	// Specialization must be copyable
	struct OutputBlock {
		OutputBlockData_T data;
		Vector3i position; // In LOD0 block coordinates
		unsigned int lod = 0;
	};

	struct Input {
		std::vector<InputBlock> blocks;
		Vector3i priority_position; // In LOD0 block coordinates
		int exclusive_region_extent = 0; // Region beyond which the processor is allowed to discard requests
		bool use_exclusive_region = false;

		bool is_empty() const {
			return blocks.empty();
		}
	};

	struct Stats {
		bool first = true;
		uint64_t min_time = 0;
		uint64_t max_time = 0;
		uint64_t sorting_time = 0;
		uint32_t remaining_blocks[MAX_JOBS];
		uint32_t thread_count = 0;
	};

	struct Output {
		Vector<OutputBlock> blocks;
		Stats stats;
	};

	// TODO Make job count dynamic, don't start threads in constructor

	// Creates and starts jobs.
	// Processors are given as array because you could decide to either re-use the same one,
	// or have clones depending on them being stateless or not.
	VoxelBlockThreadManager(unsigned int job_count, unsigned int sync_interval_ms, Processor_T *processors) {

		CRASH_COND(job_count < 1);
		CRASH_COND(job_count >= MAX_JOBS);
		_job_count = job_count;

		for (unsigned int i = 0; i < MAX_JOBS; ++i) {
			_jobs[i].job_index = i;
		}

		for (unsigned int i = 0; i < _job_count; ++i) {

			JobData &job = _jobs[i];
			CRASH_COND(job.thread != nullptr);

			job.input_mutex = Mutex::create();
			job.output_mutex = Mutex::create();
			job.semaphore = Semaphore::create();
			job.thread = Thread::create(_thread_func, &job);
			job.needs_sort = true;
			job.sync_interval_ms = sync_interval_ms;
			job.processor = processors[i];
		}
	}

	~VoxelBlockThreadManager() {

		for (unsigned int i = 0; i < _job_count; ++i) {
			JobData &job = _jobs[i];
			job.thread_exit = true;
			job.semaphore->post();
		}

		for (unsigned int i = 0; i < _job_count; ++i) {

			JobData &job = _jobs[i];
			CRASH_COND(job.thread == nullptr);

			Thread::wait_to_finish(job.thread);

			memdelete(job.thread);
			memdelete(job.semaphore);
			memdelete(job.input_mutex);
			memdelete(job.output_mutex);
		}
	}

	void push(const Input &input) {

		CRASH_COND(_job_count < 1);

		int replaced_blocks = 0;
		int highest_pending_count = 0;
		int lowest_pending_count = 0;

		// Lock all inputs and gather their pending work counts
		for (int job_index = 0; job_index < _job_count; ++job_index) {

			JobData &job = _jobs[job_index];

			job.input_mutex->lock();

			highest_pending_count = MAX(highest_pending_count, job.shared_input.blocks.size());
			lowest_pending_count = MIN(lowest_pending_count, job.shared_input.blocks.size());
		}

		int i = 0;

		// We don't use a "weakest team gets it" dispatch for speed,
		// So use median count to prioritize only jobs under that median and not just the highest.
		int median_pending_count = lowest_pending_count + (highest_pending_count - lowest_pending_count) / 2;

		// Dispatch to jobs with least pending requests
		for (int job_index = 0; job_index < _job_count && i < input.blocks.size(); ++job_index) {

			JobData &job = _jobs[job_index];
			int pending_count = job.shared_input.blocks.size();

			int count = MIN(median_pending_count - pending_count, input.blocks.size());

			if (count > 0) {
				if (i + count > input.blocks.size()) {
					count = input.blocks.size() - i;
				}
				replaced_blocks += push_block_requests(job, input.blocks, i, count);
				i += count;
			}
		}

		// Dispatch equal count of remaining requests.
		// Remainder is dispatched too until consumed through the first jobs.
		// Then unlock each job.
		int base_count = (input.blocks.size() - i) / _job_count;
		int remainder = (input.blocks.size() - i) % _job_count;
		for (int job_index = 0; job_index < _job_count && i < input.blocks.size(); ++job_index) {

			JobData &job = _jobs[job_index];

			int count = base_count;
			if (remainder > 0) {
				++count;
				--remainder;
			}

			if (i + count > input.blocks.size()) {
				replaced_blocks += push_block_requests(job, input.blocks, i, input.blocks.size() - i);
			} else {
				replaced_blocks += push_block_requests(job, input.blocks, i, count);
				i += count;
			}

			if (job.shared_input.priority_position != input.priority_position || input.blocks.size() > 0) {
				job.needs_sort = true;
			}

			job.shared_input.priority_position = input.priority_position;

			if (input.use_exclusive_region) {
				job.shared_input.use_exclusive_region = true;
				job.shared_input.exclusive_region_extent = input.exclusive_region_extent;
			}
		}

		// Unlock inputs and resume
		for (int job_index = 0; job_index < _job_count; ++job_index) {

			JobData &job = _jobs[job_index];

			bool should_run = !job.shared_input.is_empty();

			job.input_mutex->unlock();

			if (should_run) {
				job.semaphore->post();
			}
		}

		if (replaced_blocks > 0) {
			print_line(String("VoxelBlockProcessor: {1} blocks already in queue were replaced").format(varray(replaced_blocks)));
		}
	}

	void pop(Output &output) {

		output.stats = Stats();
		output.stats.thread_count = _job_count;

		// Harvest results from all jobs
		for (unsigned int i = 0; i < _job_count; ++i) {

			JobData &job = _jobs[i];
			{
				MutexLock lock(job.output_mutex);

				output.blocks.append_array(job.shared_output.blocks);
				merge_stats(output.stats, job.shared_output.stats, i);
				job.shared_output.blocks.clear();
			}
		}
	}

	static Dictionary to_dictionary(const Stats &stats) {
		Dictionary d;
		d["min_time"] = stats.min_time;
		d["max_time"] = stats.max_time;
		d["sorting_time"] = stats.sorting_time;
		Array remaining_blocks;
		remaining_blocks.resize(stats.thread_count);
		for (int i = 0; i < stats.thread_count; ++i) {
			remaining_blocks[i] = stats.remaining_blocks[i];
		}
		d["remaining_blocks_per_thread"] = remaining_blocks;
		return d;
	}

private:
	struct JobData {

		// Data accessed from other threads, so they need mutexes
		Input shared_input;
		Output shared_output;
		Mutex *input_mutex = nullptr;
		Mutex *output_mutex = nullptr;
		// Indexes which blocks are present in shared_input,
		// so if we push a duplicate request with the same coordinates, we can discard it without a linear search
		HashMap<Vector3i, int, Vector3iHasher> block_indexes[MAX_LOD];
		bool needs_sort = false;
		// Only read by the thread
		bool thread_exit = false;

		Input input;
		Output output;
		Semaphore *semaphore = nullptr;
		Thread *thread = nullptr;
		uint32_t sync_interval_ms = 100;
		uint32_t job_index = -1;

		Processor_T processor;
	};

	static void merge_stats(Stats &a, const Stats &b, int job_index) {
		a.max_time = MAX(a.max_time, b.max_time);
		a.min_time = MIN(a.min_time, b.min_time);
		a.remaining_blocks[job_index] = b.remaining_blocks[job_index];
		a.sorting_time += b.sorting_time;
	}

	int push_block_requests(JobData &job, const std::vector<InputBlock> &input_blocks, int begin, int count) {
		// The job's input must have been locked first

		int replaced_blocks = 0;
		int end = begin + count;
		CRASH_COND(end > input_blocks.size());

		for (int i = begin; i < end; ++i) {
			const InputBlock &block = input_blocks[i];

			CRASH_COND(block.lod >= MAX_LOD)
			int *index = job.block_indexes[block.lod].getptr(block.position);

			// TODO When using more than one thread, duplicate rejection is less effective... is it relevant to keep it at all?
			if (index) {
				// The block is already in the update queue, replace it
				++replaced_blocks;
				job.shared_input.blocks[*index] = block;

			} else {
				// Append new block request
				int j = job.shared_input.blocks.size();
				job.shared_input.blocks.push_back(block);
				job.block_indexes[block.lod][block.position] = j;
			}
		}

		return replaced_blocks;
	}

	static void _thread_func(void *p_data) {
		JobData *data = reinterpret_cast<JobData *>(p_data);
		CRASH_COND(data == nullptr);
		thread_func(*data);
	}

	static void thread_func(JobData &data) {

		while (!data.thread_exit) {

			uint32_t sync_time = OS::get_singleton()->get_ticks_msec() + data.sync_interval_ms;

			int queue_index = 0;
			Stats stats;

			thread_sync(data, queue_index, stats, stats.sorting_time);

			while (!data.input.blocks.empty() && !data.thread_exit) {

				if (!data.input.blocks.empty()) {

					InputBlock block = data.input.blocks[queue_index];
					++queue_index;

					if (queue_index >= data.input.blocks.size()) {
						data.input.blocks.clear();
					}

					uint64_t time_before = OS::get_singleton()->get_ticks_usec();

					OutputBlock ob;
					// Implemented in specialization
					data.processor.process_block(block.data, ob.data, block.position, block.lod);
					ob.position = block.position;
					ob.lod = block.lod;

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

					data.output.blocks.push_back(ob);
				}

				uint32_t time = OS::get_singleton()->get_ticks_msec();
				if (time >= sync_time || data.input.blocks.empty()) {

					uint64_t sort_time;
					thread_sync(data, queue_index, stats, sort_time);

					sync_time = OS::get_singleton()->get_ticks_msec() + data.sync_interval_ms;
					queue_index = 0;
					stats = Stats();
					stats.sorting_time = sort_time;
				}
			}

			if (data.thread_exit) {
				break;
			}

			// Wait for future wake-up
			data.semaphore->wait();
		}
	}

	// TODO Precalculate a heuristic on all blocks instead of a comparator, it will be faster
	// Sorts distance to viewer
	// The closest block will be the first one in the array
	struct BlockUpdateComparator {
		Vector3i center; // In LOD0 block coordinates
		inline bool operator()(const InputBlock &a, const InputBlock &b) const {
			// TODO Give a higher priority to blocks in frustrum
			if (a.lod == b.lod) {
				int da = (a.position * (1 << a.lod)).distance_sq(center);
				int db = (b.position * (1 << b.lod)).distance_sq(center);
				return da < db;
			} else {
				// Load highest lods first because they are needed for the octree to subdivide
				return a.lod > b.lod;
			}
		}
	};

	static void thread_sync(JobData &data, int queue_index, Stats stats, uint64_t &out_sort_time) {

		if (!data.input.blocks.empty()) {
			// Cleanup input vector

			if (queue_index >= data.input.blocks.size()) {
				data.input.blocks.clear();

			} else if (queue_index > 0) {

				// Shift up remaining items since we use a Vector
				shift_up(data.input.blocks, queue_index);
			}
		}

		stats.remaining_blocks[data.job_index] = data.input.blocks.size();
		bool needs_sort;

		// Get input
		{
			MutexLock lock(data.input_mutex);

			// Copy requests from shared to internal
			append_array(data.input.blocks, data.shared_input.blocks);

			data.input.priority_position = data.shared_input.priority_position;

			if (data.shared_input.use_exclusive_region) {
				data.input.use_exclusive_region = true;
				data.input.exclusive_region_extent = data.shared_input.exclusive_region_extent;
			}

			data.shared_input.blocks.clear();

			for (unsigned int lod_index = 0; lod_index < MAX_LOD; ++lod_index) {
				data.block_indexes[lod_index].clear();
			}

			needs_sort = data.needs_sort;
			data.needs_sort = false;
		}

		if (!data.output.blocks.empty()) {

			//		print_line(String("VoxelMeshUpdater: posting {0} blocks, {1} remaining ; cost [{2}..{3}] usec")
			//				   .format(varray(_output.blocks.size(), _input.blocks.size(), stats.min_time, stats.max_time)));

			// Copy output to shared
			MutexLock lock(data.output_mutex);
			data.shared_output.blocks.append_array(data.output.blocks);
			data.shared_output.stats = stats;
			data.output.blocks.clear();
		}

		// Cancel blocks outside exclusive region.
		// We do this early because if the player keeps moving forward,
		// we would keep accumulating requests forever, and that means slower sorting and memory waste
		//int dropped_count = 0;
		if (data.input.use_exclusive_region) {
			for (int i = 0; i < data.input.blocks.size(); ++i) {
				const InputBlock &ib = data.input.blocks[i];

				Rect3i box = Rect3i::from_center_extents(data.input.priority_position >> ib.lod, Vector3i(data.input.exclusive_region_extent));

				if (!box.contains(ib.position)) {

					Vector3i shifted_block_pos = data.input.blocks.back().position;

					data.input.blocks[i] = data.input.blocks.back();
					data.input.blocks.pop_back();

					data.block_indexes[ib.lod].erase(ib.position);
					data.block_indexes[ib.lod][shifted_block_pos] = i;

					//++dropped_count;
				}
			}
		}

		// TODO Make it part of stats?
		//	if (dropped_count > 0) {
		//		print_line(String("Dropped {0} blocks to mesh from thread").format(varray(dropped_count)));
		//	}

		uint64_t time_before = OS::get_singleton()->get_ticks_usec();

		if (!data.input.blocks.empty() && needs_sort) {
			// Re-sort priority
			SortArray<InputBlock, BlockUpdateComparator> sorter;
			sorter.compare.center = data.input.priority_position;
			sorter.sort(data.input.blocks.data(), data.input.blocks.size());
		}

		out_sort_time = OS::get_singleton()->get_ticks_usec() - time_before;
	}

	JobData _jobs[MAX_JOBS];
	unsigned int _job_count = 0;
};

#endif // VOXEL_BLOCK_THREAD_MANAGER_H
