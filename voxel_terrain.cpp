#include "voxel_terrain.h"
#include <scene/3d/mesh_instance.h>
#include <os/os.h>

VoxelTerrain::VoxelTerrain(): Node(), _min_y(-4), _max_y(4) {

	_map = Ref<VoxelMap>(memnew(VoxelMap));
	_mesher = Ref<VoxelMesher>(memnew(VoxelMesher));
}

// Sorts distance to world origin
// TODO Use distance to camera
struct BlockUpdateComparator0 {
	inline bool operator()(const Vector3i & a, const Vector3i & b) const {
		return a.length_sq() > b.length_sq();
	}
};

void VoxelTerrain::set_provider(Ref<VoxelProvider> provider) {
	_provider = provider;
}

Ref<VoxelProvider> VoxelTerrain::get_provider() {
	return _provider;
}

void VoxelTerrain::force_load_blocks(Vector3i center, Vector3i extents) {
	//Vector3i min = center - extents;
	//Vector3i max = center + extents + Vector3i(1,1,1);
	//Vector3i size = max - min;

	_block_update_queue.clear();

	Vector3i pos;
	for (pos.z = -extents.z; pos.z <= extents.z; ++pos.z) {
		for (pos.x = -extents.x; pos.x <= extents.x; ++pos.x) {
			for (pos.y = -extents.y; pos.y <= extents.y; ++pos.y) {
				_block_update_queue.push_back(pos);
			}
		}
	}

	_block_update_queue.sort_custom<BlockUpdateComparator0>();

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

	uint32_t time_before = os.get_ticks_msec();
	uint32_t max_time = 1000 / 60;

	while (!_block_update_queue.empty() && (os.get_ticks_msec() - time_before) < max_time) {
		//printf("Remaining: %i\n", _block_update_queue.size());

		// TODO Move this to a thread
		// TODO Have VoxelTerrainGenerator in C++
		// TODO Keep track of MeshInstances!

		// Get request
		Vector3i block_pos = _block_update_queue[_block_update_queue.size() - 1];

		if (!_map->has_block(block_pos)) {
			// Create buffer
			if(!_provider.is_null()) {
				Ref<VoxelBuffer> buffer_ref = Ref<VoxelBuffer>(memnew(VoxelBuffer));
				const Vector3i block_size(VoxelBlock::SIZE, VoxelBlock::SIZE, VoxelBlock::SIZE);
				buffer_ref->create(block_size.x, block_size.y, block_size.z);

				// Query voxel provider
				_provider->emerge_block(buffer_ref, block_pos);

				// Check script return
				ERR_FAIL_COND(buffer_ref->get_size() != block_size);

				// Store buffer
				_map->set_block_buffer(block_pos, buffer_ref);
			}
		}

		// Update meshes
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
		//update_block_mesh(block_pos);

		// Pop request
		_block_update_queue.resize(_block_update_queue.size() - 1);
	}
}

void VoxelTerrain::update_block_mesh(Vector3i block_pos) {
	VoxelBlock * block = _map->get_block(block_pos);
	if (block == NULL) {
		return;
	}
	if (block->voxels->is_uniform(0) && block->voxels->get_voxel(0, 0, 0, 0) == 0) {
		return;
	}

	// Create buffer padded with neighbor voxels
	VoxelBuffer nbuffer;
	nbuffer.create(VoxelBlock::SIZE + 2, VoxelBlock::SIZE + 2, VoxelBlock::SIZE + 2);
	_map->get_buffer_copy(VoxelMap::block_to_voxel(block_pos) - Vector3i(1, 1, 1), nbuffer);

	// TEST
	//if (block_pos == Vector3i(0, 0, 0)) {
	//    printf(">>>\n");
	//    String os;
	//    for (unsigned int y = 0; y < nbuffer.get_size().y; ++y) {
	//        for (unsigned int z = 0; z < nbuffer.get_size().z; ++z) {
	//            for (unsigned int x = 0; x < nbuffer.get_size().x; ++x) {
	//                if (nbuffer.get_voxel(x, y, z) == 0)
	//                    os += '-';
	//                else
	//                    os += 'O';
	//            }
	//            os += '\n';
	//        }
	//        os += '\n';
	//    }
	//    wprintf(os.c_str());
	//}

	// Build mesh (that part is the most CPU-intensive)
	Ref<Mesh> mesh = _mesher->build(nbuffer);

	MeshInstance * mesh_instance = block->get_mesh_instance(*this);
	if (mesh_instance == NULL) {
		// Create and spawn mesh
		mesh_instance = memnew(MeshInstance);
		mesh_instance->set_mesh(mesh);
		mesh_instance->set_translation(VoxelMap::block_to_voxel(block_pos).to_vec3());
		add_child(mesh_instance);
		block->mesh_instance_path = mesh_instance->get_path();
	}
	else {
		// Update mesh
		mesh_instance->set_mesh(mesh);
	}
}

//void VoxelTerrain::block_removed(VoxelBlock & block) {
//    MeshInstance * mesh_instance = block.get_mesh_instance(*this);
//    if (mesh_instance) {
//        mesh_instance->queue_delete();
//    }
//}

void VoxelTerrain::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_provider", "provider:VoxelProvider"), &VoxelTerrain::set_provider);
	ClassDB::bind_method(D_METHOD("get_provider:VoxelProvider"), &VoxelTerrain::get_provider);

	ClassDB::bind_method(D_METHOD("get_block_update_count"), &VoxelTerrain::get_block_update_count);
	ClassDB::bind_method(D_METHOD("get_mesher:VoxelMesher"), &VoxelTerrain::get_mesher);

	ClassDB::bind_method(D_METHOD("get_map:VoxelMap"), &VoxelTerrain::get_map);

	// TODO Make those two static in VoxelMap?
	ClassDB::bind_method(D_METHOD("voxel_to_block", "voxel_pos"), &VoxelTerrain::_voxel_to_block_binding);
	ClassDB::bind_method(D_METHOD("block_to_voxel", "block_pos"), &VoxelTerrain::_block_to_voxel_binding);

	ClassDB::bind_method(D_METHOD("force_load_blocks", "center", "extents"), &VoxelTerrain::_force_load_blocks_binding);

}

