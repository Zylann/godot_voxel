#ifndef VOXEL_SERVER_H
#define VOXEL_SERVER_H

#include "../generators/voxel_generator.h"
#include "../meshers/blocky/voxel_mesher_blocky.h"
#include "../streams/voxel_stream.h"
#include "../util/file_locker.h"
#include "struct_db.h"
#include "voxel_thread_pool.h"
#include <scene/main/node.h>

#include <memory>

class IVoxelTimeSpreadTask {
public:
	virtual ~IVoxelTimeSpreadTask() {}
	virtual void run() = 0;
};

// Runs tasks in the caller thread, within a time budget per call.
class VoxelTimeSpreadTaskRunner {
public:
	~VoxelTimeSpreadTaskRunner();

	void push(IVoxelTimeSpreadTask *task);
	void process(uint64_t time_budget_usec);
	void flush();
	unsigned int get_pending_count() const;

private:
	std::queue<IVoxelTimeSpreadTask *> _tasks;
};

class VoxelNode;

// TODO Don't inherit Object. Instead have a Godot wrapper, there is very little use for Object stuff

// Access point for asynchronous voxel processing APIs.
// Functions must be used from the main thread.
class VoxelServer : public Object {
	GDCLASS(VoxelServer, Object)
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
		enum Type {
			TYPE_LOAD,
			TYPE_SAVE
		};

		Type type;
		std::shared_ptr<VoxelBufferInternal> voxels;
		std::unique_ptr<VoxelInstanceBlockData> instances;
		Vector3i position;
		uint8_t lod;
		bool dropped;
		bool max_lod_hint;
	};

	struct BlockMeshInput {
		// Moore area ordered by forward XYZ iteration
		FixedArray<std::shared_ptr<VoxelBufferInternal>, VoxelConstants::MAX_BLOCK_COUNT_PER_REQUEST> data_blocks;
		unsigned int data_blocks_count = 0;
		Vector3i render_block_position;
		uint8_t lod = 0;
	};

	struct ReceptionBuffers {
		void (*mesh_output_callback)(void *, const BlockMeshOutput &) = nullptr;
		void *callback_data = nullptr;
		std::vector<BlockDataOutput> data_output;
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
	};

	enum VolumeType {
		VOLUME_SPARSE_GRID,
		VOLUME_SPARSE_OCTREE
	};

	static VoxelServer *get_singleton();
	static void create_singleton();
	static void destroy_singleton();

	VoxelServer();
	~VoxelServer();

	// TODO Rename functions to C convention
	uint32_t add_volume(ReceptionBuffers *buffers, VolumeType type);
	void set_volume_transform(uint32_t volume_id, Transform t);
	void set_volume_render_block_size(uint32_t volume_id, uint32_t block_size);
	void set_volume_data_block_size(uint32_t volume_id, uint32_t block_size);
	void set_volume_stream(uint32_t volume_id, Ref<VoxelStream> stream);
	void set_volume_generator(uint32_t volume_id, Ref<VoxelGenerator> generator);
	void set_volume_mesher(uint32_t volume_id, Ref<VoxelMesher> mesher);
	void set_volume_octree_lod_distance(uint32_t volume_id, float lod_distance);
	void invalidate_volume_mesh_requests(uint32_t volume_id);
	void request_block_mesh(uint32_t volume_id, const BlockMeshInput &input);
	void request_block_load(uint32_t volume_id, Vector3i block_pos, int lod, bool request_instances);
	void request_voxel_block_save(uint32_t volume_id, std::shared_ptr<VoxelBufferInternal> voxels, Vector3i block_pos,
			int lod);
	void request_instance_block_save(uint32_t volume_id, std::unique_ptr<VoxelInstanceBlockData> instances,
			Vector3i block_pos, int lod);
	void remove_volume(uint32_t volume_id);
	bool is_volume_valid(uint32_t volume_id) const;

	// TODO Rename functions to C convention
	uint32_t add_viewer();
	void remove_viewer(uint32_t viewer_id);
	void set_viewer_position(uint32_t viewer_id, Vector3 position);
	void set_viewer_distance(uint32_t viewer_id, unsigned int distance);
	unsigned int get_viewer_distance(uint32_t viewer_id) const;
	void set_viewer_requires_visuals(uint32_t viewer_id, bool enabled);
	bool is_viewer_requiring_visuals(uint32_t viewer_id) const;
	void set_viewer_requires_collisions(uint32_t viewer_id, bool enabled);
	bool is_viewer_requiring_collisions(uint32_t viewer_id) const;
	bool viewer_exists(uint32_t viewer_id) const;

	template <typename F>
	inline void for_each_viewer(F f) const {
		_world.viewers.for_each_with_id(f);
	}

	void push_time_spread_task(IVoxelTimeSpreadTask *task);
	int get_main_thread_time_budget_usec() const;

	// Gets by how much voxels must be padded with neighbors in order to be polygonized properly
	// void get_min_max_block_padding(
	// 		bool blocky_enabled, bool smooth_enabled,
	// 		unsigned int &out_min_padding, unsigned int &out_max_padding) const;

	void process();
	void wait_and_clear_all_tasks(bool warn);

	inline VoxelFileLocker &get_file_locker() {
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
	class BlockDataRequest;
	class BlockGenerateRequest;

	void request_block_generate_from_data_request(BlockDataRequest &src);
	void request_block_save_from_generate_request(BlockGenerateRequest &src);

	Dictionary _b_get_stats();

	static void _bind_methods();

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

	struct StreamingDependency {
		Ref<VoxelStream> stream;
		Ref<VoxelGenerator> generator;
		bool valid = true;
	};

	struct MeshingDependency {
		Ref<VoxelMesher> mesher;
		bool valid = true;
	};

	struct Volume {
		VolumeType type;
		ReceptionBuffers *reception_buffers = nullptr;
		Transform transform;
		Ref<VoxelStream> stream;
		Ref<VoxelGenerator> generator;
		Ref<VoxelMesher> mesher;
		uint32_t render_block_size = 16;
		uint32_t data_block_size = 16;
		float octree_lod_distance = 0;
		std::shared_ptr<StreamingDependency> stream_dependency;
		std::shared_ptr<MeshingDependency> meshing_dependency;
	};

	struct PriorityDependencyShared {
		// These positions are written by the main thread and read by block processing threads.
		// Order doesn't matter.
		// It's only used to adjust task priority so using a lock isn't worth it. In worst case scenario,
		// a task will run much sooner or later than expected, but it will run in any case.
		std::vector<Vector3> viewers;
		float highest_view_distance = 999999;
	};

	struct World {
		StructDB<Volume> volumes;
		StructDB<Viewer> viewers;

		// Must be overwritten with a new instance if count changes.
		std::shared_ptr<PriorityDependencyShared> shared_priority_dependency;
	};

	struct PriorityDependency {
		std::shared_ptr<PriorityDependencyShared> shared;
		Vector3 world_position; // TODO Won't update while in queue. Can it be bad?
		// If the closest viewer is further away than this distance, the request can be cancelled as not worth it
		float drop_distance_squared;
	};

	void init_priority_dependency(PriorityDependency &dep, Vector3i block_position, uint8_t lod, const Volume &volume,
			int block_size);
	static int get_priority(const PriorityDependency &dep, uint8_t lod_index, float *out_closest_distance_sq);

	class BlockDataRequest : public IVoxelTask {
	public:
		enum Type {
			TYPE_LOAD = 0,
			TYPE_SAVE,
			TYPE_FALLBACK_ON_GENERATOR
		};

		BlockDataRequest();
		~BlockDataRequest();

		void run(VoxelTaskContext ctx) override;
		int get_priority() override;
		bool is_cancelled() override;
		void apply_result() override;

		std::shared_ptr<VoxelBufferInternal> voxels;
		std::unique_ptr<VoxelInstanceBlockData> instances;
		Vector3i position; // In data blocks of the specified lod
		uint32_t volume_id;
		uint8_t lod;
		uint8_t block_size;
		uint8_t type;
		bool has_run = false;
		bool too_far = false;
		bool request_instances = false;
		bool request_voxels = false;
		bool max_lod_hint = false;
		PriorityDependency priority_dependency;
		std::shared_ptr<StreamingDependency> stream_dependency;
		// TODO Find a way to separate save, it doesnt need sorting
	};

	class BlockGenerateRequest : public IVoxelTask {
	public:
		BlockGenerateRequest();
		~BlockGenerateRequest();

		void run(VoxelTaskContext ctx) override;
		int get_priority() override;
		bool is_cancelled() override;
		void apply_result() override;

		std::shared_ptr<VoxelBufferInternal> voxels;
		Vector3i position;
		uint32_t volume_id;
		uint8_t lod;
		uint8_t block_size;
		bool has_run = false;
		bool too_far = false;
		bool max_lod_hint = false;
		PriorityDependency priority_dependency;
		std::shared_ptr<StreamingDependency> stream_dependency;
	};

	class BlockMeshRequest : public IVoxelTask {
	public:
		BlockMeshRequest();
		~BlockMeshRequest();

		void run(VoxelTaskContext ctx) override;
		int get_priority() override;
		bool is_cancelled() override;
		void apply_result() override;

		FixedArray<std::shared_ptr<VoxelBufferInternal>, VoxelConstants::MAX_BLOCK_COUNT_PER_REQUEST> blocks;
		Vector3i position; // In mesh blocks of the specified lod
		uint32_t volume_id;
		uint8_t lod;
		uint8_t blocks_count;
		bool has_run = false;
		bool too_far = false;
		PriorityDependency priority_dependency;
		std::shared_ptr<MeshingDependency> meshing_dependency;
		VoxelMesher::Output surfaces_output;
	};

	// TODO multi-world support in the future
	World _world;

	// Pool specialized in file I/O
	VoxelThreadPool _streaming_thread_pool;
	// Pool for every other task
	VoxelThreadPool _general_thread_pool;
	// For tasks that can only run on the main thread and be spread out over frames
	VoxelTimeSpreadTaskRunner _time_spread_task_runner;
	int _main_thread_time_budget_usec = 8000;

	VoxelFileLocker _file_locker;
};

// TODO Hack to make VoxelServer update... need ways to integrate callbacks from main loop!
class VoxelServerUpdater : public Node {
	GDCLASS(VoxelServerUpdater, Node)
public:
	~VoxelServerUpdater();
	static void ensure_existence(SceneTree *st);

protected:
	void _notification(int p_what);

private:
	VoxelServerUpdater();
};

struct VoxelFileLockerRead {
	VoxelFileLockerRead(String path) :
			_path(path) {
		VoxelServer::get_singleton()->get_file_locker().lock_read(path);
	}

	~VoxelFileLockerRead() {
		VoxelServer::get_singleton()->get_file_locker().unlock(_path);
	}

	String _path;
};

struct VoxelFileLockerWrite {
	VoxelFileLockerWrite(String path) :
			_path(path) {
		VoxelServer::get_singleton()->get_file_locker().lock_write(path);
	}

	~VoxelFileLockerWrite() {
		VoxelServer::get_singleton()->get_file_locker().unlock(_path);
	}

	String _path;
};

#endif // VOXEL_SERVER_H
