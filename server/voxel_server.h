#ifndef VOXEL_SERVER_H
#define VOXEL_SERVER_H

#include "../meshers/blocky/voxel_mesher_blocky.h"
#include "../streams/voxel_stream.h"
#include "struct_db.h"
#include "voxel_thread_pool.h"
#include <scene/main/node.h>

#include <memory>

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
		VoxelMesher::Output blocky_surfaces;
		VoxelMesher::Output smooth_surfaces;
		Vector3i position;
		uint8_t lod;
	};

	struct BlockDataOutput {
		enum Type {
			TYPE_LOAD,
			TYPE_SAVE
		};

		Type type;
		Ref<VoxelBuffer> voxels;
		Vector3i position;
		uint8_t lod;
		bool dropped;
	};

	struct BlockMeshInput {
		// Moore area ordered by forward XYZ iteration
		FixedArray<Ref<VoxelBuffer>, Cube::MOORE_AREA_3D_COUNT> blocks;
		Vector3i position;
		uint8_t lod = 0;
	};

	struct ReceptionBuffers {
		std::vector<BlockMeshOutput> mesh_output;
		std::vector<BlockDataOutput> data_output;
	};

	static VoxelServer *get_singleton();
	static void create_singleton();
	static void destroy_singleton();

	VoxelServer();
	~VoxelServer();

	uint32_t add_volume(ReceptionBuffers *buffers);
	void set_volume_transform(uint32_t volume_id, Transform t);
	void set_volume_block_size(uint32_t volume_id, uint32_t block_size);
	void set_volume_stream(uint32_t volume_id, Ref<VoxelStream> stream);
	void set_volume_voxel_library(uint32_t volume_id, Ref<VoxelLibrary> library);
	void invalidate_volume_mesh_requests(uint32_t volume_id);
	void request_block_mesh(uint32_t volume_id, BlockMeshInput &input);
	void request_block_load(uint32_t volume_id, Vector3i block_pos, int lod);
	void request_block_save(uint32_t volume_id, Ref<VoxelBuffer> voxels, Vector3i block_pos, int lod);
	void remove_volume(uint32_t volume_id);

	uint32_t add_viewer();
	void set_viewer_position(uint32_t viewer_id, Vector3 position);
	void set_viewer_region_extents(uint32_t viewer_id, Vector3 extents);
	void remove_viewer(uint32_t viewer_id);

	// Gets by how much voxels must be padded with neighbors in order to be polygonized properly
	void get_min_max_block_padding(
			bool blocky_enabled, bool smooth_enabled,
			unsigned int &out_min_padding, unsigned int &out_max_padding) const;

	void process();

private:
	Dictionary _b_get_stats();

	static void _bind_methods();

	// Since we are going to send data to tasks running in multiple threads, a few strategies are in place:
	//
	// - Copy the data for each thread. This is suitable for simple information that doesn't change after scheduling.
	//
	// - Per-thread instances. This is done if some heap-allocated class instances are not safe
	//   to use in multiple threads, and don't change after being scheduled.
	//
	// - Shared pointers. This is used if the data can change after being scheduled.
	//   This is often done without locking, but only if it's ok to have dirty reads.
	//   If such data sets change structurally (like their size, or other non-dirty-readable fields),
	//   then a new instance is created and old references are left to "die out".

	// Data common to all requests about a particular volume
	struct StreamingDependency {
		FixedArray<Ref<VoxelStream>, VoxelThreadPool::MAX_THREADS> streams;
		bool valid = true;
	};

	// Data common to all requests about a particular volume
	struct MeshingDependency {
		Ref<VoxelLibrary> library;
		bool valid = true;
	};

	struct Volume {
		ReceptionBuffers *reception_buffers = nullptr;
		Transform transform;
		Ref<VoxelStream> stream;
		Ref<VoxelLibrary> voxel_library;
		uint32_t block_size = 16;
		std::shared_ptr<StreamingDependency> stream_dependency;
		std::shared_ptr<MeshingDependency> meshing_dependency;
	};

	struct Viewer {
		Vector3 world_position;
	};

	struct VoxelViewersArray {
		// These positions are written by the main thread and read by block processing threads.
		// It's only used to adjust task priority so using a lock isn't worth it. In worst case scenario,
		// a task will run much sooner or later than expected, but it will run in any case.
		std::vector<Vector3> positions;
	};

	struct World {
		StructDB<Volume> volumes;
		StructDB<Viewer> viewers;

		// Order doesn't matter.
		// Must be overwritten with a new instance if count changes.
		std::shared_ptr<VoxelViewersArray> viewers_for_priority;
	};

	struct BlockRequestPriorityDependency {
		std::shared_ptr<VoxelViewersArray> viewers;
		Vector3 world_position; // TODO Won't update while in queue. Can it be bad?
	};

	static int get_priority(const BlockRequestPriorityDependency &dep);

	class BlockDataRequest : public IVoxelTask {
	public:
		enum Type {
			TYPE_LOAD = 0,
			TYPE_SAVE
		};

		void run(VoxelTaskContext ctx) override;
		int get_priority() override;
		bool is_cancelled() override;

		Ref<VoxelBuffer> voxels;
		Vector3i position;
		uint32_t volume_id;
		uint8_t lod;
		uint8_t block_size;
		uint8_t type;
		bool has_run = false;
		BlockRequestPriorityDependency priority_dependency;
		std::shared_ptr<StreamingDependency> stream_dependency;
		// TODO Find a way to separate save, it doesnt need sorting
	};

	class BlockMeshRequest : public IVoxelTask {
	public:
		void run(VoxelTaskContext ctx) override;
		int get_priority() override;
		bool is_cancelled() override;

		FixedArray<Ref<VoxelBuffer>, Cube::MOORE_AREA_3D_COUNT> blocks;
		Vector3i position;
		uint32_t volume_id;
		uint8_t lod;
		bool smooth_enabled;
		bool blocky_enabled;
		bool has_run = false;
		BlockRequestPriorityDependency priority_dependency;
		std::shared_ptr<MeshingDependency> meshing_dependency;
		VoxelMesher::Output blocky_surfaces_output;
		VoxelMesher::Output smooth_surfaces_output;
	};

	// TODO multi-world support in the future
	World _world;

	VoxelThreadPool _streaming_thread_pool;
	VoxelThreadPool _meshing_thread_pool;

	// TODO I do this because meshers have memory caches. But perhaps we could put them in thread locals?
	// Used by tasks from threads.
	// Meshers have internal state because they use memory caches,
	// so we instanciate one per thread to be sure it's safe without having to lock.
	// Options such as library etc can change per task.
	FixedArray<Ref<VoxelMesherBlocky>, VoxelThreadPool::MAX_THREADS> _blocky_meshers;
	FixedArray<Ref<VoxelMesher>, VoxelThreadPool::MAX_THREADS> _smooth_meshers;
};

// TODO Hack to make VoxelServer update... need ways to integrate callbacks from main loop!
class VoxelServerUpdater : public Node {
	GDCLASS(VoxelServerUpdater, Node)
public:
	static void ensure_existence(SceneTree *st);

protected:
	void _notification(int p_what) {
		if (p_what == NOTIFICATION_PROCESS) {
			VoxelServer::get_singleton()->process();
		}
	}

private:
	VoxelServerUpdater() {
		set_process(true);
	}
};

#endif // VOXEL_SERVER_H
