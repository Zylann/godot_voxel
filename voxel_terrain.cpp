#include "voxel_terrain.h"
#include "voxel_raycast.h"
#include "rect3i.h"
#include <os/os.h>
#include <scene/3d/mesh_instance.h>
#include <engine.h>


VoxelTerrain::VoxelTerrain()
	: Spatial(), _generate_collisions(true) {

	_map = Ref<VoxelMap>(memnew(VoxelMap));
	_mesher = Ref<VoxelMesher>(memnew(VoxelMesher));
	_mesher_smooth = Ref<VoxelMesherSmooth>(memnew(VoxelMesherSmooth));

	_view_distance_blocks = 8;
	_last_view_distance_blocks = 0;
}

// TODO UGLY! Lambdas or pointers needed... DO NOT use this outside of lambdas!
Vector3i g_viewer_block_pos;

// Sorts distance to viewer
struct BlockUpdateComparator {
	inline bool operator()(const Vector3i &a, const Vector3i &b) const {
		return a.distance_sq(g_viewer_block_pos) > b.distance_sq(g_viewer_block_pos);
	}
};

// TODO See if there is a way to specify materials in voxels directly?

bool VoxelTerrain::_set(const StringName &p_name, const Variant &p_value) {

	if (p_name.operator String().begins_with("material/")) {
		int idx = p_name.operator String().get_slicec('/', 1).to_int();
		if (idx >= VoxelMesher::MAX_MATERIALS || idx < 0)
			return false;
		set_material(idx, p_value);
		return true;
	}

	return false;
}

bool VoxelTerrain::_get(const StringName &p_name, Variant &r_ret) const {

	if (p_name.operator String().begins_with("material/")) {
		int idx = p_name.operator String().get_slicec('/', 1).to_int();
		if (idx >= VoxelMesher::MAX_MATERIALS || idx < 0)
			return false;
		r_ret = get_material(idx);
		return true;
	}

	return false;
}

void VoxelTerrain::_get_property_list(List<PropertyInfo> *p_list) const {

	for (int i = 0; i < VoxelMesher::MAX_MATERIALS; ++i) {
		p_list->push_back(PropertyInfo(Variant::OBJECT, "material/" + itos(i), PROPERTY_HINT_RESOURCE_TYPE, "ShaderMaterial,SpatialMaterial"));
	}
}

void VoxelTerrain::set_provider(Ref<VoxelProvider> provider) {
	if(provider != _provider) {
		_provider = provider;
		// The whole map might change, so make all area dirty
		make_all_view_dirty_deferred();
	}
}

Ref<VoxelProvider> VoxelTerrain::get_provider() const {
	return _provider;
}

Ref<VoxelLibrary> VoxelTerrain::get_voxel_library() const {
	return _mesher->get_library();
}

void VoxelTerrain::set_voxel_library(Ref<VoxelLibrary> library) {
	if(library != _mesher->get_library()) {

#ifdef TOOLS_ENABLED
		if(library->get_voxel_count() == 0) {
			library->load_default();
		}
#endif
		_mesher->set_library(library);

		// Voxel appearance might completely change
		make_all_view_dirty_deferred();
	}
}

void VoxelTerrain::set_generate_collisions(bool enabled) {
	_generate_collisions = enabled;
}

int VoxelTerrain::get_view_distance() const {
	return _view_distance_blocks * _map->get_block_size();
}

void VoxelTerrain::set_view_distance(int distance_in_voxels) {
	ERR_FAIL_COND(distance_in_voxels < 0)
	int d = distance_in_voxels / _map->get_block_size();
	if(d != _view_distance_blocks) {
		print_line(String("View distance changed from ") + String::num(_view_distance_blocks) + String(" blocks to ") + String::num(d));
		_view_distance_blocks = d;
		// Blocks too far away will be removed in _process, same for blocks to load
	}
}

void VoxelTerrain::set_viewer_path(NodePath path) {
	_viewer_path = path;
}

NodePath VoxelTerrain::get_viewer_path() const {
	return _viewer_path;
}

Spatial *VoxelTerrain::get_viewer(NodePath path) const {
	if (path.is_empty())
		return NULL;
	Node *node = get_node(path);
	if (node == NULL)
		return NULL;
	return Object::cast_to<Spatial>(node);
}

void VoxelTerrain::set_material(int id, Ref<Material> material) {
	// TODO Update existing block surfaces
	_mesher->set_material(material, id);
}

Ref<Material> VoxelTerrain::get_material(int id) const {
	return _mesher->get_material(id);
}

//void VoxelTerrain::clear_update_queue() {
//	_block_update_queue.clear();
//	_dirty_blocks.clear();
//}

void VoxelTerrain::make_block_dirty(Vector3i bpos) {
	// TODO Immediate update viewer distance
	if (is_block_dirty(bpos) == false) {
		//OS::get_singleton()->print("Dirty (%i, %i, %i)", bpos.x, bpos.y, bpos.z);
		_block_update_queue.push_back(bpos);
		_dirty_blocks[bpos] = true;
	}
}

void VoxelTerrain::immerge_block(Vector3i bpos) {

	ERR_FAIL_COND(_map.is_null());

	// TODO Schedule block saving when supported
	_map->remove_block(bpos, VoxelMap::NoAction());

	_dirty_blocks.erase(bpos);
	// Blocks in the update queue will be cancelled in _process,
	// because it's too expensive to linear-search all blocks for each block
}

bool VoxelTerrain::is_block_dirty(Vector3i bpos) {
	return _dirty_blocks.has(bpos);
}

void VoxelTerrain::make_blocks_dirty(Vector3i min, Vector3i size) {
	Vector3i max = min + size;
	Vector3i pos;
	for (pos.z = min.z; pos.z < max.z; ++pos.z) {
		for (pos.y = min.y; pos.y < max.y; ++pos.y) {
			for (pos.x = min.x; pos.x < max.x; ++pos.x) {
				make_block_dirty(pos);
			}
		}
	}
}

void VoxelTerrain::make_all_view_dirty_deferred() {
	// This trick will regenerate all chunks in view, according to the view distance found during block updates.
	// The point of doing this instead of immediately scheduling updates is that it will
	// always use an up-to-date view distance, which is not necessarily loaded yet on initialization.
	_last_view_distance_blocks = 0;

//	Vector3i radius(_view_distance_blocks, _view_distance_blocks, _view_distance_blocks);
//	make_blocks_dirty(-radius, 2*radius);
}

inline int get_border_index(int x, int max) {
	return x == 0 ? 0 : x != max ? 1 : 2;
}

void VoxelTerrain::make_voxel_dirty(Vector3i pos) {

	// Update the block in which the voxel is
	Vector3i bpos = _map->voxel_to_block(pos);
	make_block_dirty(bpos);
	//OS::get_singleton()->print("Dirty (%i, %i, %i)\n", bpos.x, bpos.y, bpos.z);

	// Update neighbor blocks if the voxel is touching a boundary

	Vector3i rpos = _map->to_local(pos);

	bool check_corners = _mesher->get_occlusion_enabled();

	const int max = _map->get_block_size() - 1;

	if (rpos.x == 0)
		make_block_dirty(bpos - Vector3i(1, 0, 0));
	else if (rpos.x == max)
		make_block_dirty(bpos + Vector3i(1, 0, 0));

	if (rpos.y == 0)
		make_block_dirty(bpos - Vector3i(0, 1, 0));
	else if (rpos.y == max)
		make_block_dirty(bpos + Vector3i(0, 1, 0));

	if (rpos.z == 0)
		make_block_dirty(bpos - Vector3i(0, 0, 1));
	else if (rpos.z == max)
		make_block_dirty(bpos + Vector3i(0, 0, 1));

	// We might want to update blocks in corners in order to update ambient occlusion
	if (check_corners) {

		//       24------25------26
		//       /|              /|
		//      / |             / |
		//    21  |           23  |
		//    /  15           /  17
		//   /    |          /    |
		// 18------19------20     |
		//  |     |         |     |
		//  |     6-------7-|-----8
		//  |    /          |    /
		//  9   /          11   /
		//  |  3            |  5
		//  | /             | /      y z
		//  |/              |/       |/
		//  0-------1-------2        o--x

		// I'm not good at writing piles of ifs

		static const int normals[27][3] = {
			{ -1, -1, -1 }, { 0, -1, -1 }, { 1, -1, -1 },
			{ -1, -1, 0 }, { 0, -1, 0 }, { 1, -1, 0 },
			{ -1, -1, 1 }, { 0, -1, 1 }, { 1, -1, 1 },

			{ -1, 0, -1 }, { 0, 0, -1 }, { 1, 0, -1 },
			{ -1, 0, 0 }, { 0, 0, 0 }, { 1, 0, 0 },
			{ -1, 0, 1 }, { 0, 0, 1 }, { 1, 0, 1 },

			{ -1, 1, -1 }, { 0, 1, -1 }, { 1, 1, -1 },
			{ -1, 1, 0 }, { 0, 1, 0 }, { 1, 1, 0 },
			{ -1, 1, 1 }, { 0, 1, 1 }, { 1, 1, 1 }
		};
		static const int ce_counts[27] = {
			4, 1, 4,
			1, 0, 1,
			4, 1, 4,

			1, 0, 1,
			0, 0, 0,
			1, 0, 1,

			4, 1, 4,
			1, 0, 1,
			4, 1, 4
		};
		static const int ce_indexes_lut[27][4] = {
			{ 0, 1, 3, 9 }, { 1 }, { 2, 1, 5, 11 },
			{ 3 }, {}, { 5 },
			{ 6, 3, 7, 15 }, { 7 }, { 8, 7, 5, 17 },

			{ 9 }, {}, { 11 },
			{}, {}, {},
			{ 15 }, {}, { 17 },

			{ 18, 9, 19, 21 }, { 19 }, { 20, 11, 19, 23 },
			{ 21 }, {}, { 23 },
			{ 24, 15, 21, 25 }, { 25 }, { 26, 17, 23, 25 }
		};

		int m = get_border_index(rpos.x, max) + 3 * get_border_index(rpos.z, max) + 9 * get_border_index(rpos.y, max);

		const int *ce_indexes = ce_indexes_lut[m];
		int ce_count = ce_counts[m];
		//OS::get_singleton()->print("m=%i, rpos=(%i, %i, %i)\n", m, rpos.x, rpos.y, rpos.z);

		for (int i = 0; i < ce_count; ++i) {
			// TODO Because it's about ambient occlusion across 1 voxel only,
			// we could optimize it even more by looking at neighbor voxels,
			// and discard the update if we know it won't change anything
			const int *normal = normals[ce_indexes[i]];
			Vector3i nbpos(bpos.x + normal[0], bpos.y + normal[1], bpos.z + normal[2]);
			//OS::get_singleton()->print("Corner dirty (%i, %i, %i)\n", nbpos.x, nbpos.y, nbpos.z);
			make_block_dirty(nbpos);
		}
	}
}

int VoxelTerrain::get_block_update_count() {
	return _block_update_queue.size();
}

struct EnterWorldAction {
	World *world;
	EnterWorldAction(World *w) : world(w) {}
	void operator()(VoxelBlock *block) {
		block->enter_world(world);
	}
};

struct ExitWorldAction {
	void operator()(VoxelBlock *block) {
		block->exit_world();
	}
};

struct SetVisibilityAction {
	bool visible;
	SetVisibilityAction(bool v) : visible(v) {}
	void operator()(VoxelBlock *block) {
		block->set_visible(visible);
	}
};

void VoxelTerrain::_notification(int p_what) {

	switch (p_what) {

		case NOTIFICATION_ENTER_TREE:
			set_process(true);
			break;

		case NOTIFICATION_PROCESS:
			_process();
			break;

		case NOTIFICATION_EXIT_TREE:
			break;

		case NOTIFICATION_ENTER_WORLD: {
			ERR_FAIL_COND(_map.is_null());
			_map->for_all_blocks(EnterWorldAction(*get_world()));
		} break;

		case NOTIFICATION_EXIT_WORLD:
			ERR_FAIL_COND(_map.is_null());
			_map->for_all_blocks(ExitWorldAction());
			break;

		case NOTIFICATION_VISIBILITY_CHANGED:
			ERR_FAIL_COND(_map.is_null());
			_map->for_all_blocks(SetVisibilityAction(is_visible()));
			break;

		// TODO Listen for transform changes

		default:
			break;
	}
}

void VoxelTerrain::_process() {
	update_blocks();
}

void VoxelTerrain::update_blocks() {

	OS &os = *OS::get_singleton();
	Engine &engine = *Engine::get_singleton();

	ERR_FAIL_COND(_map.is_null());

	// Get viewer location
	// TODO Transform to local (Spatial Transform)
	Vector3i viewer_block_pos;
	if(engine.is_editor_hint()) {
		// TODO Use editor's camera here
		viewer_block_pos = Vector3i();
	} else {
		Spatial *viewer = get_viewer(_viewer_path);
		if (viewer)
			viewer_block_pos = _map->voxel_to_block(viewer->get_translation());
		else
			viewer_block_pos = Vector3i();
	}

	{
		// Find out which blocks need to appear and which need to be unloaded

		//Vector3i viewer_block_pos_delta = _last_viewer_block_pos - viewer_block_pos;
		Rect3i new_box = Rect3i::from_center_extents(viewer_block_pos, Vector3i(_view_distance_blocks));
		Rect3i prev_box = Rect3i::from_center_extents(_last_viewer_block_pos, Vector3i(_last_view_distance_blocks));

		if(prev_box != new_box) {
			//print_line(String("Loaded area changed: from ") + prev_box.to_string() + String(" to ") + new_box.to_string());

			Rect3i bounds = Rect3i::get_bounding_box(prev_box, new_box);
			Vector3i max = bounds.pos + bounds.size;

			// TODO There should be a way to only iterate relevant blocks
			Vector3i pos;
			for(pos.z = bounds.pos.z; pos.z < max.z; ++pos.z) {
				for(pos.y = bounds.pos.y; pos.y < max.y; ++pos.y) {
					for(pos.x = bounds.pos.x; pos.x < max.x; ++pos.x) {

						bool prev_contains = prev_box.contains(pos);
						bool new_contains = new_box.contains(pos);

						if(prev_contains && !new_contains) {
							// Unload block
							immerge_block(pos);

						} else if(!prev_contains && new_contains) {
							// Load or update block
							make_block_dirty(pos);
						}
					}
				}
			}
		}

		// Eliminate blocks in queue that aren't needed
		for(int i = 0; i < _block_update_queue.size(); ++i) {
			const Vector3i bpos = _block_update_queue[i];
			if(!new_box.contains(bpos)) {
				int last = _block_update_queue.size() - 1;
				_block_update_queue.write[i] = _block_update_queue[last];
				_block_update_queue.resize(last);
				--i;
			}
		}
	}

	_last_view_distance_blocks = _view_distance_blocks;
	_last_viewer_block_pos = viewer_block_pos;

	// Sort updates so nearest blocks are done first
	VOXEL_PROFILE_BEGIN("block_update_sorting")
	g_viewer_block_pos = viewer_block_pos;
	_block_update_queue.sort_custom<BlockUpdateComparator>();
	VOXEL_PROFILE_END("block_update_sorting")

	uint32_t time_before = os.get_ticks_msec();
	uint32_t max_time = 1000 / 120;

	const unsigned int bs = _map->get_block_size();
	const Vector3i block_size(bs, bs, bs);

	// Update a bunch of blocks until none are left or too much time elapsed
	while (!_block_update_queue.empty() && (os.get_ticks_msec() - time_before) < max_time) {

		//printf("Remaining: %i\n", _block_update_queue.size());

		// TODO Move this to a thread

		// Get request
		Vector3i block_pos = _block_update_queue[_block_update_queue.size() - 1];

		bool entire_block_changed = false;

		if (!_map->has_block(block_pos)) {
			// The block's data isn't loaded yet
			// Create buffer
			if (!_provider.is_null()) {

				VOXEL_PROFILE_BEGIN("voxel_buffer_creation_gen")

				Ref<VoxelBuffer> buffer_ref = Ref<VoxelBuffer>(memnew(VoxelBuffer));
				buffer_ref->create(block_size.x, block_size.y, block_size.z);

				VOXEL_PROFILE_END("voxel_buffer_creation_gen")
				VOXEL_PROFILE_BEGIN("block_generation")

				// Query voxel provider
				_provider->emerge_block(buffer_ref, _map->block_to_voxel(block_pos));

				// Check script return
				// TODO Shouldn't halt execution though, as it can bring the map in an invalid state!
				ERR_FAIL_COND(buffer_ref->get_size() != block_size);

				VOXEL_PROFILE_END("block_generation")

				// Store buffer
				_map->set_block_buffer(block_pos, buffer_ref);

				entire_block_changed = true;
			}
		}

		// Update views (mesh/collisions)

		if (entire_block_changed) {
			// All neighbors have to be checked
			Vector3i ndir;
			for (ndir.z = -1; ndir.z < 2; ++ndir.z) {
				for (ndir.x = -1; ndir.x < 2; ++ndir.x) {
					for (ndir.y = -1; ndir.y < 2; ++ndir.y) {
						Vector3i npos = block_pos + ndir;
						// TODO What if the map is really composed of empty blocks?
						if (_map->is_block_surrounded(npos)) {
							update_block_mesh(npos);
						}
					}
				}
			}
		} else {
			// Only update the block, neighbors will probably follow if needed
			update_block_mesh(block_pos);
			//OS::get_singleton()->print("Update (%i, %i, %i)\n", block_pos.x, block_pos.y, block_pos.z);
		}

		// Pop request
		_block_update_queue.resize(_block_update_queue.size() - 1);
		_dirty_blocks.erase(block_pos);
	}

	//print_line(String("d:") + String::num(_dirty_blocks.size()) + String(", q:") + String::num(_block_update_queue.size()));
}

static inline bool is_mesh_empty(Ref<Mesh> mesh_ref) {
	if (mesh_ref.is_null())
		return true;
	const Mesh &mesh = **mesh_ref;
	if (mesh.get_surface_count() == 0)
		return true;
	if (mesh.surface_get_array_len(0) == 0)
		return true;
	return false;
}

void VoxelTerrain::update_block_mesh(Vector3i block_pos) {

	VoxelBlock *block = _map->get_block(block_pos);
	if (block == NULL) {
		return;
	}

	VOXEL_PROFILE_BEGIN("voxel_buffer_creation_extract")
	// Create buffer padded with neighbor voxels
	VoxelBuffer nbuffer;
	// TODO Make the buffer re-usable
	// TODO Padding set to 3 at the moment because Transvoxel works on 2x2 cells.
	// It should change for a smarter padding (if smooth isn't used for example).
	unsigned int block_size = _map->get_block_size();
	nbuffer.create(block_size + 3, block_size + 3, block_size + 3);
	VOXEL_PROFILE_END("voxel_buffer_creation_extract")

	VOXEL_PROFILE_BEGIN("block_extraction")
	_map->get_buffer_copy(_map->block_to_voxel(block_pos) - Vector3i(1, 1, 1), nbuffer, 0x3);
	VOXEL_PROFILE_END("block_extraction")

	// TODO Re-use existing meshes to optimize memory cost

	// Build cubic parts of the mesh
	Ref<ArrayMesh> mesh = _mesher->build(nbuffer, Voxel::CHANNEL_TYPE, Vector3i(0, 0, 0), nbuffer.get_size() - Vector3(1, 1, 1));
	// Build smooth parts of the mesh
	_mesher_smooth->build(nbuffer, Voxel::CHANNEL_ISOLEVEL, mesh);

	if(is_mesh_empty(mesh))
		mesh = Ref<Mesh>();

	Ref<World> world = get_world();
	block->set_mesh(mesh, world);
}

//void VoxelTerrain::block_removed(VoxelBlock & block) {
//    MeshInstance * mesh_instance = block.get_mesh_instance(*this);
//    if (mesh_instance) {
//        mesh_instance->queue_delete();
//    }
//}

struct _VoxelTerrainRaycastContext {
	VoxelTerrain &terrain;
	//unsigned int channel_mask;
};

static bool _raycast_binding_predicate(Vector3i pos, void *context_ptr) {

	ERR_FAIL_COND_V(context_ptr == NULL, false);
	_VoxelTerrainRaycastContext *context = (_VoxelTerrainRaycastContext *)context_ptr;
	VoxelTerrain &terrain = context->terrain;

	//unsigned int channel = context->channel;

	Ref<VoxelMap> map = terrain.get_map();
	int v0 = map->get_voxel(pos, Voxel::CHANNEL_TYPE);

	Ref<VoxelLibrary> lib_ref = terrain.get_voxel_library();
	if (lib_ref.is_null())
		return false;
	const VoxelLibrary &lib = **lib_ref;

	if (lib.has_voxel(v0) == false)
		return false;

	const Voxel &voxel = lib.get_voxel_const(v0);
	if (voxel.is_transparent() == false)
		return true;

	int v1 = map->get_voxel(pos, Voxel::CHANNEL_ISOLEVEL);
	return v1 - 128 >= 0;
}

Variant VoxelTerrain::_raycast_binding(Vector3 origin, Vector3 direction, real_t max_distance) {

	// TODO Transform input if the terrain is rotated (in the future it can be made a Spatial node)

	Vector3i hit_pos;
	Vector3i prev_pos;

	_VoxelTerrainRaycastContext context = { *this };

	if (voxel_raycast(origin, direction, _raycast_binding_predicate, &context, max_distance, hit_pos, prev_pos)) {

		Dictionary hit = Dictionary();
		hit["position"] = hit_pos.to_vec3();
		hit["prev_position"] = prev_pos.to_vec3();
		return hit;
	} else {
		return Variant(); // Null dictionary, no alloc
	}
}

Vector3 VoxelTerrain::_voxel_to_block_binding(Vector3 pos) {
	return Vector3i(_map->voxel_to_block(pos)).to_vec3();
}

Vector3 VoxelTerrain::_block_to_voxel_binding(Vector3 pos) {
	return Vector3i(_map->block_to_voxel(pos)).to_vec3();
}

void VoxelTerrain::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_provider", "provider"), &VoxelTerrain::set_provider);
	ClassDB::bind_method(D_METHOD("get_provider"), &VoxelTerrain::get_provider);

	ClassDB::bind_method(D_METHOD("set_voxel_library", "library"), &VoxelTerrain::set_voxel_library);
	ClassDB::bind_method(D_METHOD("get_voxel_library"), &VoxelTerrain::get_voxel_library);

	ClassDB::bind_method(D_METHOD("set_view_distance", "distance_in_voxels"), &VoxelTerrain::set_view_distance);
	ClassDB::bind_method(D_METHOD("get_view_distance"), &VoxelTerrain::get_view_distance);

	ClassDB::bind_method(D_METHOD("get_block_update_count"), &VoxelTerrain::get_block_update_count);
	ClassDB::bind_method(D_METHOD("get_mesher"), &VoxelTerrain::get_mesher);

	ClassDB::bind_method(D_METHOD("get_generate_collisions"), &VoxelTerrain::get_generate_collisions);
	ClassDB::bind_method(D_METHOD("set_generate_collisions", "enabled"), &VoxelTerrain::set_generate_collisions);

	ClassDB::bind_method(D_METHOD("get_viewer_path"), &VoxelTerrain::get_viewer_path);
	ClassDB::bind_method(D_METHOD("set_viewer_path", "path"), &VoxelTerrain::set_viewer_path);

	ClassDB::bind_method(D_METHOD("get_storage"), &VoxelTerrain::get_map);

	ClassDB::bind_method(D_METHOD("voxel_to_block", "voxel_pos"), &VoxelTerrain::_voxel_to_block_binding);
	ClassDB::bind_method(D_METHOD("block_to_voxel", "block_pos"), &VoxelTerrain::_block_to_voxel_binding);

	ClassDB::bind_method(D_METHOD("make_block_dirty", "pos"), &VoxelTerrain::_make_block_dirty_binding);
	ClassDB::bind_method(D_METHOD("make_blocks_dirty", "min", "size"), &VoxelTerrain::_make_blocks_dirty_binding);
	ClassDB::bind_method(D_METHOD("make_voxel_dirty", "pos"), &VoxelTerrain::_make_voxel_dirty_binding);

	ClassDB::bind_method(D_METHOD("raycast", "origin", "direction", "max_distance"), &VoxelTerrain::_raycast_binding, DEFVAL(100));

#ifdef VOXEL_PROFILING
	ClassDB::bind_method(D_METHOD("get_profiling_info"), &VoxelTerrain::get_profiling_info);
#endif

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "provider", PROPERTY_HINT_RESOURCE_TYPE, "VoxelProvider"), "set_provider", "get_provider");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "voxel_library", PROPERTY_HINT_RESOURCE_TYPE, "VoxelLibrary"), "set_voxel_library", "get_voxel_library");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "view_distance"), "set_view_distance", "get_view_distance");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "viewer_path"), "set_viewer_path", "get_viewer_path");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generate_collisions"), "set_generate_collisions", "get_generate_collisions");
}
