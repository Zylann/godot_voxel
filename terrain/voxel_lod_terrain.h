#ifndef VOXEL_LOD_TERRAIN_HPP
#define VOXEL_LOD_TERRAIN_HPP

#include "../server/voxel_server.h"
#include "lod_octree.h"
#include "voxel_map.h"
#include <core/set.h>
#include <scene/3d/spatial.h>

#ifdef TOOLS_ENABLED
#include "../editor/voxel_debug.h"
#endif

class VoxelTool;
class VoxelStream;

// Paged terrain made of voxel blocks of variable level of detail.
// Designed for highest view distances, preferably using smooth voxels.
// Voxels are polygonized around the viewer by distance in a very large sphere, usually extending beyond far clip.
// Data is streamed using a VoxelStream, which must support LOD.
class VoxelLodTerrain : public Spatial {
	GDCLASS(VoxelLodTerrain, Spatial)
public:
	VoxelLodTerrain();
	~VoxelLodTerrain();

	String get_configuration_warning() const override;

	Ref<Material> get_material() const;
	void set_material(Ref<Material> p_material);

	Ref<VoxelStream> get_stream() const;
	void set_stream(Ref<VoxelStream> p_stream);

	int get_view_distance() const;
	void set_view_distance(int p_distance_in_voxels);

	void set_lod_split_scale(float p_lod_split_scale);
	float get_lod_split_scale() const;

	void set_lod_count(int p_lod_count);
	int get_lod_count() const;

	void set_generate_collisions(bool enabled);
	bool get_generate_collisions() const { return _generate_collisions; }

	// Sets up to which amount of LODs collision will generate. -1 means all of them.
	void set_collision_lod_count(int lod_count);
	int get_collision_lod_count() const;

	int get_block_region_extent() const;
	Vector3 voxel_to_block_position(Vector3 vpos, int lod_index) const;

	unsigned int get_block_size_pow2() const;
	void set_block_size_po2(unsigned int p_block_size_po2);
	unsigned int get_block_size() const;

	// These must be called after an edit
	void post_edit_area(Rect3i p_box);
	void post_edit_block_lod0(Vector3i bpos);

	void set_voxel_bounds(Rect3i p_box);
	inline Rect3i get_voxel_bounds() const { return _bounds_in_voxels; }

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
		uint64_t time_detect_required_blocks = 0;
		uint64_t time_request_blocks_to_load = 0;
		uint64_t time_process_load_responses = 0;
		uint64_t time_request_blocks_to_update = 0;
		uint64_t time_process_update_responses = 0;
	};

	Dictionary get_statistics() const;

	void set_run_stream_in_editor(bool enable);
	bool is_stream_running_in_editor() const;

	void restart_stream();

	struct BlockToSave {
		Ref<VoxelBuffer> voxels;
		Vector3i position;
		uint8_t lod;
	};

	Array debug_raycast_block(Vector3 world_origin, Vector3 world_direction) const;
	Dictionary debug_get_block_info(Vector3 fbpos, int lod_index) const;
	Array debug_get_octrees() const;

#ifdef TOOLS_ENABLED
	void set_show_gizmos(bool enable);
	bool is_showing_gizmos() const { return _show_gizmos_enabled; }
#endif

protected:
	static void _bind_methods();

	void _notification(int p_what);
	void _process();

private:
	void immerge_block(Vector3i block_pos, int lod_index);

	void start_updater();
	void stop_updater();
	void start_streamer();
	void stop_streamer();
	void reset_maps();

	Vector3 get_local_viewer_pos() const;
	void try_schedule_loading_with_neighbors(const Vector3i &p_bpos, int lod_index);
	bool is_block_surrounded(const Vector3i &p_bpos, int lod_index, const VoxelMap &map) const;
	bool check_block_loaded_and_updated(const Vector3i &p_bpos, int lod_index);
	bool check_block_mesh_updated(VoxelBlock *block);
	void _set_lod_count(int p_lod_count);
	void _set_block_size_po2(int p_block_size_po2);

	void _on_stream_params_changed();

	void flush_pending_lod_edits();
	void save_all_modified_blocks(bool with_copy);
	void send_block_data_requests();

	void add_transition_update(VoxelBlock *block);
	void add_transition_updates_around(Vector3i block_pos, int lod_index);
	void process_transition_updates();
	uint8_t get_transition_mask(Vector3i block_pos, int lod_index) const;

	void _b_save_modified_blocks();
	void _b_set_voxel_bounds(AABB aabb);
	AABB _b_get_voxel_bounds() const;
	Array _b_debug_print_sdf_top_down(Vector3 center, Vector3 extents) const;
	int _b_debug_get_block_count() const;
	Error _b_debug_dump_as_scene(String fpath) const;

	struct OctreeItem {
		LodOctree octree;
	};

#ifdef TOOLS_ENABLED
	void update_gizmos();
#endif

private:
	// This terrain type is a sparse grid of octrees.
	// Indexed by a grid coordinate whose step is the size of the highest-LOD block.
	// Not using a pointer because Map storage is stable.
	Map<Vector3i, OctreeItem> _lod_octrees;
	Rect3i _last_octree_region_box;

	// Area within which voxels can exist.
	// Note, these bounds might not be exactly represented. This volume is chunk-based, so the result will be
	// approximated to the closest chunk.
	Rect3i _bounds_in_voxels;
	//Rect3i _prev_bounds_in_voxels;

	Ref<VoxelStream> _stream;
	std::vector<BlockToSave> _blocks_to_save;
	VoxelServer::ReceptionBuffers _reception_buffers;
	uint32_t _volume_id = 0;
	ProcessMode _process_mode = PROCESS_MODE_IDLE;

	// Only populated and then cleared inside _process, so lifetime of pointers should be valid
	std::vector<VoxelBlock *> _blocks_pending_transition_update;

	Ref<Material> _material;
	std::vector<Ref<ShaderMaterial> > _shader_material_pool;

	bool _generate_collisions = true;
	int _collision_lod_count = -1;

	// Each LOD works in a set of coordinates spanning 2x more voxels the higher their index is
	struct Lod {
		VoxelMap map;
		Set<Vector3i> loading_blocks;
		std::vector<Vector3i> blocks_pending_update;

		// Blocks that were edited and need their LOD counterparts to be updated
		std::vector<Vector3i> blocks_pending_lodding;

		// These are relative to this LOD, in block coordinates
		Vector3i last_viewer_block_pos;
		int last_view_distance_blocks = 0;

		// Members for memory caching
		std::vector<Vector3i> blocks_to_load;
	};

	FixedArray<Lod, VoxelConstants::MAX_LOD> _lods;
	int _lod_count = 0;
	float _lod_split_scale = 0.f;
	unsigned int _view_distance_voxels = 512;

	bool _run_stream_in_editor = true;
#ifdef TOOLS_ENABLED
	bool _show_gizmos_enabled = false;
	VoxelDebug::DebugRenderer _debug_renderer;
#endif

	Stats _stats;
};

VARIANT_ENUM_CAST(VoxelLodTerrain::ProcessMode)

#endif // VOXEL_LOD_TERRAIN_HPP
