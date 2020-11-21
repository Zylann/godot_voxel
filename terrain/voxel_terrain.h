#ifndef VOXEL_TERRAIN_H
#define VOXEL_TERRAIN_H

#include "../server/voxel_server.h"
#include "voxel_map.h"

#include <scene/3d/spatial.h>

class VoxelTool;

// Infinite paged terrain made of voxel blocks all with the same level of detail.
// Voxels are polygonized around the viewer by distance in a large cubic space.
// Data is streamed using a VoxelStream.
class VoxelTerrain : public Spatial {
	GDCLASS(VoxelTerrain, Spatial)
public:
	static const unsigned int MAX_VIEW_DISTANCE_FOR_LARGE_VOLUME = 512;

	VoxelTerrain();
	~VoxelTerrain();

	String get_configuration_warning() const override;

	void set_stream(Ref<VoxelStream> p_stream);
	Ref<VoxelStream> get_stream() const;

	unsigned int get_block_size_pow2() const;
	void set_block_size_po2(unsigned int p_block_size_po2);

	void set_voxel_library(Ref<VoxelLibrary> library);
	Ref<VoxelLibrary> get_voxel_library() const;

	void make_voxel_dirty(Vector3i pos);
	void make_area_dirty(Rect3i box);

	void set_generate_collisions(bool enabled);
	bool get_generate_collisions() const { return _generate_collisions; }

	unsigned int get_max_view_distance() const;
	void set_max_view_distance(unsigned int distance_in_voxels);

	// TODO Make this obsolete with multi-viewers
	void set_viewer_path(NodePath path);
	NodePath get_viewer_path() const;

	void set_material(unsigned int id, Ref<Material> material);
	Ref<Material> get_material(unsigned int id) const;

	VoxelMap &get_storage() { return _map; }
	const VoxelMap &get_storage() const { return _map; }

	Ref<VoxelTool> get_voxel_tool();

	void set_run_stream_in_editor(bool enable);
	bool is_stream_running_in_editor() const;

	void set_bounds(Rect3i box);
	Rect3i get_bounds() const;

	void restart_stream();

	struct Stats {
		int updated_blocks = 0;
		int dropped_block_loads = 0;
		int dropped_block_meshs = 0;
		uint64_t time_detect_required_blocks = 0;
		uint64_t time_request_blocks_to_load = 0;
		uint64_t time_process_load_responses = 0;
		uint64_t time_request_blocks_to_update = 0;
		uint64_t time_process_update_responses = 0;
	};

	struct BlockToSave {
		Ref<VoxelBuffer> voxels;
		Vector3i position;
	};

protected:
	void _notification(int p_what);

private:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	void _process();

	void _on_stream_params_changed();
	void _set_block_size_po2(int p_block_size_po2);
	void make_all_view_dirty();
	void start_updater();
	void stop_updater();
	void start_streamer();
	void stop_streamer();
	void reset_map();

	void view_block(Vector3i bpos, bool data_flag, bool mesh_flag, bool collision_flag);
	void unview_block(Vector3i bpos, bool data_flag, bool mesh_flag, bool collision_flag);
	void immerge_block(Vector3i bpos);
	void make_block_dirty(Vector3i bpos);
	void make_block_dirty(VoxelBlock *block);
	void try_schedule_block_update(VoxelBlock *block);

	void save_all_modified_blocks(bool with_copy);
	void get_viewer_pos_and_direction(Vector3 &out_pos, Vector3 &out_direction) const;
	void send_block_data_requests();

	void emit_block_loaded(const VoxelBlock *block);
	void emit_block_unloaded(const VoxelBlock *block);

	bool try_get_paired_viewer_index(uint32_t id, size_t &out_i) const;

	Dictionary get_statistics() const;

	static void _bind_methods();

	// Bindings
	Vector3 _b_voxel_to_block(Vector3 pos);
	Vector3 _b_block_to_voxel(Vector3 pos);
	//void _force_load_blocks_binding(Vector3 center, Vector3 extents) { force_load_blocks(center, extents); }
	void _b_save_modified_blocks();
	void _b_save_block(Vector3 p_block_pos);
	void _b_set_bounds(AABB aabb);
	AABB _b_get_bounds() const;

	uint32_t _volume_id = 0;
	VoxelServer::ReceptionBuffers _reception_buffers;

	struct PairedViewer {
		struct State {
			Vector3i block_position;
			int view_distance_blocks = 0;
			bool requires_collisions = false;
			bool requires_meshes = false;
		};
		uint32_t id;
		State state;
		State prev_state;
	};

	std::vector<PairedViewer> _paired_viewers;

	// Voxel storage
	VoxelMap _map;

	// Area within which voxels can exist.
	// Note, these bounds might not be exactly represented. This volume is chunk-based, so the result will be
	// approximated to the closest chunk.
	Rect3i _bounds_in_voxels;
	Rect3i _prev_bounds_in_voxels;

	// How many blocks to load around the viewer
	unsigned int _max_view_distance_blocks = 8;

	// TODO Terrains only need to handle the visible portion of voxels, which reduces the bounds blocks to handle.
	// Therefore, could a simple grid be better to use than a hashmap?

	struct LoadingBlock {
		VoxelViewerRefCount viewers;
	};

	HashMap<Vector3i, LoadingBlock, Vector3iHasher> _loading_blocks;
	std::vector<Vector3i> _blocks_pending_load;
	std::vector<Vector3i> _blocks_pending_update;
	std::vector<BlockToSave> _blocks_to_save;

	Ref<VoxelStream> _stream;

	Ref<VoxelLibrary> _library;

	bool _generate_collisions = true;
	bool _run_stream_in_editor = true;
	//bool _stream_enabled = false;

	Ref<Material> _materials[VoxelMesherBlocky::MAX_MATERIALS];

	Stats _stats;
};

#endif // VOXEL_TERRAIN_H
