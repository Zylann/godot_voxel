#ifndef VOXEL_LOD_TERRAIN_HPP
#define VOXEL_LOD_TERRAIN_HPP

#include "lod_octree.h"
#include "voxel_data_loader.h"
#include "voxel_mesh_updater.h"
#include <core/set.h>
#include <scene/3d/spatial.h>

//#define VOXEL_DEBUG_BOXES

class VoxelMap;
class VoxelTool;
class VoxelStream;
class VoxelBlock;

// Paged terrain made of voxel blocks of variable level of detail.
// Designed for highest view distances, preferably using smooth voxels.
// Voxels are polygonized around the viewer by distance in a very large sphere, usually extending beyond far clip.
// Data is streamed using a VoxelStream, which must support LOD.
class VoxelLodTerrain : public Spatial {
	GDCLASS(VoxelLodTerrain, Spatial)
public:
	// TODO Put this in a constant outside, I had to re-declare it in various places
	static const int MAX_LOD = 32;

	VoxelLodTerrain();
	~VoxelLodTerrain();

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

	void set_viewer_path(NodePath path);
	NodePath get_viewer_path() const;

	int get_block_region_extent() const;
	Dictionary get_block_info(Vector3 fbpos, int lod_index) const;
	Vector3 voxel_to_block_position(Vector3 vpos, int lod_index) const;

	unsigned int get_block_size_pow2() const;
	void set_block_size_po2(unsigned int p_block_size_po2);

	// These must be called after an edit
	void post_edit_area(Rect3i p_box);
	void post_edit_block_lod0(Vector3i bpos);

	Ref<VoxelTool> get_voxel_tool();

	struct Stats {
		VoxelMeshUpdater::Stats updater;
		VoxelDataLoader::Stats stream;
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

protected:
	static void _bind_methods();

	void _notification(int p_what);
	void _process();

private:
	unsigned int get_block_size() const;
	Spatial *get_viewer() const;
	void immerge_block(Vector3i block_pos, int lod_index);

	void start_updater();
	void stop_updater();
	void start_streamer();
	void stop_streamer();
	void reset_maps();

	void get_viewer_pos_and_direction(Vector3 &out_viewer_pos, Vector3 &out_direction) const;
	void try_schedule_loading_with_neighbors(const Vector3i &p_bpos, int lod_index);
	bool check_block_loaded_and_updated(const Vector3i &p_bpos, int lod_index);
	bool check_block_loaded_and_updated(VoxelBlock *block);
	void _set_lod_count(int p_lod_count);
	void _set_block_size_po2(int p_block_size_po2);

	void _on_stream_params_changed();

	void flush_pending_lod_edits();
	void save_all_modified_blocks(bool with_copy);
	void send_block_data_requests();

	struct OctreeItem {
		LodOctree<bool> octree;
#ifdef VOXEL_DEBUG_BOXES
		Spatial *debug_box = nullptr;
#endif
	};

#ifdef VOXEL_DEBUG_BOXES
	void create_octree_debug_box(OctreeItem &item, Vector3i pos);
	void destroy_octree_debug_box(OctreeItem &item, Vector3i pos);
#endif

	// This terrain type is a sparse grid of octrees.
	// Indexed by a grid coordinate whose step is the size of the highest-LOD block
	// This octree doesn't hold any data... hence bool.
	Map<Vector3i, OctreeItem> _lod_octrees;
	Rect3i _last_octree_region_box;

	NodePath _viewer_path;

	Ref<VoxelStream> _stream;
	VoxelDataLoader *_stream_thread = nullptr;
	VoxelMeshUpdater *_block_updater = nullptr;
	std::vector<VoxelMeshUpdater::OutputBlock> _blocks_pending_main_thread_update;
	std::vector<VoxelDataLoader::InputBlock> _blocks_to_save;

	Ref<Material> _material;

	bool _generate_collisions = true;
	int _collision_lod_count = -1;

	// Each LOD works in a set of coordinates spanning 2x more voxels the higher their index is
	struct Lod {
		Ref<VoxelMap> map;
		Set<Vector3i> loading_blocks;
		std::vector<Vector3i> blocks_pending_update;

		// Blocks that were edited and need their LOD counterparts to be updated
		std::vector<Vector3i> blocks_pending_lodding;

		// These are relative to this LOD, in block coordinates
		Vector3i last_viewer_block_pos;
		int last_view_distance_blocks = 0;

		// Members for memory caching
		std::vector<Vector3i> blocks_to_load;

#ifdef TOOLS_ENABLED
		// TODO Debug, may be removed in the future
		HashMap<Vector3i, int, Vector3iHasher> debug_unexpected_load_drop_time;
#endif
	};

	Lod _lods[MAX_LOD];
	int _lod_count = 0;
	float _lod_split_scale = 0.f;
	unsigned int _view_distance_voxels = 512;

	Stats _stats;
};

#endif // VOXEL_LOD_TERRAIN_HPP
