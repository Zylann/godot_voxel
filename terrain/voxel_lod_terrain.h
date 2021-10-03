#ifndef VOXEL_LOD_TERRAIN_HPP
#define VOXEL_LOD_TERRAIN_HPP

#include "../server/voxel_server.h"
#include "../storage/voxel_data_map.h"
#include "lod_octree.h"
#include "voxel_mesh_map.h"
#include "voxel_node.h"

#include <scene/3d/spatial.h>
#include <unordered_set>

#ifdef TOOLS_ENABLED
#include "../editor/voxel_debug.h"
#endif

class VoxelTool;
class VoxelStream;
class VoxelInstancer;

// Paged terrain made of voxel blocks of variable level of detail.
// Designed for highest view distances, preferably using smooth voxels.
// Voxels are polygonized around the viewer by distance in a very large sphere, usually extending beyond far clip.
// Data is streamed using a VoxelStream, which must support LOD.
class VoxelLodTerrain : public VoxelNode {
	GDCLASS(VoxelLodTerrain, VoxelNode)
public:
	VoxelLodTerrain();
	~VoxelLodTerrain();

	Ref<Material> get_material() const;
	void set_material(Ref<Material> p_material);

	Ref<VoxelStream> get_stream() const override;
	void set_stream(Ref<VoxelStream> p_stream) override;

	Ref<VoxelGenerator> get_generator() const override;
	void set_generator(Ref<VoxelGenerator> p_stream) override;

	Ref<VoxelMesher> get_mesher() const override;
	void set_mesher(Ref<VoxelMesher> p_mesher) override;

	int get_view_distance() const;
	void set_view_distance(int p_distance_in_voxels);

	void set_lod_distance(float p_lod_distance);
	float get_lod_distance() const;

	void set_lod_count(int p_lod_count);
	int get_lod_count() const;

	void set_generate_collisions(bool enabled);
	bool get_generate_collisions() const { return _generate_collisions; }

	// Sets up to which amount of LODs collision will generate. -1 means all of them.
	void set_collision_lod_count(int lod_count);
	int get_collision_lod_count() const;

	void set_collision_layer(int layer);
	int get_collision_layer() const;

	void set_collision_mask(int mask);
	int get_collision_mask() const;

	void set_collision_margin(float margin);
	float get_collision_margin() const;

	int get_data_block_region_extent() const;
	int get_mesh_block_region_extent() const;

	Vector3 voxel_to_data_block_position(Vector3 vpos, int lod_index) const;
	Vector3 voxel_to_mesh_block_position(Vector3 vpos, int lod_index) const;

	unsigned int get_data_block_size_pow2() const;
	unsigned int get_data_block_size() const;
	//void set_data_block_size_po2(unsigned int p_block_size_po2);

	unsigned int get_mesh_block_size_pow2() const;
	unsigned int get_mesh_block_size() const;
	void set_mesh_block_size(unsigned int mesh_block_size);

	bool is_area_editable(Box3i p_box) const;
	uint64_t get_voxel(Vector3i pos, unsigned int channel, uint64_t defval) const;
	bool try_set_voxel_without_update(Vector3i pos, unsigned int channel, uint64_t value);
	void copy(Vector3i p_origin_voxels, VoxelBufferInternal &dst_buffer, uint8_t channels_mask) const;

	template <typename F>
	void write_box(const Box3i &p_voxel_box, unsigned int channel, F action) {
		const Box3i voxel_box = p_voxel_box.clipped(_bounds_in_voxels);
		if (is_area_editable(voxel_box)) {
			_lods[0].data_map.write_box(voxel_box, channel, action);
			post_edit_area(voxel_box);
		} else {
			PRINT_VERBOSE("Area not editable");
		}
	}

	template <typename F>
	void write_box_2(const Box3i &p_voxel_box, unsigned int channel1, unsigned int channel2, F action) {
		const Box3i voxel_box = p_voxel_box.clipped(_bounds_in_voxels);
		if (is_area_editable(voxel_box)) {
			_lods[0].data_map.write_box_2(voxel_box, channel1, channel2, action);
			post_edit_area(voxel_box);
		} else {
			PRINT_VERBOSE("Area not editable");
		}
	}

	// These must be called after an edit
	void post_edit_area(Box3i p_box);

	void set_voxel_bounds(Box3i p_box);
	inline Box3i get_voxel_bounds() const { return _bounds_in_voxels; }

	void set_collision_update_delay(int delay_msec);
	int get_collision_update_delay() const;

	void set_lod_fade_duration(float seconds);
	float get_lod_fade_duration() const;

	enum ProcessMode {
		PROCESS_MODE_IDLE = 0,
		PROCESS_MODE_PHYSICS,
		PROCESS_MODE_DISABLED
	};

	// This was originally added to fix a problem with rigidbody teleportation and floating world origin:
	// The player teleported at a different rate than the rest of the world due to delays in transform updates,
	// which caused the world to unload and then reload entirely over the course of 3 frames,
	// producing flickers and CPU lag. Changing process mode allows to align update rate,
	// and freeze LOD for the duration of the teleport.
	void set_process_mode(ProcessMode mode);
	ProcessMode get_process_mode() const { return _process_mode; }

	Ref<VoxelTool> get_voxel_tool();

	struct Stats {
		int blocked_lods = 0;
		int updated_blocks = 0;
		int dropped_block_loads = 0;
		int dropped_block_meshs = 0;
		uint32_t time_detect_required_blocks = 0;
		uint32_t time_request_blocks_to_load = 0;
		uint32_t time_process_load_responses = 0;
		uint32_t time_request_blocks_to_update = 0;
	};

	const Stats &get_stats() const;

	void restart_stream() override;
	void remesh_all_blocks() override;

	struct BlockToSave {
		std::shared_ptr<VoxelBufferInternal> voxels;
		Vector3i position;
		uint8_t lod;
	};

	// Debugging

	Array debug_raycast_mesh_block(Vector3 world_origin, Vector3 world_direction) const;
	Dictionary debug_get_data_block_info(Vector3 fbpos, int lod_index) const;
	Dictionary debug_get_mesh_block_info(Vector3 fbpos, int lod_index) const;
	Array debug_get_octree_positions() const;
	Array debug_get_octrees_detailed() const;

	// Editor

	void set_run_stream_in_editor(bool enable);
	bool is_stream_running_in_editor() const;

#ifdef TOOLS_ENABLED
	void set_show_gizmos(bool enable);
	bool is_showing_gizmos() const { return _show_gizmos_enabled; }
#endif

	String get_configuration_warning() const override;

	// Internal

	void set_instancer(VoxelInstancer *instancer);
	uint32_t get_volume_id() const { return _volume_id; }

	Array get_mesh_block_surface(Vector3i block_pos, int lod_index) const;
	Vector<Vector3i> get_meshed_block_positions_at_lod(int lod_index) const;

protected:
	void _notification(int p_what);

private:
	void _process(float delta);
	void apply_mesh_update(const VoxelServer::BlockMeshOutput &ob);

	void unload_data_block(Vector3i block_pos, int lod_index);
	void unload_mesh_block(Vector3i block_pos, int lod_index);

	static inline bool check_block_sizes(int data_block_size, int mesh_block_size) {
		return (data_block_size == 16 || data_block_size == 32) &&
			   (mesh_block_size == 16 || mesh_block_size == 32) &&
			   mesh_block_size >= data_block_size;
	}

	void start_updater();
	void stop_updater();
	void start_streamer();
	void stop_streamer();
	void reset_maps();

	Vector3 get_local_viewer_pos() const;
	void try_schedule_loading_with_neighbors(const Vector3i &p_data_block_pos, int lod_index);
	bool is_block_surrounded(const Vector3i &p_bpos, int lod_index, const VoxelDataMap &map) const;
	bool check_block_loaded_and_meshed(const Vector3i &p_mesh_block_pos, int lod_index);
	bool check_block_mesh_updated(VoxelMeshBlock *block);
	void _set_lod_count(int p_lod_count);
	void _set_block_size_po2(int p_block_size_po2);
	void set_mesh_block_active(VoxelMeshBlock &block, bool active);

	void _on_stream_params_changed();

	void flush_pending_lod_edits();
	void save_all_modified_blocks(bool with_copy);
	void send_block_data_requests();
	void process_deferred_collision_updates(uint32_t timeout_msec);
	void process_fading_blocks(float delta);

	void add_transition_update(VoxelMeshBlock *block);
	void add_transition_updates_around(Vector3i block_pos, int lod_index);
	void process_transition_updates();
	uint8_t get_transition_mask(Vector3i block_pos, int lod_index) const;

	void _b_save_modified_blocks();
	void _b_set_voxel_bounds(AABB aabb);
	AABB _b_get_voxel_bounds() const;
	Array _b_debug_print_sdf_top_down(Vector3 center, Vector3 extents) const;
	int _b_debug_get_mesh_block_count() const;
	int _b_debug_get_data_block_count() const;
	Error _b_debug_dump_as_scene(String fpath) const;
	Dictionary _b_get_statistics() const;

	struct OctreeItem {
		LodOctree octree;
	};

#ifdef TOOLS_ENABLED
	void update_gizmos();
#endif

	static void _bind_methods();

private:
	// This terrain type is a sparse grid of octrees.
	// Indexed by a grid coordinate whose step is the size of the highest-LOD block.
	// Not using a pointer because Map storage is stable.
	Map<Vector3i, OctreeItem> _lod_octrees;
	Box3i _last_octree_region_box;

	// Area within which voxels can exist.
	// Note, these bounds might not be exactly represented. This volume is chunk-based, so the result will be
	// approximated to the closest chunk.
	Box3i _bounds_in_voxels;
	//Box3i _prev_bounds_in_voxels;

	Ref<VoxelStream> _stream;
	Ref<VoxelGenerator> _generator;
	Ref<VoxelMesher> _mesher;

	// TODO Might put this batch into VoxelServer directly instead of here
	std::vector<BlockToSave> _blocks_to_save;

	VoxelServer::ReceptionBuffers _reception_buffers;
	uint32_t _volume_id = 0;
	ProcessMode _process_mode = PROCESS_MODE_IDLE;

	// Only populated and then cleared inside _process, so lifetime of pointers should be valid
	std::vector<VoxelMeshBlock *> _blocks_pending_transition_update;

	Ref<Material> _material;
	std::vector<Ref<ShaderMaterial>> _shader_material_pool;

	bool _generate_collisions = true;
	unsigned int _collision_lod_count = 0;
	unsigned int _collision_layer = 1;
	unsigned int _collision_mask = 1;
	float _collision_margin = VoxelConstants::DEFAULT_COLLISION_MARGIN;
	int _collision_update_delay = 0;

	VoxelInstancer *_instancer = nullptr;

	// Each LOD works in a set of coordinates spanning 2x more voxels the higher their index is
	struct Lod {
		VoxelDataMap data_map;
		std::unordered_set<Vector3i> loading_blocks;
		// Blocks that were edited and need their LOD counterparts to be updated
		std::vector<Vector3i> blocks_pending_lodding;
		// These are relative to this LOD, in block coordinates
		Vector3i last_viewer_data_block_pos;
		int last_view_distance_data_blocks = 0;

		VoxelMeshMap mesh_map;
		std::vector<Vector3i> blocks_pending_update;
		std::vector<Vector3i> deferred_collision_updates;
		Map<Vector3i, VoxelMeshBlock *> fading_blocks;
		Vector3i last_viewer_mesh_block_pos;
		int last_view_distance_mesh_blocks = 0;

		// Members for memory caching
		std::vector<Vector3i> blocks_to_load;

		inline bool has_loading_block(const Vector3i &pos) const {
			return loading_blocks.find(pos) != loading_blocks.end();
		}
	};

	FixedArray<Lod, VoxelConstants::MAX_LOD> _lods;
	unsigned int _lod_count = 0;
	// Distance between a viewer and the end of LOD0
	float _lod_distance = 0.f;
	float _lod_fade_duration = 0.f;
	unsigned int _view_distance_voxels = 512;

	bool _run_stream_in_editor = true;
#ifdef TOOLS_ENABLED
	bool _show_gizmos_enabled = false;
	bool _show_octree_bounds_gizmos = true;
	bool _show_volume_bounds_gizmos = true;
	bool _show_octree_node_gizmos = false;
	VoxelDebug::DebugRenderer _debug_renderer;
#endif

	Stats _stats;
};

VARIANT_ENUM_CAST(VoxelLodTerrain::ProcessMode)

#endif // VOXEL_LOD_TERRAIN_HPP
