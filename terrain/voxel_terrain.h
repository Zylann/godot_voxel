#ifndef VOXEL_TERRAIN_H
#define VOXEL_TERRAIN_H

#include "../math/rect3i.h"
#include "../math/vector3i.h"
#include "../util/zprofiling.h"
#include "voxel_data_loader.h"
#include "voxel_mesh_updater.h"

#include <scene/3d/spatial.h>

class VoxelMap;
class VoxelLibrary;
class VoxelStream;
class VoxelTool;

// Infinite paged terrain made of voxel blocks all with the same level of detail.
// Voxels are polygonized around the viewer by distance in a large cubic space.
// Data is streamed using a VoxelStream.
class VoxelTerrain : public Spatial {
	GDCLASS(VoxelTerrain, Spatial)
public:
	VoxelTerrain();
	~VoxelTerrain();

	void set_stream(Ref<VoxelStream> p_stream);
	Ref<VoxelStream> get_stream() const;

	unsigned int get_block_size_pow2() const;
	void set_block_size_po2(unsigned int p_block_size_po2);

	void set_voxel_library(Ref<VoxelLibrary> library);
	Ref<VoxelLibrary> get_voxel_library() const;

	void make_block_dirty(Vector3i bpos);
	//void make_blocks_dirty(Vector3i min, Vector3i size);
	void make_voxel_dirty(Vector3i pos);
	void make_area_dirty(Rect3i box);

	void set_generate_collisions(bool enabled);
	bool get_generate_collisions() const { return _generate_collisions; }

	int get_view_distance() const;
	void set_view_distance(int distance_in_voxels);

	void set_viewer_path(NodePath path);
	NodePath get_viewer_path() const;

	void set_material(unsigned int id, Ref<Material> material);
	Ref<Material> get_material(unsigned int id) const;

	bool is_smooth_meshing_enabled() const;
	void set_smooth_meshing_enabled(bool enabled);

	Ref<VoxelMap> get_storage() { return _map; }
	Ref<VoxelTool> get_voxel_tool();

	struct Stats {
		VoxelMeshUpdater::Stats updater;
		VoxelDataLoader::Stats stream;
		int updated_blocks = 0;
		int dropped_block_loads = 0;
		int dropped_block_meshs = 0;
		uint64_t time_detect_required_blocks = 0;
		uint64_t time_request_blocks_to_load = 0;
		uint64_t time_process_load_responses = 0;
		uint64_t time_request_blocks_to_update = 0;
		uint64_t time_process_update_responses = 0;
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
	void make_all_view_dirty_deferred();
	void start_updater();
	void stop_updater();
	void start_streamer();
	void stop_streamer();
	void reset_map();

	Spatial *get_viewer() const;

	void immerge_block(Vector3i bpos);
	void save_all_modified_blocks(bool with_copy);
	void get_viewer_pos_and_direction(Vector3 &out_pos, Vector3 &out_direction) const;
	void send_block_data_requests();

	Dictionary get_statistics() const;

	static void _bind_methods();

	// Bindings
	Vector3 _b_voxel_to_block(Vector3 pos);
	Vector3 _b_block_to_voxel(Vector3 pos);
	//void _force_load_blocks_binding(Vector3 center, Vector3 extents) { force_load_blocks(center, extents); }

private:
	// Voxel storage
	Ref<VoxelMap> _map;

	// How many blocks to load around the viewer
	int _view_distance_blocks;

	// TODO Terrains only need to handle the visible portion of voxels, which reduces the bounds blocks to handle.
	// Therefore, could a simple grid be better to use than a hashmap?

	Set<Vector3i> _loading_blocks;
	Vector<Vector3i> _blocks_pending_load;
	Vector<Vector3i> _blocks_pending_update;
	Vector<VoxelMeshUpdater::OutputBlock> _blocks_pending_main_thread_update;

	std::vector<VoxelDataLoader::InputBlock> _blocks_to_save;

	Ref<VoxelStream> _stream;
	VoxelDataLoader *_stream_thread;

	Ref<VoxelLibrary> _library;
	VoxelMeshUpdater *_block_updater;

	NodePath _viewer_path;
	Vector3i _last_viewer_block_pos;
	int _last_view_distance_blocks;

	bool _generate_collisions = true;
	bool _run_in_editor;
	bool _smooth_meshing_enabled;

	Ref<Material> _materials[VoxelMesherBlocky::MAX_MATERIALS];

	Stats _stats;
};

#endif // VOXEL_TERRAIN_H
