#ifndef VOXEL_SERVER_H
#define VOXEL_SERVER_H

#include "../constants/voxel_constants.h"
#include "../generators/voxel_generator.h"
#include "../meshers/blocky/voxel_mesher_blocky.h"
#include "../streams/voxel_stream.h"
#include "../util/file_locker.h"
#include "../util/struct_db.h"
#include "../util/tasks/progressive_task_runner.h"
#include "../util/tasks/threaded_task_runner.h"
#include "../util/tasks/time_spread_task_runner.h"
#include "meshing_dependency.h"
#include "priority_dependency.h"
#include "streaming_dependency.h"

#include <memory>

namespace zylann {
class AsyncDependencyTracker;
}

namespace zylann::voxel {

// Access point for asynchronous voxel processing APIs.
// Functions must be used from the main thread.
class VoxelServer {
public:
	struct BlockMeshOutput {
		enum Type {
			TYPE_MESHED, // Contains mesh
			TYPE_DROPPED // Indicates the meshing was cancelled
		};

		Type type;
		VoxelMesher::Output surfaces;
		Vector3i position;
		uint8_t lod;
	};

	struct BlockDataOutput {
		enum Type { //
			TYPE_LOADED,
			TYPE_GENERATED,
			TYPE_SAVED
		};

		Type type;
		std::shared_ptr<VoxelBufferInternal> voxels;
		UniquePtr<InstanceBlockData> instances;
		Vector3i position;
		uint8_t lod;
		bool dropped;
		bool max_lod_hint;
		// Blocks with this flag set should not be ignored
		bool initial_load;
	};

	struct BlockMeshInput {
		// Moore area ordered by forward XYZ iteration
		FixedArray<std::shared_ptr<VoxelBufferInternal>, constants::MAX_BLOCK_COUNT_PER_REQUEST> data_blocks;
		unsigned int data_blocks_count = 0;
		Vector3i render_block_position;
		uint8_t lod = 0;
	};

	struct VolumeCallbacks {
		void (*mesh_output_callback)(void *, const BlockMeshOutput &) = nullptr;
		void (*data_output_callback)(void *, BlockDataOutput &) = nullptr;
		void *data = nullptr;

		inline bool check_callbacks() const {
			ERR_FAIL_COND_V(mesh_output_callback == nullptr, false);
			ERR_FAIL_COND_V(data_output_callback == nullptr, false);
			ERR_FAIL_COND_V(data == nullptr, false);
			return true;
		}
	};

	struct Viewer {
		// enum Flags {
		// 	FLAG_DATA = 1,
		// 	FLAG_VISUAL = 2,
		// 	FLAG_COLLISION = 4,
		// 	FLAGS_COUNT = 3
		// };
		Vector3 world_position;
		unsigned int view_distance = 128;
		bool require_collisions = true;
		bool require_visuals = true;
		bool requires_data_block_notifications = false;
		int network_peer_id = -1;
	};

	enum VolumeType { //
		VOLUME_SPARSE_GRID,
		VOLUME_SPARSE_OCTREE
	};

	static VoxelServer &get_singleton();
	static void create_singleton();
	static void destroy_singleton();

	VoxelServer();
	~VoxelServer();

	uint32_t add_volume(VolumeCallbacks callbacks, VolumeType type);
	void set_volume_transform(uint32_t volume_id, Transform3D t);
	void set_volume_render_block_size(uint32_t volume_id, uint32_t block_size);
	void set_volume_data_block_size(uint32_t volume_id, uint32_t block_size);
	void set_volume_stream(uint32_t volume_id, Ref<VoxelStream> stream);
	void set_volume_generator(uint32_t volume_id, Ref<VoxelGenerator> generator);
	void set_volume_mesher(uint32_t volume_id, Ref<VoxelMesher> mesher);
	VolumeCallbacks get_volume_callbacks(uint32_t volume_id) const;
	void set_volume_octree_lod_distance(uint32_t volume_id, float lod_distance);
	void invalidate_volume_mesh_requests(uint32_t volume_id);
	void request_block_mesh(uint32_t volume_id, const BlockMeshInput &input);
	// TODO Add parameter to skip stream loading
	void request_block_load(uint32_t volume_id, Vector3i block_pos, int lod, bool request_instances);
	void request_block_generate(
			uint32_t volume_id, Vector3i block_pos, int lod, std::shared_ptr<AsyncDependencyTracker> tracker);
	void request_all_stream_blocks(uint32_t volume_id);
	void request_voxel_block_save(
			uint32_t volume_id, std::shared_ptr<VoxelBufferInternal> voxels, Vector3i block_pos, int lod);
	void request_instance_block_save(
			uint32_t volume_id, UniquePtr<InstanceBlockData> instances, Vector3i block_pos, int lod);
	void remove_volume(uint32_t volume_id);
	bool is_volume_valid(uint32_t volume_id) const;

	std::shared_ptr<PriorityDependency::ViewersData> get_shared_viewers_data_from_default_world() const {
		return _world.shared_priority_dependency;
	}

	uint32_t add_viewer();
	void remove_viewer(uint32_t viewer_id);
	void set_viewer_position(uint32_t viewer_id, Vector3 position);
	void set_viewer_distance(uint32_t viewer_id, unsigned int distance);
	unsigned int get_viewer_distance(uint32_t viewer_id) const;
	void set_viewer_requires_visuals(uint32_t viewer_id, bool enabled);
	bool is_viewer_requiring_visuals(uint32_t viewer_id) const;
	void set_viewer_requires_collisions(uint32_t viewer_id, bool enabled);
	bool is_viewer_requiring_collisions(uint32_t viewer_id) const;
	void set_viewer_requires_data_block_notifications(uint32_t viewer_id, bool enabled);
	bool is_viewer_requiring_data_block_notifications(uint32_t viewer_id) const;
	void set_viewer_network_peer_id(uint32_t viewer_id, int peer_id);
	int get_viewer_network_peer_id(uint32_t viewer_id) const;
	bool viewer_exists(uint32_t viewer_id) const;

	template <typename F>
	inline void for_each_viewer(F f) const {
		_world.viewers.for_each_with_id(f);
	}

	void push_main_thread_time_spread_task(ITimeSpreadTask *task);
	int get_main_thread_time_budget_usec() const;

	void push_main_thread_progressive_task(IProgressiveTask *task);

	// Thread-safe.
	void push_async_task(IThreadedTask *task);
	// Thread-safe.
	void push_async_tasks(Span<IThreadedTask *> tasks);
	// Thread-safe.
	void push_async_io_task(IThreadedTask *task);
	// Thread-safe.
	void push_async_io_tasks(Span<IThreadedTask *> tasks);

	// Gets by how much voxels must be padded with neighbors in order to be polygonized properly
	// void get_min_max_block_padding(
	// 		bool blocky_enabled, bool smooth_enabled,
	// 		unsigned int &out_min_padding, unsigned int &out_max_padding) const;

	void process();
	void wait_and_clear_all_tasks(bool warn);

	inline FileLocker &get_file_locker() {
		return _file_locker;
	}

	static inline int get_octree_lod_block_region_extent(float lod_distance, float block_size) {
		// This is a bounding radius of blocks around a viewer within which we may load them.
		// `lod_distance` is the distance under which a block should subdivide into a smaller one.
		// Each LOD is fractal so that value is the same for each of them, multiplied by 2^lod.
		return static_cast<int>(Math::ceil(lod_distance / block_size)) * 2 + 2;
	}

	struct Stats {
		struct ThreadPoolStats {
			unsigned int thread_count;
			unsigned int active_threads;
			unsigned int tasks;

			Dictionary to_dict() {
				Dictionary d;
				d["tasks"] = tasks;
				d["active_threads"] = active_threads;
				d["thread_count"] = thread_count;
				return d;
			}
		};

		ThreadPoolStats streaming;
		ThreadPoolStats general;
		int generation_tasks;
		int streaming_tasks;
		int meshing_tasks;
		int main_thread_tasks;

		Dictionary to_dict();
	};

	Stats get_stats() const;

private:
	// Since we are going to send data to tasks running in multiple threads, a few strategies are in place:
	//
	// - Copy the data for each task. This is suitable for simple information that doesn't change after scheduling.
	//
	// - Per-thread instances. This is done if some heap-allocated class instances are not safe
	//   to use in multiple threads, and don't change after being scheduled.
	//
	// - Shared pointers. This is used if the data can change after being scheduled.
	//   This is often done without locking, but only if it's ok to have dirty reads.
	//   If such data sets change structurally (like their size, or other non-dirty-readable fields),
	//   then a new instance is created and old references are left to "die out".

	struct Volume {
		VolumeType type;
		VolumeCallbacks callbacks;
		Transform3D transform;
		Ref<VoxelStream> stream;
		Ref<VoxelGenerator> generator;
		Ref<VoxelMesher> mesher;
		uint32_t render_block_size = 16;
		uint32_t data_block_size = 16;
		float octree_lod_distance = 0;
		std::shared_ptr<StreamingDependency> stream_dependency;
		std::shared_ptr<MeshingDependency> meshing_dependency;
	};

	struct World {
		StructDB<Volume> volumes;
		StructDB<Viewer> viewers;

		// Must be overwritten with a new instance if count changes.
		std::shared_ptr<PriorityDependency::ViewersData> shared_priority_dependency;
	};

	void init_priority_dependency(
			PriorityDependency &dep, Vector3i block_position, uint8_t lod, const Volume &volume, int block_size);

	// TODO multi-world support in the future
	World _world;

	// Pool specialized in file I/O
	ThreadedTaskRunner _streaming_thread_pool;
	// Pool for every other task
	ThreadedTaskRunner _general_thread_pool;
	// For tasks that can only run on the main thread and be spread out over frames
	TimeSpreadTaskRunner _time_spread_task_runner;
	int _main_thread_time_budget_usec = 8000;
	ProgressiveTaskRunner _progressive_task_runner;

	FileLocker _file_locker;
};

struct VoxelFileLockerRead {
	VoxelFileLockerRead(String path) : _path(path) {
		VoxelServer::get_singleton().get_file_locker().lock_read(path);
	}

	~VoxelFileLockerRead() {
		VoxelServer::get_singleton().get_file_locker().unlock(_path);
	}

	String _path;
};

struct VoxelFileLockerWrite {
	VoxelFileLockerWrite(String path) : _path(path) {
		VoxelServer::get_singleton().get_file_locker().lock_write(path);
	}

	~VoxelFileLockerWrite() {
		VoxelServer::get_singleton().get_file_locker().unlock(_path);
	}

	String _path;
};

class BufferedTaskScheduler {
public:
	static BufferedTaskScheduler &get_for_current_thread();

	inline void push_main_task(IThreadedTask *task) {
		_main_tasks.push_back(task);
	}

	inline void push_io_task(IThreadedTask *task) {
		_io_tasks.push_back(task);
	}

	inline void flush() {
		VoxelServer::get_singleton().push_async_tasks(to_span(_main_tasks));
		VoxelServer::get_singleton().push_async_io_tasks(to_span(_io_tasks));
		_main_tasks.clear();
		_io_tasks.clear();
	}

	// No destructor! This does not take ownership, it is only a helper. Flush should be called after each use.

private:
	std::vector<IThreadedTask *> _main_tasks;
	std::vector<IThreadedTask *> _io_tasks;
};

} // namespace zylann::voxel

#endif // VOXEL_SERVER_H
