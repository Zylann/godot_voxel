#ifndef VOXEL_LOD_TERRAIN_HPP
#define VOXEL_LOD_TERRAIN_HPP

#include "../streams/voxel_stream.h"
#include "lod_octree.h"
#include "voxel_data_loader.h"
#include "voxel_map.h"
#include "voxel_mesh_updater.h"
#include <core/set.h>
#include <scene/3d/spatial.h>

class VoxelMap;

// Paged terrain made of voxel blocks of variable level of detail.
// Designed for highest view distances, preferably using smooth voxels.
// Voxels are polygonized around the viewer by distance in a very large sphere, usually extending beyond far clip.
// Data is streamed using a VoxelStream, which must support LOD.
class VoxelLodTerrain : public Spatial {
	GDCLASS(VoxelLodTerrain, Spatial)
public:
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

	void set_lod_count(unsigned int p_lod_count);
	unsigned int get_lod_count() const;

	void set_viewer_path(NodePath path);
	NodePath get_viewer_path() const;

	int get_block_region_extent() const;
	Dictionary get_block_info(Vector3 fbpos, unsigned int lod_index) const;
	Vector3 voxel_to_block_position(Vector3 vpos, unsigned int lod_index) const;

	Ref<VoxelMap> get_map() { return _lods[0].map; }

	struct Stats {
		VoxelMeshUpdater::Stats updater;
		VoxelDataLoader::Stats stream;
		uint64_t time_request_blocks_to_load = 0;
		uint64_t time_process_load_responses = 0;
		uint64_t time_request_blocks_to_update = 0;
		uint64_t time_process_update_responses = 0;
		uint64_t time_process_lod = 0;
		int blocked_lods = 0;
		int dropped_block_loads = 0;
		int dropped_block_meshs = 0;
	};

	Dictionary get_stats() const;

protected:
	static void _bind_methods();

	void _notification(int p_what);
	void _process();

private:
	unsigned int get_block_size() const;
	unsigned int get_block_size_pow2() const;
	void make_all_view_dirty_deferred();
	Spatial *get_viewer() const;
	void immerge_block(Vector3i block_pos, unsigned int lod_index);
	void reset_updater();
	Vector3 get_viewer_pos(Vector3 &out_direction) const;
	void try_schedule_loading_with_neighbors(const Vector3i &p_bpos, unsigned int lod_index);
	bool check_block_loaded_and_updated(const Vector3i &p_bpos, unsigned int lod_index);
	Variant _raycast_binding(Vector3 origin, Vector3 direction, real_t max_distance);

	template <typename A>
	void for_all_blocks(A &action) {
		for (int lod_index = 0; lod_index < MAX_LOD; ++lod_index) {
			if (_lods[lod_index].map.is_valid()) {
				_lods[lod_index].map->for_all_blocks(action);
			}
		}
	}

	// TODO Dare having a grid of octrees for infinite world?
	// This octree doesn't hold any data... hence bool.
	LodOctree<bool> _lod_octree;

	NodePath _viewer_path;

	Ref<VoxelStream> _stream;
	VoxelDataLoader *_stream_thread = nullptr;
	VoxelMeshUpdater *_block_updater = nullptr;
	std::vector<VoxelMeshUpdater::OutputBlock> _blocks_pending_main_thread_update;

	Ref<Material> _material;

	// Each LOD works in a set of coordinates spanning 2x more voxels the higher their index is
	struct Lod {
		Ref<VoxelMap> map;
		Set<Vector3i> loading_blocks;
		std::vector<Vector3i> blocks_pending_update;

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

	Stats _stats;
};

#endif // VOXEL_LOD_TERRAIN_HPP
