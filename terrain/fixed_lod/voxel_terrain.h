#ifndef VOXEL_TERRAIN_H
#define VOXEL_TERRAIN_H

#include "../../constants/voxel_constants.h"
#include "../../engine/meshing_dependency.h"
#include "../../storage/voxel_data.h"
#include "../../util/godot/memory.h"
#include "../../util/math/box3i.h"
#include "../voxel_data_block_enter_info.h"
#include "../voxel_mesh_map.h"
#include "../voxel_node.h"
#include "voxel_mesh_block_vt.h"
#include "voxel_terrain_multiplayer_synchronizer.h"

namespace zylann {

class AsyncDependencyTracker;

namespace voxel {

class VoxelTool;
class VoxelInstancer;
class VoxelSaveCompletionTracker;
class VoxelTerrainMultiplayerSynchronizer;

// Infinite paged terrain made of voxel blocks all with the same level of detail.
// Voxels are polygonized around the viewer by distance in a large cubic space.
// Data is streamed using a VoxelStream.
class VoxelTerrain : public VoxelNode {
	GDCLASS(VoxelTerrain, VoxelNode)
public:
	static const unsigned int MAX_VIEW_DISTANCE_FOR_LARGE_VOLUME = 512;

	VoxelTerrain();
	~VoxelTerrain();

	void set_stream(Ref<VoxelStream> p_stream) override;
	Ref<VoxelStream> get_stream() const override;

	void set_generator(Ref<VoxelGenerator> p_generator) override;
	Ref<VoxelGenerator> get_generator() const override;

	void set_mesher(Ref<VoxelMesher> mesher) override;
	Ref<VoxelMesher> get_mesher() const override;

	unsigned int get_data_block_size_pow2() const;
	inline unsigned int get_data_block_size() const {
		return 1 << get_data_block_size_pow2();
	}
	// void set_data_block_size_po2(unsigned int p_block_size_po2);

	unsigned int get_mesh_block_size_pow2() const;
	inline unsigned int get_mesh_block_size() const {
		return 1 << get_mesh_block_size_pow2();
	}
	void set_mesh_block_size(unsigned int p_block_size);

	void post_edit_voxel(Vector3i pos);
	void post_edit_area(Box3i box_in_voxels);

	void set_generate_collisions(bool enabled);
	bool get_generate_collisions() const {
		return _generate_collisions;
	}

	void set_collision_layer(int layer);
	int get_collision_layer() const;

	void set_collision_mask(int mask);
	int get_collision_mask() const;

	void set_collision_margin(float margin);
	float get_collision_margin() const;

	unsigned int get_max_view_distance() const;
	void set_max_view_distance(unsigned int distance_in_voxels);

	void set_block_enter_notification_enabled(bool enable);
	bool is_block_enter_notification_enabled() const;

	void set_area_edit_notification_enabled(bool enable);
	bool is_area_edit_notification_enabled() const;

	void set_automatic_loading_enabled(bool enable);
	bool is_automatic_loading_enabled() const;

	void set_material_override(Ref<Material> material);
	Ref<Material> get_material_override() const;

	VoxelData &get_storage() const {
		ZN_ASSERT(_data != nullptr);
		return *_data;
	}

	std::shared_ptr<VoxelData> get_storage_shared() const {
		return _data;
	}

	Ref<VoxelTool> get_voxel_tool() override;

	// Creates or overrides whatever block data there is at the given position.
	// The use case is multiplayer, client-side.
	// If no local viewer is actually in range, the data will not be applied and the function returns `false`.
	bool try_set_block_data(Vector3i position, std::shared_ptr<VoxelBufferInternal> &voxel_data);

	bool has_data_block(Vector3i position) const;

	void set_run_stream_in_editor(bool enable);
	bool is_stream_running_in_editor() const;

	void set_bounds(Box3i box);
	Box3i get_bounds() const;

	void restart_stream() override;
	void remesh_all_blocks() override;

	// Asks to generate (or re-generate) a block at the given position asynchronously.
	// If the block already exists once the block is generated, it will be cancelled.
	// If the block is out of range of any viewer, it will be cancelled.
	void generate_block_async(Vector3i block_position);

	struct Stats {
		int updated_blocks = 0;
		int dropped_block_loads = 0;
		int dropped_block_meshs = 0;
		uint32_t time_detect_required_blocks = 0;
		uint32_t time_request_blocks_to_load = 0;
		uint32_t time_process_load_responses = 0;
		uint32_t time_request_blocks_to_update = 0;
	};

	const Stats &get_stats() const;

	// struct BlockToSave {
	// 	std::shared_ptr<VoxelBufferInternal> voxels;
	// 	Vector3i position;
	// };

	// Internal

	void set_instancer(VoxelInstancer *instancer);
	void get_meshed_block_positions(std::vector<Vector3i> &out_positions) const;
	Array get_mesh_block_surface(Vector3i block_pos) const;

	VolumeID get_volume_id() const override {
		return _volume_id;
	}

	std::shared_ptr<StreamingDependency> get_streaming_dependency() const override {
		return _streaming_dependency;
	}

	void get_viewers_in_area(std::vector<ViewerID> &out_viewer_ids, Box3i voxel_box) const;

	void set_multiplayer_synchronizer(VoxelTerrainMultiplayerSynchronizer *synchronizer);
	const VoxelTerrainMultiplayerSynchronizer *get_multiplayer_synchronizer() const;

protected:
	void _notification(int p_what);

	void _on_gi_mode_changed() override;
	void _on_shadow_casting_changed() override;

private:
	void process();
	void process_viewers();
	void process_viewer_data_box_change(
			ViewerID viewer_id, Box3i prev_data_box, Box3i new_data_box, bool can_load_blocks);
	// void process_received_data_blocks();
	void process_meshing();
	void apply_mesh_update(const VoxelEngine::BlockMeshOutput &ob);
	void apply_data_block_response(VoxelEngine::BlockDataOutput &ob);

	void _on_stream_params_changed();
	// void _set_block_size_po2(int p_block_size_po2);
	// void make_all_view_dirty();
	void start_updater();
	void stop_updater();
	void start_streamer();
	void stop_streamer();
	void reset_map();

	// void view_data_block(Vector3i bpos, uint32_t viewer_id, bool require_notification);
	void view_mesh_block(Vector3i bpos, bool mesh_flag, bool collision_flag);
	// void unview_data_block(Vector3i bpos);
	void unview_mesh_block(Vector3i bpos, bool mesh_flag, bool collision_flag);
	// void unload_data_block(Vector3i bpos);
	void unload_mesh_block(Vector3i bpos);
	// void make_data_block_dirty(Vector3i bpos);
	void try_schedule_mesh_update(VoxelMeshBlockVT &block);
	void try_schedule_mesh_update_from_data(const Box3i &box_in_voxels);

	void save_all_modified_blocks(bool with_copy, std::shared_ptr<AsyncDependencyTracker> tracker);
	void get_viewer_pos_and_direction(Vector3 &out_pos, Vector3 &out_direction) const;
	void send_data_load_requests();
	void consume_block_data_save_requests(
			BufferedTaskScheduler &task_scheduler, std::shared_ptr<AsyncDependencyTracker> saving_tracker);

	void emit_data_block_loaded(Vector3i bpos);
	void emit_data_block_unloaded(Vector3i bpos);

	void emit_mesh_block_entered(Vector3i bpos);
	void emit_mesh_block_exited(Vector3i bpos);

	bool try_get_paired_viewer_index(ViewerID id, size_t &out_i) const;

	void notify_data_block_enter(const VoxelDataBlock &block, Vector3i bpos, ViewerID viewer_id);

#ifdef ZN_GODOT
	// Called each time a data block enters a viewer's area.
	// This can be either when the block exists and the viewer gets close enough, or when it gets loaded.
	// This only happens if data block enter notifications are enabled.
	GDVIRTUAL1(_on_data_block_entered, VoxelDataBlockEnterInfo *);

	// Called each time voxels are edited within a region.
	GDVIRTUAL2(_on_area_edited, Vector3i, Vector3i);
#elif defined(ZN_GODOT_EXTENSION)
	// TODO GDX: Defining custom virtual functions is not supported...
#endif

	static void _bind_methods();

	// Bindings
	Vector3i _b_voxel_to_data_block(Vector3 pos) const;
	Vector3i _b_data_block_to_voxel(Vector3i pos) const;
	// void _force_load_blocks_binding(Vector3 center, Vector3 extents) { force_load_blocks(center, extents); }
	Ref<VoxelSaveCompletionTracker> _b_save_modified_blocks();
	void _b_save_block(Vector3i p_block_pos);
	void _b_set_bounds(AABB aabb);
	AABB _b_get_bounds() const;
	bool _b_try_set_block_data(Vector3i position, Ref<gd::VoxelBuffer> voxel_data);
	Dictionary _b_get_statistics() const;
	PackedInt32Array _b_get_viewer_network_peer_ids_in_area(Vector3i area_origin, Vector3i area_size) const;
	void _b_rpc_receive_block(PackedByteArray data);
	void _b_rpc_receive_area(PackedByteArray data);

	VolumeID _volume_id;

	// Paired viewers are VoxelViewers which intersect with the boundaries of the volume
	struct PairedViewer {
		struct State {
			Vector3i local_position_voxels;
			Box3i data_box; // In block coordinates
			Box3i mesh_box;
			int view_distance_voxels = 0;
			bool requires_collisions = false;
			bool requires_meshes = false;
		};
		ViewerID id;
		State state;
		State prev_state;
	};

	std::vector<PairedViewer> _paired_viewers;

	// Voxel storage. Using a shared_ptr so threaded tasks can use it safely.
	std::shared_ptr<VoxelData> _data;

	// Mesh storage
	VoxelMeshMap<VoxelMeshBlockVT> _mesh_map;
	uint32_t _mesh_block_size_po2 = constants::DEFAULT_BLOCK_SIZE_PO2;

	unsigned int _max_view_distance_voxels = 128;

	// TODO Terrains only need to handle the visible portion of voxels, which reduces the bounds blocks to handle.
	// Therefore, could a simple grid be better to use than a hashmap?

	struct LoadingBlock {
		RefCount viewers;
		// TODO Optimize allocations here
		std::vector<ViewerID> viewers_to_notify;
	};

	// Blocks currently being loaded.
	std::unordered_map<Vector3i, LoadingBlock> _loading_blocks;
	// Blocks that should be loaded on the next process call.
	// The order in that list does not matter.
	std::vector<Vector3i> _blocks_pending_load;
	// Block meshes that should be updated on the next process call.
	// The order in that list does not matter.
	std::vector<Vector3i> _blocks_pending_update;
	// Blocks that should be saved on the next process call.
	// The order in that list does not matter.
	std::vector<VoxelData::BlockToSave> _blocks_to_save;

	Ref<VoxelMesher> _mesher;

	// Data stored with a shared pointer so it can be sent to asynchronous tasks, and these tasks can be cancelled by
	// setting a bool to false and re-instantiating the structure
	std::shared_ptr<StreamingDependency> _streaming_dependency;
	std::shared_ptr<MeshingDependency> _meshing_dependency;

	bool _generate_collisions = true;
	unsigned int _collision_layer = 1;
	unsigned int _collision_mask = 1;
	float _collision_margin = constants::DEFAULT_COLLISION_MARGIN;
	bool _run_stream_in_editor = true;
	// bool _stream_enabled = false;
	bool _block_enter_notification_enabled = false;
	bool _area_edit_notification_enabled = false;
	// If enabled, VoxelViewers will cause blocks to automatically load around them.
	bool _automatic_loading_enabled = true;

	Ref<Material> _material_override;

	GodotObjectUniquePtr<VoxelDataBlockEnterInfo> _data_block_enter_info_obj;

	// References to external nodes.
	VoxelInstancer *_instancer = nullptr;
	VoxelTerrainMultiplayerSynchronizer *_multiplayer_synchronizer = nullptr;

	Stats _stats;
};

} // namespace voxel
} // namespace zylann

#endif // VOXEL_TERRAIN_H
