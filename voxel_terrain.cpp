#include "voxel_terrain.h"
#include <scene/3d/mesh_instance.h>
#include <os/os.h>
#include "voxel_raycast.h"

VoxelTerrain::VoxelTerrain(): Node(), _generate_collisions(true) {

	_map = Ref<VoxelMap>(memnew(VoxelMap));
	_mesher = Ref<VoxelMesher>(memnew(VoxelMesher));
	_mesher_smooth = Ref<VoxelMesherSmooth>(memnew(VoxelMesherSmooth));
}

Vector3i g_viewer_block_pos; // TODO UGLY! Lambdas or pointers needed...

// Sorts distance to viewer
struct BlockUpdateComparator {
	inline bool operator()(const Vector3i & a, const Vector3i & b) const {
		return a.distance_sq(g_viewer_block_pos) > b.distance_sq(g_viewer_block_pos);
	}
};

void VoxelTerrain::set_provider(Ref<VoxelProvider> provider) {
	_provider = provider;
}

Ref<VoxelProvider> VoxelTerrain::get_provider() {
	return _provider;
}

Ref<VoxelLibrary> VoxelTerrain::get_voxel_library() {
	return _mesher->get_library();
}

void VoxelTerrain::set_generate_collisions(bool enabled) {
	_generate_collisions = enabled;
}

void VoxelTerrain::set_viewer_path(NodePath path) {
	if(!path.is_empty())
		ERR_FAIL_COND(get_viewer(path) == NULL);
	_viewer_path = path;
}

NodePath VoxelTerrain::get_viewer_path() {
	return _viewer_path;
}

Spatial * VoxelTerrain::get_viewer(NodePath path) {
	if(path.is_empty())
		return NULL;
	Node * node = get_node(path);
	if(node == NULL)
		return NULL;
	return node->cast_to<Spatial>();
}

//void VoxelTerrain::clear_update_queue() {
//	_block_update_queue.clear();
//	_dirty_blocks.clear();
//}

void VoxelTerrain::make_block_dirty(Vector3i bpos) {
	// TODO Immediate update viewer distance
	if(is_block_dirty(bpos) == false) {
		//OS::get_singleton()->print("Dirty (%i, %i, %i)", bpos.x, bpos.y, bpos.z);
		_block_update_queue.push_back(bpos);
		_dirty_blocks[bpos] = true;
	}
}

bool VoxelTerrain::is_block_dirty(Vector3i bpos) {
	return _dirty_blocks.has(bpos);
}

void VoxelTerrain::make_blocks_dirty(Vector3i min, Vector3i size) {
	Vector3i max = min + size;
	Vector3i pos;
	for(pos.z = min.z; pos.z < max.z; ++pos.z) {
		for(pos.y = min.y; pos.y < max.y; ++pos.y) {
			for(pos.x = min.x; pos.x < max.x; ++pos.x) {
				make_block_dirty(pos);
			}
		}
	}
}

inline int get_border_index(int x, int max) {
	return x == 0 ? 0 : x != max ? 1 : 2;
}

void VoxelTerrain::make_voxel_dirty(Vector3i pos) {

	// Update the block in which the voxel is
	Vector3i bpos = VoxelMap::voxel_to_block(pos);
	make_block_dirty(bpos);
	//OS::get_singleton()->print("Dirty (%i, %i, %i)\n", bpos.x, bpos.y, bpos.z);

	// Update neighbor blocks if the voxel is touching a boundary

	Vector3i rpos = VoxelMap::to_local(pos);

	bool check_corners = _mesher->get_occlusion_enabled();

	const int max = VoxelBlock::SIZE-1;

	if(rpos.x == 0)
		make_block_dirty(bpos - Vector3i(1,0,0));
	else
	if(rpos.x == max)
		make_block_dirty(bpos + Vector3i(1,0,0));

	if(rpos.y == 0)
		make_block_dirty(bpos - Vector3i(0,1,0));
	else
	if(rpos.y == max)
		make_block_dirty(bpos + Vector3i(0,1,0));

	if(rpos.z == 0)
		make_block_dirty(bpos - Vector3i(0,0,1));
	else
	if(rpos.z == max)
		make_block_dirty(bpos + Vector3i(0,0,1));

	// We might want to update blocks in corners in order to update ambient occlusion
	if(check_corners) {

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
			{-1,-1,-1}, { 0,-1,-1}, { 1,-1,-1},
			{-1,-1, 0}, { 0,-1, 0}, { 1,-1, 0},
			{-1,-1, 1}, { 0,-1, 1}, { 1,-1, 1},

			{-1, 0,-1}, { 0, 0,-1}, { 1, 0,-1},
			{-1, 0, 0}, { 0, 0, 0}, { 1, 0, 0},
			{-1, 0, 1}, { 0, 0, 1}, { 1, 0, 1},

			{-1, 1,-1}, { 0, 1,-1}, { 1, 1,-1},
			{-1, 1, 0}, { 0, 1, 0}, { 1, 1, 0},
			{-1, 1, 1}, { 0, 1, 1}, { 1, 1, 1}
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
			{0, 1, 3, 9}, {1}, {2, 1, 5, 11},
			{3}, {}, {5},
			{6, 3, 7, 15}, {7}, {8, 7, 5, 17},

			{9}, {}, {11},
			{}, {}, {},
			{15}, {}, {17},

			{18, 9, 19, 21}, {19}, {20, 11, 19, 23},
			{21}, {}, {23},
			{24, 15, 21, 25}, {25}, {26, 17, 23, 25}
		};

		int m = get_border_index(rpos.x, max)
				+ 3*get_border_index(rpos.z, max)
				+ 9*get_border_index(rpos.y, max);

		const int * ce_indexes = ce_indexes_lut[m];
		int ce_count = ce_counts[m];
		//OS::get_singleton()->print("m=%i, rpos=(%i, %i, %i)\n", m, rpos.x, rpos.y, rpos.z);

		for(int i = 0; i < ce_count; ++i) {
			// TODO Because it's about ambient occlusion across 1 voxel only,
			// we could optimize it even more by looking at neighbor voxels,
			// and discard the update if we know it won't change anything
			const int * normal = normals[ce_indexes[i]];
			Vector3i nbpos(bpos.x + normal[0], bpos.y + normal[1], bpos.z + normal[2]);
			//OS::get_singleton()->print("Corner dirty (%i, %i, %i)\n", nbpos.x, nbpos.y, nbpos.z);
			make_block_dirty(nbpos);
		}
	}
}

int VoxelTerrain::get_block_update_count() {
	return _block_update_queue.size();
}

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

	default:
		break;
	}
}

void VoxelTerrain::_process() {
	update_blocks();
}

void VoxelTerrain::update_blocks() {
	OS & os = *OS::get_singleton();

	// Get viewer location
	Spatial * viewer = get_viewer(_viewer_path);
	if(viewer)
		g_viewer_block_pos = VoxelMap::voxel_to_block(viewer->get_translation());
	else
		g_viewer_block_pos = Vector3i();

	// Sort updates so nearest blocks are done first
	VOXEL_PROFILE_BEGIN("block_update_sorting")
	_block_update_queue.sort_custom<BlockUpdateComparator>();
	VOXEL_PROFILE_END("block_update_sorting")

	uint32_t time_before = os.get_ticks_msec();
	uint32_t max_time = 1000 / 60;

	// Update a bunch of blocks until none are left or too much time elapsed
	while (!_block_update_queue.empty() && (os.get_ticks_msec() - time_before) < max_time) {

		//printf("Remaining: %i\n", _block_update_queue.size());

		// TODO Move this to a thread
		// TODO Have VoxelTerrainGenerator in C++

		// Get request
		Vector3i block_pos = _block_update_queue[_block_update_queue.size() - 1];

		bool entire_block_changed = false;

		if (!_map->has_block(block_pos)) {
			// Create buffer
			if(!_provider.is_null()) {

				VOXEL_PROFILE_BEGIN("voxel_buffer_creation_gen")

				Ref<VoxelBuffer> buffer_ref = Ref<VoxelBuffer>(memnew(VoxelBuffer));
				const Vector3i block_size(VoxelBlock::SIZE, VoxelBlock::SIZE, VoxelBlock::SIZE);
				buffer_ref->create(block_size.x, block_size.y, block_size.z);

				VOXEL_PROFILE_END("voxel_buffer_creation_gen")
				VOXEL_PROFILE_BEGIN("block_generation")

				// Query voxel provider
				_provider->emerge_block(buffer_ref, block_pos);

				// Check script return
				ERR_FAIL_COND(buffer_ref->get_size() != block_size);

				VOXEL_PROFILE_END("block_generation")

				// Store buffer
				_map->set_block_buffer(block_pos, buffer_ref);

				entire_block_changed = true;
			}
		}

		// Update views (mesh/collisions)

		if(entire_block_changed) {
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
		}
		else {
			// Only update the block, neighbors will probably follow if needed
			update_block_mesh(block_pos);
			//OS::get_singleton()->print("Update (%i, %i, %i)\n", block_pos.x, block_pos.y, block_pos.z);
		}

		// Pop request
		_block_update_queue.resize(_block_update_queue.size() - 1);
		_dirty_blocks.erase(block_pos);
	}
}


static inline bool is_mesh_empty(Ref<Mesh> mesh_ref) {
	if(mesh_ref.is_null())
		return true;
	Mesh & mesh = **mesh_ref;
	if(mesh.get_surface_count() == 0)
		return true;
	if(mesh.surface_get_array_len(Mesh::ARRAY_VERTEX) == 0)
		return true;
	return false;
}


void VoxelTerrain::update_block_mesh(Vector3i block_pos) {

	VoxelBlock * block = _map->get_block(block_pos);
	if (block == NULL) {
		return;
	}

	VOXEL_PROFILE_BEGIN("voxel_buffer_creation_extract")
	// Create buffer padded with neighbor voxels
	VoxelBuffer nbuffer;
	// TODO Make the buffer re-usable
	// TODO Padding set to 3 at the moment because Transvoxel works on 2x2 cells.
	// It should change for a smarter padding (if smooth isn't used for example).
	nbuffer.create(VoxelBlock::SIZE + 3, VoxelBlock::SIZE + 3, VoxelBlock::SIZE + 3);
	VOXEL_PROFILE_END("voxel_buffer_creation_extract")

	VOXEL_PROFILE_BEGIN("block_extraction")
	_map->get_buffer_copy(VoxelMap::block_to_voxel(block_pos) - Vector3i(1, 1, 1), nbuffer, 0x3);
	VOXEL_PROFILE_END("block_extraction")

	Vector3 block_node_pos = VoxelMap::block_to_voxel(block_pos).to_vec3();

	// TODO Re-use existing meshes to optimize memory cost	

	// Build cubic parts of the mesh
	Ref<ArrayMesh> mesh = _mesher->build(nbuffer, Voxel::CHANNEL_TYPE, Vector3i(0,0,0), nbuffer.get_size()-Vector3(1,1,1));
	// Build smooth parts of the mesh
	_mesher_smooth->build(nbuffer, Voxel::CHANNEL_ISOLEVEL, mesh);

	MeshInstance * mesh_instance = block->get_mesh_instance(*this);
	StaticBody * body = block->get_physics_body(*this);

	if(is_mesh_empty(mesh)) {
		if(mesh_instance) {
			mesh_instance->set_mesh(Ref<Mesh>());
		}
		if(body) {
			body->set_shape(0, Ref<Shape>());
		}
	}
	else {
		// The mesh exist and it has vertices

		// TODO Don't use nodes! Use servers directly, it's faster
		if (mesh_instance == NULL) {
			// Create and spawn mesh
			mesh_instance = memnew(MeshInstance);
			mesh_instance->set_mesh(mesh);
			mesh_instance->set_translation(block_node_pos);
			add_child(mesh_instance);
			block->mesh_instance_path = mesh_instance->get_path();
		}
		else {
			// Update mesh
			VOXEL_PROFILE_BEGIN("mesh_instance_set_mesh")
			mesh_instance->set_mesh(mesh);
			VOXEL_PROFILE_END("mesh_instance_set_mesh")
		}

		if(get_tree()->is_editor_hint() == false && _generate_collisions) {

			// Generate collisions
			// TODO Need to select only specific surfaces because some may not have collisions
			VOXEL_PROFILE_BEGIN("create_trimesh_shape")
			Ref<Shape> shape = mesh->create_trimesh_shape();
			VOXEL_PROFILE_END("create_trimesh_shape")

			if(body == NULL) {
				// Create body
				body = memnew(StaticBody);
				body->set_translation(block_node_pos);
				body->add_shape(shape);
				add_child(body);
				block->body_path = body->get_path();
			}
			else {
				// Update body
				VOXEL_PROFILE_BEGIN("body_set_shape")
				body->set_shape(0, shape);
				VOXEL_PROFILE_END("body_set_shape")
			}
		}
	}
}

//void VoxelTerrain::block_removed(VoxelBlock & block) {
//    MeshInstance * mesh_instance = block.get_mesh_instance(*this);
//    if (mesh_instance) {
//        mesh_instance->queue_delete();
//    }
//}

struct _VoxelTerrainRaycastContext {
	VoxelTerrain & terrain;
	//unsigned int channel_mask;
};

static bool _raycast_binding_predicate(Vector3i pos, void *context_ptr) {

	ERR_FAIL_COND_V(context_ptr == NULL, false);
	_VoxelTerrainRaycastContext * context = (_VoxelTerrainRaycastContext*)context_ptr;
	VoxelTerrain & terrain = context->terrain;

	//unsigned int channel = context->channel;

	Ref<VoxelMap> map = terrain.get_map();
	int v0 = map->get_voxel(pos, Voxel::CHANNEL_TYPE);

	Ref<VoxelLibrary> lib_ref = terrain.get_voxel_library();
	if(lib_ref.is_null())
		return false;
	const VoxelLibrary & lib = **lib_ref;

	if(lib.has_voxel(v0) == false)
		return false;

	const Voxel & voxel = lib.get_voxel_const(v0);
	if(voxel.is_transparent() == false)
		return true;

	int v1 = map->get_voxel(pos, Voxel::CHANNEL_ISOLEVEL);
	return v1 - 128 >= 0;
}

Variant VoxelTerrain::_raycast_binding(Vector3 origin, Vector3 direction, real_t max_distance) {

	// TODO Transform input if the terrain is rotated (in the future it can be made a Spatial node)

	Vector3i hit_pos;
	Vector3i prev_pos;

	_VoxelTerrainRaycastContext context = { *this };

	if(voxel_raycast(origin, direction, _raycast_binding_predicate, &context, max_distance, hit_pos, prev_pos)) {

		Dictionary hit = Dictionary();
		hit["position"] = hit_pos.to_vec3();
		hit["prev_position"] = prev_pos.to_vec3();
		return hit;
	}
	else {
		return Variant(); // Null dictionary, no alloc
	}
}

void VoxelTerrain::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_provider", "provider:VoxelProvider"), &VoxelTerrain::set_provider);
	ClassDB::bind_method(D_METHOD("get_provider:VoxelProvider"), &VoxelTerrain::get_provider);

	ClassDB::bind_method(D_METHOD("get_block_update_count"), &VoxelTerrain::get_block_update_count);
	ClassDB::bind_method(D_METHOD("get_mesher:VoxelMesher"), &VoxelTerrain::get_mesher);

	ClassDB::bind_method(D_METHOD("get_generate_collisions"), &VoxelTerrain::get_generate_collisions);
	ClassDB::bind_method(D_METHOD("set_generate_collisions", "enabled"), &VoxelTerrain::set_generate_collisions);

	ClassDB::bind_method(D_METHOD("get_viewer"), &VoxelTerrain::get_viewer_path);
	ClassDB::bind_method(D_METHOD("set_viewer", "path"), &VoxelTerrain::set_viewer_path);

	ClassDB::bind_method(D_METHOD("get_storage:VoxelMap"), &VoxelTerrain::get_map);

	// TODO Make those two static in VoxelMap?
	ClassDB::bind_method(D_METHOD("voxel_to_block", "voxel_pos"), &VoxelTerrain::_voxel_to_block_binding);
	ClassDB::bind_method(D_METHOD("block_to_voxel", "block_pos"), &VoxelTerrain::_block_to_voxel_binding);

	ClassDB::bind_method(D_METHOD("make_block_dirty", "pos"), &VoxelTerrain::_make_block_dirty_binding);
	ClassDB::bind_method(D_METHOD("make_blocks_dirty", "min", "size"), &VoxelTerrain::_make_blocks_dirty_binding);
	ClassDB::bind_method(D_METHOD("make_voxel_dirty", "pos"), &VoxelTerrain::_make_voxel_dirty_binding);

	ClassDB::bind_method(D_METHOD("raycast:Dictionary", "origin", "direction", "max_distance"), &VoxelTerrain::_raycast_binding, DEFVAL(100));

#ifdef VOXEL_PROFILING
	ClassDB::bind_method(D_METHOD("get_profiling_info"), &VoxelTerrain::get_profiling_info);
#endif

}

