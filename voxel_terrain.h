#ifndef VOXEL_TERRAIN_H
#define VOXEL_TERRAIN_H

#include "voxel_map.h"
#include "voxel_mesher.h"
#include "voxel_mesher_smooth.h"
#include "voxel_provider.h"
#include "zprofiling.h"
#include <scene/main/node.h>

// Infinite static terrain made of voxels.
// It is loaded around VoxelTerrainStreamers.
class VoxelTerrain : public Spatial /*, public IVoxelMapObserver*/ {
	GDCLASS(VoxelTerrain, Spatial)
public:
	VoxelTerrain();

	void set_provider(Ref<VoxelProvider> provider);
	Ref<VoxelProvider> get_provider() const;

	void set_voxel_library(Ref<VoxelLibrary> library);
	Ref<VoxelLibrary> get_voxel_library() const;

	void force_load_blocks(Vector3i center, Vector3i extents);
	int get_block_update_count();

	void make_block_dirty(Vector3i bpos);
	void make_blocks_dirty(Vector3i min, Vector3i size);
	void make_voxel_dirty(Vector3i pos);
	bool is_block_dirty(Vector3i bpos);

	void set_generate_collisions(bool enabled);
	bool get_generate_collisions() const { return _generate_collisions; }

	int get_view_distance() const;
	void set_view_distance(int distance_in_voxels);

	void set_viewer_path(NodePath path);
	NodePath get_viewer_path() const;

	void set_material(int id, Ref<Material> material);
	Ref<Material> get_material(int id) const;

	Ref<VoxelMesher> get_mesher() { return _mesher; }
	Ref<VoxelMap> get_map() { return _map; }

protected:
	void _notification(int p_what);

private:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	void _process();

	void update_blocks();
	void update_block_mesh(Vector3i block_pos);

	void make_all_view_dirty_deferred();

	Spatial *get_viewer(NodePath path) const;

	void immerge_block(Vector3i bpos);

	// Observer events
	//void block_removed(VoxelBlock & block);

	static void _bind_methods();

	// Convenience
	Vector3 _voxel_to_block_binding(Vector3 pos);
	Vector3 _block_to_voxel_binding(Vector3 pos);
	void _force_load_blocks_binding(Vector3 center, Vector3 extents) { force_load_blocks(center, extents); }
	void _make_block_dirty_binding(Vector3 bpos) { make_block_dirty(bpos); }
	void _make_blocks_dirty_binding(Vector3 min, Vector3 size) { make_blocks_dirty(min, size); }
	void _make_voxel_dirty_binding(Vector3 pos) { make_voxel_dirty(pos); }

	Variant _raycast_binding(Vector3 origin, Vector3 direction, real_t max_distance);

	void set_voxel(Vector3 pos, int value, int c);
	int get_voxel(Vector3 pos, int c);

private:
	// Voxel storage
	Ref<VoxelMap> _map;

	// How many blocks to load around the viewer
	int _view_distance_blocks;

	// TODO Terrains only need to handle the visible portion of voxels, which reduces the bounds blocks to handle.
	// Therefore, could a simple grid be better to use than a hashmap?

	Vector<Vector3i> _block_update_queue;
	HashMap<Vector3i, bool, Vector3iHasher> _dirty_blocks; // only the key is relevant

	Ref<VoxelMesher> _mesher;
	// TODO I'm not sure it will stay here... refactoring ahead
	Ref<VoxelMesherSmooth> _mesher_smooth;

	Ref<VoxelProvider> _provider;

	NodePath _viewer_path;
	Vector3i _last_viewer_block_pos;
	int _last_view_distance_blocks;

	bool _generate_collisions;

#ifdef VOXEL_PROFILING
	ZProfiler _zprofiler;
	Dictionary get_profiling_info() { return _zprofiler.get_all_serialized_info(); }
#endif
};

#endif // VOXEL_TERRAIN_H
