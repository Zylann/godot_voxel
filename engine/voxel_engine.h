#ifndef VOXEL_ENGINE_H
#define VOXEL_ENGINE_H

#include "../meshers/voxel_mesher.h"
#include "../streams/instance_data.h"
#include "../util/file_locker.h"
#include "../util/memory.h"
#include "../util/slot_map.h"
#include "../util/tasks/progressive_task_runner.h"
#include "../util/tasks/threaded_task_runner.h"
#include "../util/tasks/time_spread_task_runner.h"
#include "compute_shader.h"
#include "detail_rendering.h"
#include "gpu_storage_buffer_pool.h"
#include "gpu_task_runner.h"
#include "ids.h"
#include "priority_dependency.h"

#include "../util/godot/classes/rendering_device.h"

ZN_GODOT_FORWARD_DECLARE(class RenderingDevice);
#ifdef ZN_GODOT_EXTENSION
using namespace godot;
#endif

namespace zylann::voxel {

// Singleton for common things, notably the task system and shared viewers list.
// In Godot terminology this used to be called a "server", but I dont really agree with the term here, and it can be
// confused with networking features.
class VoxelEngine {
public:
	struct BlockMeshOutput {
		enum Type {
			TYPE_MESHED, // Contains mesh
			TYPE_DROPPED // Indicates the meshing was cancelled
		};

		Type type;
		VoxelMesher::Output surfaces;
		// Only used if `has_mesh_resource` is true (usually when meshes are allowed to be build in threads). Otherwise,
		// mesh data will be in `surfaces` and has to be built on the main thread.
		Ref<Mesh> mesh;
		// Remaps Mesh surface indices to Mesher material indices. Only used if `has_mesh_resource` is true.
		// TODO Optimize: candidate for small vector optimization. A big majority of meshes will have a handful of
		// surfaces, which would fit here without allocating.
		std::vector<uint8_t> mesh_material_indices;
		// In mesh block coordinates
		Vector3i position;
		// TODO Rename lod_index
		uint8_t lod;
		// Tells if the mesh resource was built as part of the task. If not, you need to build it on the main thread.
		bool has_mesh_resource;
		// Can be null. Attached to meshing output so it is tracked more easily, because it is baked asynchronously
		// starting from the mesh task, and it might complete earlier or later than the mesh.
		std::shared_ptr<DetailTextureOutput> detail_textures;
	};

	struct BlockDataOutput {
		enum Type { //
			TYPE_LOADED,
			TYPE_GENERATED,
			TYPE_SAVED
		};

		Type type;
		// If voxels are null with TYPE_LOADED, it means no block was found in the stream (if any) and no generator task
		// was scheduled. This is the case when we don't want to cache blocks of generated data.
		std::shared_ptr<VoxelBufferInternal> voxels;
		UniquePtr<InstanceBlockData> instances;
		Vector3i position;
		// TODO Rename lod_index
		uint8_t lod;
		bool dropped;
		bool max_lod_hint;
		// Blocks with this flag set should not be ignored.
		// This is used when data streaming is off, all blocks are loaded at once.
		bool initial_load;
	};

	struct BlockDetailTextureOutput {
		std::shared_ptr<DetailTextureOutput> detail_textures;
		Vector3i position;
		uint32_t lod_index;
	};

	struct VolumeCallbacks {
		void (*mesh_output_callback)(void *, BlockMeshOutput &) = nullptr;
		void (*data_output_callback)(void *, BlockDataOutput &) = nullptr;
		void (*virtual_texture_output_callback)(void *, BlockDetailTextureOutput &) = nullptr;
		void *data = nullptr;

		inline bool check_callbacks() const {
			ZN_ASSERT_RETURN_V(mesh_output_callback != nullptr, false);
			ZN_ASSERT_RETURN_V(data_output_callback != nullptr, false);
			// ZN_ASSERT_RETURN_V(normalmap_output_callback != nullptr, false);
			ZN_ASSERT_RETURN_V(data != nullptr, false);
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

	struct ThreadsConfig {
		int thread_count_minimum = 1;
		// How many threads below available count on the CPU should we set as limit
		int thread_count_margin_below_max = 1;
		// Portion of available CPU threads to attempt using
		float thread_count_ratio_over_max = 0.5;
	};

	static VoxelEngine &get_singleton();
	static void create_singleton(ThreadsConfig threads_config);
	static void destroy_singleton();

	VolumeID add_volume(VolumeCallbacks callbacks);
	VolumeCallbacks get_volume_callbacks(VolumeID volume_id) const;

	void remove_volume(VolumeID volume_id);
	bool is_volume_valid(VolumeID volume_id) const;

	std::shared_ptr<PriorityDependency::ViewersData> get_shared_viewers_data_from_default_world() const {
		return _world.shared_priority_dependency;
	}

	ViewerID add_viewer();
	void remove_viewer(ViewerID viewer_id);
	void set_viewer_position(ViewerID viewer_id, Vector3 position);
	void set_viewer_distance(ViewerID viewer_id, unsigned int distance);
	unsigned int get_viewer_distance(ViewerID viewer_id) const;
	void set_viewer_requires_visuals(ViewerID viewer_id, bool enabled);
	bool is_viewer_requiring_visuals(ViewerID viewer_id) const;
	void set_viewer_requires_collisions(ViewerID viewer_id, bool enabled);
	bool is_viewer_requiring_collisions(ViewerID viewer_id) const;
	void set_viewer_requires_data_block_notifications(ViewerID viewer_id, bool enabled);
	bool is_viewer_requiring_data_block_notifications(ViewerID viewer_id) const;
	void set_viewer_network_peer_id(ViewerID viewer_id, int peer_id);
	int get_viewer_network_peer_id(ViewerID viewer_id) const;
	bool viewer_exists(ViewerID viewer_id) const;

	template <typename F>
	inline void for_each_viewer(F f) const {
		_world.viewers.for_each_key_value(f);
	}

	void push_main_thread_time_spread_task(
			ITimeSpreadTask *task, TimeSpreadTaskRunner::Priority priority = TimeSpreadTaskRunner::PRIORITY_NORMAL);
	int get_main_thread_time_budget_usec() const;
	void set_main_thread_time_budget_usec(unsigned int usec);

	// Allows/disallows building Mesh and Texture resources from inside threads.
	// Depends on Godot's efficiency at doing so, and which renderer is used.
	// For example, the OpenGL renderer does not support this well, but the Vulkan one should.
	void set_threaded_graphics_resource_building_enabled(bool enable);
	// This should be fast and safe to access from multiple threads.
	bool is_threaded_graphics_resource_building_enabled() const;

	void push_main_thread_progressive_task(IProgressiveTask *task);

	// Thread-safe.
	void push_async_task(IThreadedTask *task);
	// Thread-safe.
	void push_async_tasks(Span<IThreadedTask *> tasks);
	// Thread-safe.
	void push_async_io_task(IThreadedTask *task);
	// Thread-safe.
	void push_async_io_tasks(Span<IThreadedTask *> tasks);
	void push_gpu_task(IGPUTask *task);

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
			FixedArray<const char *, ThreadedTaskRunner::MAX_THREADS> active_task_names;
		};

		ThreadPoolStats general;
		int generation_tasks;
		int streaming_tasks;
		int meshing_tasks;
		int main_thread_tasks;
	};

	Stats get_stats() const;

	bool has_rendering_device() const {
		return _rendering_device != nullptr;
	}

	RenderingDevice &get_rendering_device() const {
		ZN_ASSERT(_rendering_device != nullptr);
		return *_rendering_device;
	}

	const ComputeShader &get_dilate_normalmap_compute_shader() const {
		return _dilate_normalmap_shader;
	}

	const ComputeShader &get_detail_gather_hits_compute_shader() const {
		return _detail_gather_hits_shader;
	}

	const ComputeShader &get_detail_normalmap_compute_shader() const {
		return _detail_normalmap_shader;
	}

	const ComputeShader &get_detail_modifier_sphere_shader() const {
		return _detail_modifier_sphere_shader;
	}

	const ComputeShader &get_detail_modifier_mesh_shader() const {
		return _detail_modifier_mesh_shader;
	}

	RID get_filtering_sampler() const {
		return _filtering_sampler_rid;
	}

	// TODO Should be private, but can't because `memdelete<T>` would be unable to call it otherwise...
	~VoxelEngine();

private:
	VoxelEngine(ThreadsConfig threads_config);

	void load_shaders();

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
		VolumeCallbacks callbacks;
	};

	struct World {
		SlotMap<Volume, uint16_t, uint16_t> volumes;
		SlotMap<Viewer, uint16_t, uint16_t> viewers;

		// Must be overwritten with a new instance if count changes.
		std::shared_ptr<PriorityDependency::ViewersData> shared_priority_dependency;
	};

	// TODO multi-world support in the future
	World _world;

	ThreadedTaskRunner _general_thread_pool;
	// For tasks that can only run on the main thread and be spread out over frames
	TimeSpreadTaskRunner _time_spread_task_runner;
	unsigned int _main_thread_time_budget_usec = 8000;
	ProgressiveTaskRunner _progressive_task_runner;

	FileLocker _file_locker;

	bool _threaded_graphics_resource_building_enabled = false;

	// Rendering device used for compute shaders. May not be available depending on the chosen renderer.
	RenderingDevice *_rendering_device = nullptr;
	RID _filtering_sampler_rid;
	// TODO Can `RenderingDevice` be used on multiple threads? There is no documentation.
	// So I'll assume I can't...
	Mutex _rendering_device_mutex;
	GPUTaskRunner _gpu_task_runner;
	GPUStorageBufferPool _gpu_storage_buffer_pool;

	// TODO I don't know yet where to store these resource, at some point we may find a more dedicated place
	ComputeShader _dilate_normalmap_shader;
	ComputeShader _detail_gather_hits_shader;
	ComputeShader _detail_normalmap_shader;
	ComputeShader _detail_modifier_sphere_shader;
	ComputeShader _detail_modifier_mesh_shader;
};

struct VoxelFileLockerRead {
	VoxelFileLockerRead(const std::string &path) : _path(path) {
		VoxelEngine::get_singleton().get_file_locker().lock_read(path);
	}

	~VoxelFileLockerRead() {
		VoxelEngine::get_singleton().get_file_locker().unlock(_path);
	}

	std::string _path;
};

struct VoxelFileLockerWrite {
	VoxelFileLockerWrite(const std::string &path) : _path(path) {
		VoxelEngine::get_singleton().get_file_locker().lock_write(path);
	}

	~VoxelFileLockerWrite() {
		VoxelEngine::get_singleton().get_file_locker().unlock(_path);
	}

	std::string _path;
};

// Helper class to store tasks and schedule them in a single batch
class BufferedTaskScheduler {
public:
	static BufferedTaskScheduler &get_for_current_thread();

	inline void push_main_task(IThreadedTask *task) {
		_main_tasks.push_back(task);
	}

	inline void push_io_task(IThreadedTask *task) {
		_io_tasks.push_back(task);
	}

	inline size_t get_main_count() const {
		return _main_tasks.size();
	}

	inline size_t get_io_count() const {
		return _io_tasks.size();
	}

	void flush();

	// No destructor! This does not take ownership, it is only a helper. Flush should be called after each use.

private:
	BufferedTaskScheduler();

	bool has_tasks() const {
		return _main_tasks.size() > 0 || _io_tasks.size() > 0;
	}

	std::vector<IThreadedTask *> _main_tasks;
	std::vector<IThreadedTask *> _io_tasks;
	Thread::ID _thread_id;
};

} // namespace zylann::voxel

#endif // VOXEL_ENGINE_H
