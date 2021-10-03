#ifndef VOXEL_TERRAIN_H
#define VOXEL_TERRAIN_H

#include "../server/voxel_server.h"
#include "../storage/voxel_data_map.h"
#include "voxel_mesh_map.h"
#include "voxel_node.h"

#include <scene/3d/spatial.h>

class VoxelTool;

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
	inline unsigned int get_data_block_size() const { return 1 << get_data_block_size_pow2(); }
	//void set_data_block_size_po2(unsigned int p_block_size_po2);

	unsigned int get_mesh_block_size_pow2() const;
	inline unsigned int get_mesh_block_size() const { return 1 << get_mesh_block_size_pow2(); }
	void set_mesh_block_size(unsigned int p_block_size);

	void post_edit_voxel(Vector3i pos);
	void post_edit_area(Box3i box_in_voxels);

	void set_generate_collisions(bool enabled);
	bool get_generate_collisions() const { return _generate_collisions; }

	void set_collision_layer(int layer);
	int get_collision_layer() const;

	void set_collision_mask(int mask);
	int get_collision_mask() const;

	void set_collision_margin(float margin);
	float get_collision_margin() const;

	unsigned int get_max_view_distance() const;
	void set_max_view_distance(unsigned int distance_in_voxels);

	// TODO Make this obsolete with multi-viewers
	void set_viewer_path(NodePath path);
	NodePath get_viewer_path() const;

	void set_material(unsigned int id, Ref<Material> material);
	Ref<Material> get_material(unsigned int id) const;

	VoxelDataMap &get_storage() { return _data_map; }
	const VoxelDataMap &get_storage() const { return _data_map; }

	Ref<VoxelTool> get_voxel_tool();

	void set_run_stream_in_editor(bool enable);
	bool is_stream_running_in_editor() const;

	void set_bounds(Box3i box);
	Box3i get_bounds() const;

	void restart_stream() override;
	void remesh_all_blocks() override;

	// For convenience, this is actually stored in a particular type of mesher
	Ref<VoxelLibrary> get_voxel_library() const;

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

	struct BlockToSave {
		std::shared_ptr<VoxelBufferInternal> voxels;
		Vector3i position;
	};

protected:
	void _notification(int p_what);

private:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	void _process();
	void process_viewers();
	void process_received_data_blocks();
	void process_meshing();
	void apply_mesh_update(const VoxelServer::BlockMeshOutput &ob);

	void _on_stream_params_changed();
	void _set_block_size_po2(int p_block_size_po2);
	//void make_all_view_dirty();
	void start_updater();
	void stop_updater();
	void start_streamer();
	void stop_streamer();
	void reset_map();

	void view_data_block(Vector3i bpos);
	void view_mesh_block(Vector3i bpos, bool mesh_flag, bool collision_flag);
	void unview_data_block(Vector3i bpos);
	void unview_mesh_block(Vector3i bpos, bool mesh_flag, bool collision_flag);
	void unload_data_block(Vector3i bpos);
	void unload_mesh_block(Vector3i bpos);
	//void make_data_block_dirty(Vector3i bpos);
	void try_schedule_mesh_update(VoxelMeshBlock *block);
	void try_schedule_mesh_update_from_data(const Box3i &box_in_voxels);

	void save_all_modified_blocks(bool with_copy);
	void get_viewer_pos_and_direction(Vector3 &out_pos, Vector3 &out_direction) const;
	void send_block_data_requests();

	void emit_data_block_loaded(const VoxelDataBlock *block);
	void emit_data_block_unloaded(const VoxelDataBlock *block);

	bool try_get_paired_viewer_index(uint32_t id, size_t &out_i) const;

	static void _bind_methods();

	// Bindings
	Vector3 _b_voxel_to_data_block(Vector3 pos) const;
	Vector3 _b_data_block_to_voxel(Vector3 pos) const;
	//void _force_load_blocks_binding(Vector3 center, Vector3 extents) { force_load_blocks(center, extents); }
	void _b_save_modified_blocks();
	void _b_save_block(Vector3 p_block_pos);
	void _b_set_bounds(AABB aabb);
	AABB _b_get_bounds() const;
	Dictionary _b_get_statistics() const;

	uint32_t _volume_id = 0;
	VoxelServer::ReceptionBuffers _reception_buffers;

	struct PairedViewer {
		struct State {
			Vector3i local_position_voxels;
			Box3i data_box;
			Box3i mesh_box;
			int view_distance_voxels = 0;
			bool requires_collisions = false;
			bool requires_meshes = false;
		};
		uint32_t id;
		State state;
		State prev_state;
	};

	std::vector<PairedViewer> _paired_viewers;

	// Voxel storage
	VoxelDataMap _data_map;
	// Mesh storage
	VoxelMeshMap _mesh_map;

	// Area within which voxels can exist.
	// Note, these bounds might not be exactly represented. This volume is chunk-based, so the result will be
	// approximated to the closest chunk.
	Box3i _bounds_in_voxels;

	unsigned int _max_view_distance_voxels = 128;

	// TODO Terrains only need to handle the visible portion of voxels, which reduces the bounds blocks to handle.
	// Therefore, could a simple grid be better to use than a hashmap?

	struct LoadingBlock {
		VoxelRefCount viewers;
	};

	HashMap<Vector3i, LoadingBlock, Vector3iHasher> _loading_blocks;
	std::vector<Vector3i> _blocks_pending_load;
	std::vector<Vector3i> _blocks_pending_update;
	std::vector<BlockToSave> _blocks_to_save;

	Ref<VoxelStream> _stream;
	Ref<VoxelMesher> _mesher;
	Ref<VoxelGenerator> _generator;

	bool _generate_collisions = true;
	unsigned int _collision_layer = 1;
	unsigned int _collision_mask = 1;
	float _collision_margin = VoxelConstants::DEFAULT_COLLISION_MARGIN;
	bool _run_stream_in_editor = true;
	//bool _stream_enabled = false;

	Ref<Material> _materials[VoxelMesherBlocky::MAX_MATERIALS];

	Stats _stats;
};

#endif // VOXEL_TERRAIN_H
