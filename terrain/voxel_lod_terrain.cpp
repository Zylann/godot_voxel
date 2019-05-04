#include "voxel_lod_terrain.h"
#include "../math/rect3i.h"
#include "voxel_map.h"
#include "voxel_mesh_updater.h"
#include "voxel_provider_thread.h"
#include <core/engine.h>
#include <core/os/os.h>

VoxelLodTerrain::VoxelLodTerrain() {

	print_line("Construct VoxelLodTerrain");

	_lods[0].map.instance();

	set_lod_count(8);
	set_lod_split_scale(3);

	reset_updater();
}

VoxelLodTerrain::~VoxelLodTerrain() {

	print_line("Destroy VoxelLodTerrain");

	if (_provider_thread) {
		memdelete(_provider_thread);
	}

	if (_block_updater) {
		memdelete(_block_updater);
	}
}

Ref<Material> VoxelLodTerrain::get_material() const {
	return _material;
}

void VoxelLodTerrain::set_material(Ref<Material> p_material) {
	_material = p_material;
}

Ref<VoxelProvider> VoxelLodTerrain::get_provider() const {
	return _provider;
}

int VoxelLodTerrain::get_block_size() const {
	return _lods[0].map->get_block_size();
}

int VoxelLodTerrain::get_block_size_pow2() const {
	return _lods[0].map->get_block_size_pow2();
}

void VoxelLodTerrain::set_provider(Ref<VoxelProvider> p_provider) {
	if (p_provider != _provider) {

		if (_provider_thread) {
			memdelete(_provider_thread);
			_provider_thread = nullptr;
		}

		_provider = p_provider;
		_provider_thread = memnew(VoxelProviderThread(_provider, get_block_size_pow2()));

		// The whole map might change, so make all area dirty
		// TODO Actually, we should regenerate the whole map, not just update all its blocks
		make_all_view_dirty_deferred();
	}
}

void VoxelLodTerrain::make_all_view_dirty_deferred() {
	for (int i = 0; i < get_lod_count(); ++i) {
		Lod &lod = _lods[i];
		lod.last_view_distance_blocks = 0;
	}
}

int VoxelLodTerrain::get_view_distance() const {
	return 0;
}

void VoxelLodTerrain::set_view_distance(int p_distance_in_voxels) {
	// TODO this will be used to cap mesh visibility

	//	ERR_FAIL_COND(p_distance_in_voxels < 0)
	//	int d = p_distance_in_voxels / get_block_size();
	//	if (d != _view_distance_blocks) {
	//		print_line(String("View distance changed from ") + String::num(_view_distance_blocks) + String(" blocks to ") + String::num(d));
	//		_view_distance_blocks = d;
	//		// Blocks too far away will be removed in _process, same for blocks to load
	//	}
}

Spatial *VoxelLodTerrain::get_viewer() const {
	if (_viewer_path.is_empty()) {
		return nullptr;
	}
	Node *node = get_node(_viewer_path);
	if (node == nullptr) {
		return nullptr;
	}
	return Object::cast_to<Spatial>(node);
}

void VoxelLodTerrain::immerge_block(Vector3i block_pos, unsigned int lod_index) {

	ERR_FAIL_COND(lod_index >= get_lod_count());
	ERR_FAIL_COND(_lods[lod_index].map.is_null());

	Lod &lod = _lods[lod_index];

	// TODO Schedule block saving when supported
	lod.map->remove_block(block_pos, VoxelMap::NoAction());

	BlockState state = BLOCK_NONE;
	Map<Vector3i, BlockState>::Element *E = lod.block_states.find(block_pos);
	if (E) {
		state = E->value();
		lod.block_states.erase(E);
	}

	// Blocks in the update queue will be cancelled in _process,
	// because it's too expensive to linear-search all blocks for each block

	// No need to remove things from blocks_pending_load,
	// This vector is filled and cleared immediately in the main process.
	// It is a member only to re-use its capacity memory over frames.
}

void VoxelLodTerrain::reset_updater() {

	if (_block_updater) {
		memdelete(_block_updater);
		_block_updater = NULL;
	}

	// TODO Thread-safe way to change those parameters
	VoxelMeshUpdater::MeshingParams params;
	params.smooth_surface = true;

	_block_updater = memnew(VoxelMeshUpdater(Ref<VoxelLibrary>(), params));

	// TODO Revert any pending update states!
}

void VoxelLodTerrain::set_lod_split_scale(float p_lod_split_scale) {
	_lod_octree.set_split_scale(p_lod_split_scale);
}

float VoxelLodTerrain::get_lod_split_scale() const {
	return _lod_octree.get_split_scale();
}

void VoxelLodTerrain::set_lod_count(unsigned int p_lod_count) {

	ERR_FAIL_COND(p_lod_count >= MAX_LOD);

	if (get_lod_count() != p_lod_count) {

		int bs = get_block_size();
		_lod_octree.create_from_lod_count(bs, p_lod_count, LodOctree<bool>::NoDestroyAction());

		for (int lod_index = 0; lod_index < MAX_LOD; ++lod_index) {

			Lod &lod = _lods[lod_index];

			lod.blocks_in_meshing_area.clear();

			// Instance new maps if we have more lods, or clear them otherwise
			if (lod_index < get_lod_count()) {

				if (lod.map.is_null()) {
					lod.map.instance();
					lod.map->set_lod_index(lod_index);
				} else {
					lod.map->clear();
				}

			} else {

				if (lod.map.is_valid()) {
					lod.map.unref();
				}
			}
		}
	}
}

int VoxelLodTerrain::get_lod_count() const {
	return _lod_octree.get_lod_count();
}

void VoxelLodTerrain::set_viewer_path(NodePath path) {
	_viewer_path = path;
}

NodePath VoxelLodTerrain::get_viewer_path() const {
	return _viewer_path;
}

VoxelLodTerrain::BlockState VoxelLodTerrain::get_block_state(Vector3 bpos, unsigned int lod_index) const {
	ERR_FAIL_COND_V(lod_index >= get_lod_count(), BLOCK_NONE);
	const Lod &lod = _lods[lod_index];
	Vector3i ibpos(bpos);
	const Map<Vector3i, BlockState>::Element *state = lod.block_states.find(ibpos);
	if (state) {
		return state->value();
	} else {
		if (lod.map->has_block(ibpos)) {
			return BLOCK_IDLE;
		} else {
			return BLOCK_NONE;
		}
	}
}

bool VoxelLodTerrain::is_block_meshed(Vector3 bpos, unsigned int lod_index) const {
	ERR_FAIL_COND_V(lod_index >= get_lod_count(), false);
	const Lod &lod = _lods[lod_index];
	Vector3i ibpos(bpos);
	const VoxelBlock *block = lod.map->get_block(ibpos);
	if (block) {
		return block->has_mesh();
	} else {
		return false;
	}
}

bool VoxelLodTerrain::is_block_shown(Vector3 bpos, unsigned int lod_index) const {
	ERR_FAIL_COND_V(lod_index >= get_lod_count(), false);
	const Lod &lod = _lods[lod_index];
	Vector3i ibpos(bpos);
	const VoxelBlock *block = lod.map->get_block(ibpos);
	if (block) {
		return block->is_visible();
	} else {
		return false;
	}
}

void VoxelLodTerrain::_notification(int p_what) {

	struct EnterWorldAction {
		World *world;
		EnterWorldAction(World *w) :
				world(w) {}
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
		SetVisibilityAction(bool v) :
				visible(v) {}
		void operator()(VoxelBlock *block) {
			block->set_visible(visible);
		}
	};

	switch (p_what) {

		case NOTIFICATION_ENTER_TREE:
			set_process(true);
			break;

		case NOTIFICATION_PROCESS:
			if (!Engine::get_singleton()->is_editor_hint()) {
				_process();
			}
			break;

		case NOTIFICATION_EXIT_TREE:
			break;

		case NOTIFICATION_ENTER_WORLD:
			for_all_blocks(EnterWorldAction(*get_world()));
			break;

		case NOTIFICATION_EXIT_WORLD:
			for_all_blocks(ExitWorldAction());
			break;

		case NOTIFICATION_VISIBILITY_CHANGED:
			for_all_blocks(SetVisibilityAction(is_visible()));
			break;

			// TODO Listen for transform changes

		default:
			break;
	}
}

Vector3 VoxelLodTerrain::get_viewer_pos() const {

	if (Engine::get_singleton()->is_editor_hint()) {

		// TODO Use editor's camera here
		return Vector3();

	} else {

		// TODO Use viewport camera, much easier
		Spatial *viewer = get_viewer();

		if (viewer) {
			return viewer->get_global_transform().origin;
		}
	}

	return Vector3();
}

void VoxelLodTerrain::make_block_dirty(Vector3i bpos, unsigned int lod_index) {

	CRASH_COND(lod_index >= get_lod_count());

	// TODO Immediate update viewer distance?

	Lod &lod = _lods[lod_index];

	Map<Vector3i, BlockState>::Element *state = lod.block_states.find(bpos);

	if (state == NULL) {
		// The block is not dirty, so it will either be loaded or updated

		if (lod.map->has_block(bpos)) {

			lod.blocks_pending_update.push_back(bpos);
			lod.block_states.insert(bpos, BLOCK_UPDATE_NOT_SENT);

		} else {

			lod.blocks_to_load.push_back(bpos);
			lod.block_states.insert(bpos, BLOCK_LOAD);
		}

	} else if (state->value() == BLOCK_UPDATE_SENT) {
		// The updater is already processing the block,
		// but the block was modified again so we schedule another update
		state->value() = BLOCK_UPDATE_NOT_SENT;
		lod.blocks_pending_update.push_back(bpos);

	} else if (state->value() == BLOCK_UPDATE_NOT_SENT) {
		WARN_PRINT("Block already marked as BLOCK_UPDATE_NOT_SENT");
	}
}

static void remove_positions_outside_box(
		std::vector<Vector3i> &positions,
		Rect3i box,
		Map<Vector3i, VoxelLodTerrain::BlockState> &state_map) {

	for (int i = 0; i < positions.size(); ++i) {
		const Vector3i bpos = positions[i];
		if (!box.contains(bpos)) {
			int last = positions.size() - 1;
			positions[i] = positions[last];
			positions.resize(last);
			state_map.erase(bpos);
			--i;
		}
	}
}

void VoxelLodTerrain::_process() {

	if (get_lod_count() == 0) {
		// If there isn't a LOD 0, there is nothing to load
		return;
	}

	OS &os = *OS::get_singleton();

	Vector3 viewer_pos = get_viewer_pos();
	Vector3i viewer_block_pos = _lods[0].map->voxel_to_block(viewer_pos);

	// Here we go...

	// Find out which blocks _data_ need to be loaded
	{
		// This should be the same distance relatively to each LOD
		int view_distance_blocks_within_lod = static_cast<int>(_lod_octree.get_split_scale()) * 2 + 1;

		for (unsigned int lod_index = 0; lod_index < get_lod_count(); ++lod_index) {
			Lod &lod = _lods[lod_index];

			// Each LOD keeps a box of loaded blocks, and only some of the blocks will get polygonized.
			// The player can edit them so changes can be propagated to lower lods.

			int block_size_po2 = _lods[0].map->get_block_size_pow2() + lod_index;
			Vector3i viewer_block_pos_within_lod = VoxelMap::voxel_to_block_b(viewer_pos, block_size_po2);

			Rect3i new_box = Rect3i::from_center_extents(
					viewer_block_pos_within_lod,
					Vector3i(view_distance_blocks_within_lod));

			Rect3i prev_box = Rect3i::from_center_extents(
					lod.last_viewer_block_pos,
					Vector3i(lod.last_view_distance_blocks));

			// Eliminate pending blocks that aren't needed
			remove_positions_outside_box(lod.blocks_to_load, new_box, lod.block_states);
			remove_positions_outside_box(lod.blocks_pending_update, new_box, lod.block_states);

			if (prev_box != new_box) {

				Rect3i bounds = Rect3i::get_bounding_box(prev_box, new_box);
				Vector3i max = bounds.pos + bounds.size;

				// TODO This will explode if the player teleports!
				// There is a smarter way to only iterate relevant blocks

				Vector3i pos;
				for (pos.z = bounds.pos.z; pos.z < max.z; ++pos.z) {
					for (pos.y = bounds.pos.y; pos.y < max.y; ++pos.y) {
						for (pos.x = bounds.pos.x; pos.x < max.x; ++pos.x) {

							bool prev_contains = prev_box.contains(pos);
							bool new_contains = new_box.contains(pos);

							if (prev_contains && !new_contains) {
								// Unload block
								immerge_block(pos, lod_index);

							} else if (!prev_contains && new_contains) {
								// Load or update block
								make_block_dirty(pos, lod_index);
							}
						}
					}
				}
			}

			lod.last_viewer_block_pos = viewer_block_pos_within_lod;
			lod.last_view_distance_blocks = view_distance_blocks_within_lod;
		}
	}

	// Send block loading requests
	{
		VoxelProviderThread::InputData input;

		input.priority_block_position = viewer_block_pos;

		for (unsigned int lod_index = 0; lod_index < get_lod_count(); ++lod_index) {
			Lod &lod = _lods[lod_index];

			for (int i = 0; i < lod.blocks_to_load.size(); ++i) {
				VoxelProviderThread::EmergeInput input_block;
				input_block.block_position = lod.blocks_to_load[i];
				input_block.lod = lod_index;
				input.blocks_to_emerge.push_back(input_block);
			}

			lod.blocks_to_load.clear();
		}

		_provider_thread->push(input);
	}

	// Get block loading responses
	// Note: if block loading is too fast, this can cause stutters.
	// It should only happen on first load, though.
	{
		VoxelProviderThread::OutputData output;
		_provider_thread->pop(output);

		for (int i = 0; i < output.emerged_blocks.size(); ++i) {

			const VoxelProviderThread::EmergeOutput &eo = output.emerged_blocks[i];

			if (eo.lod >= get_lod_count()) {
				// That block was requested at a time where LOD was higher... drop it
				continue;
			}

			Lod &lod = _lods[eo.lod];

			Map<Vector3i, BlockState>::Element *state = lod.block_states.find(eo.block_position);
			if (state == nullptr || state->value() != BLOCK_LOAD) {
				// That block was not requested, or is no longer needed. drop it...
				continue;
			}

			if (eo.voxels->get_size() != lod.map->get_block_size()) {
				// Voxel block size is incorrect, drop it
				ERR_PRINT("Block size obtained from provider is different from expected size");
				continue;
			}

			// Store buffer
			bool check_neighbors = !lod.map->has_block(eo.block_position);
			lod.map->set_block_buffer(eo.block_position, eo.voxels);

			lod.block_states.erase(state);

			// if the block is surrounded or any of its neighbors becomes surrounded, and are marked to mesh,
			// it should be added to meshing requests
			if (check_neighbors) {
				// The block was not there before, so its Moore neighbors may be surrounded now.
				Vector3i ndir;
				for (ndir.z = -1; ndir.z < 2; ++ndir.z) {
					for (ndir.x = -1; ndir.x < 2; ++ndir.x) {
						for (ndir.y = -1; ndir.y < 2; ++ndir.y) {
							Vector3i npos = eo.block_position + ndir;

							if (lod.map->is_block_surrounded(npos)) {

								Map<Vector3i, BlockState>::Element *state = lod.block_states.find(npos);
								if (state && state->value() == BLOCK_UPDATE_NOT_SENT) {
									// Assuming it is scheduled to be updated already.
									// In case of BLOCK_UPDATE_SENT, we'll have to resend it.
									continue;
								}

								if (state) {
									state->value() = BLOCK_UPDATE_NOT_SENT;
								} else {
									lod.block_states.insert(npos, BLOCK_UPDATE_NOT_SENT);
								}

								lod.blocks_pending_update.push_back(npos);
							}
						}
					}
				}

			} else {
				// Only update the block, neighbors will probably follow if needed
				lod.block_states[eo.block_position] = BLOCK_UPDATE_NOT_SENT;
				lod.blocks_pending_update.push_back(eo.block_position);
			}
		}
	}

	// Send mesh updates
	{
		VoxelMeshUpdater::Input input;

		for (unsigned int lod_index = 0; lod_index < get_lod_count(); ++lod_index) {
			Lod &lod = _lods[lod_index];

			for (int i = 0; i < lod.blocks_pending_update.size(); ++i) {
				Vector3i block_pos = lod.blocks_pending_update[i];

				Map<Vector3i, BlockState>::Element *block_state = lod.block_states.find(block_pos);
				CRASH_COND(block_state == NULL);
				CRASH_COND(block_state->value() != BLOCK_UPDATE_NOT_SENT);

				// TODO Perhaps we could do a bit of early-rejection before spending time in buffer copy?

				// Create buffer padded with neighbor voxels
				Ref<VoxelBuffer> nbuffer;
				nbuffer.instance();

				// TODO Make the buffer re-usable
				unsigned int block_size = lod.map->get_block_size();
				unsigned int padding = _block_updater->get_required_padding();
				nbuffer->create(
						block_size + 2 * padding,
						block_size + 2 * padding,
						block_size + 2 * padding);

				unsigned int channels_mask = (1 << VoxelBuffer::CHANNEL_ISOLEVEL);
				lod.map->get_buffer_copy(lod.map->block_to_voxel(block_pos) - Vector3i(padding), **nbuffer, channels_mask);

				VoxelMeshUpdater::InputBlock iblock;
				iblock.voxels = nbuffer;
				iblock.position = block_pos;
				iblock.lod = lod_index;
				input.blocks.push_back(iblock);

				block_state->value() = BLOCK_UPDATE_SENT;
			}

			lod.blocks_pending_update.clear();
		}

		_block_updater->push(input);
	}

	// Receive mesh updates
	{
		{
			VoxelMeshUpdater::Output output;
			_block_updater->pop(output);

			for (unsigned int i = 0; i < output.blocks.size(); ++i) {
				const VoxelMeshUpdater::OutputBlock &ob = output.blocks[i];

				if (ob.lod >= get_lod_count()) {
					// Sorry, LOD configuration changed, drop that mesh
					continue;
				}

				_blocks_pending_main_thread_update.push_back(ob);
			}
		}

		Ref<World> world = get_world();
		uint32_t timeout = os.get_ticks_msec() + 10; // Allocate milliseconds max to upload meshes

		int queue_index = 0;

		// The following is done on the main thread because Godot doesn't really support multithreaded Mesh allocation.
		// This also proved to be very slow compared to the meshing process itself...
		// hopefully Vulkan will allow us to upload graphical resources without stalling rendering as they upload?

		for (; queue_index < _blocks_pending_main_thread_update.size() && os.get_ticks_msec() < timeout; ++queue_index) {

			const VoxelMeshUpdater::OutputBlock &ob = _blocks_pending_main_thread_update[queue_index];

			if (ob.lod >= get_lod_count()) {
				// Sorry, LOD configuration changed, drop that mesh
				continue;
			}

			Lod &lod = _lods[ob.lod];

			Map<Vector3i, BlockState>::Element *state = lod.block_states.find(ob.position);
			if (state && state->value() == BLOCK_UPDATE_SENT) {
				lod.block_states.erase(state);
			}

			VoxelBlock *block = lod.map->get_block(ob.position);
			if (block == NULL) {
				// That block is no longer loaded, drop the result
				continue;
			}

			Ref<ArrayMesh> mesh;
			mesh.instance();

			int surface_index = 0;
			for (int i = 0; i < ob.smooth_surfaces.surfaces.size(); ++i) {

				Array surface = ob.smooth_surfaces.surfaces[i];
				if (surface.empty()) {
					continue;
				}

				CRASH_COND(surface.size() != Mesh::ARRAY_MAX);
				mesh->add_surface_from_arrays(ob.smooth_surfaces.primitive_type, surface);
				mesh->surface_set_material(surface_index, _material);
				// No multi-material supported yet
				++surface_index;
			}

			if (is_mesh_empty(mesh)) {
				mesh = Ref<Mesh>();
			}

			block->set_mesh(mesh, world);
			block->set_visible(lod.blocks_in_meshing_area.has(ob.position));
			block->has_been_meshed = true;
		}

		shift_up(_blocks_pending_main_thread_update, queue_index);
	}

	// Find out which blocks need to be shown
	{
		struct SubdivideAction {
			VoxelLodTerrain *self;

			bool can_do(LodOctree<bool>::Node *node, unsigned int lod_index) {
				CRASH_COND(lod_index == 0);

				unsigned int child_lod_index = lod_index - 1;
				Lod &lod = self->_lods[child_lod_index];

				// Can only subdivide if higher detail meshes are ready to be shown, otherwise it will produce holes
				for (int i = 0; i < 8; ++i) {
					Vector3i child_pos = LodOctree<bool>::get_child_position(node->position, i);
					VoxelBlock *block = lod.map->get_block(child_pos);
					if (block == nullptr || !block->has_been_meshed) {
						return false;
					}
				}
				return true;
			}

			bool operator()(LodOctree<bool>::Node *node, unsigned int lod_index) {
				Lod &lod = self->_lods[lod_index];
				Vector3i bpos = node->position;
				lod.blocks_in_meshing_area.insert(bpos);
				VoxelBlock *block = lod.map->get_block(bpos);
				CRASH_COND(block == nullptr);
				block->set_visible(true);
				return true;
			}
		};

		struct UnsubdivideAction {
			VoxelLodTerrain *self;

			bool can_do(LodOctree<bool>::Node *node, unsigned int lod_index) {
				// Can only unsubdivide if the parent mesh is ready
				Lod &lod = self->_lods[lod_index];
				VoxelBlock *block = lod.map->get_block(node->position);
				if (block == nullptr) {
					// Ok, that block got unloaded? Might happen if you teleport away
					return true;
				} else {
					return block->has_been_meshed;
				}
			}

			void operator()(LodOctree<bool>::Node *node, unsigned int lod_index) {
				Lod &lod = self->_lods[lod_index];
				Vector3i bpos = node->position;
				lod.blocks_in_meshing_area.erase(bpos);
				VoxelBlock *block = lod.map->get_block(bpos);
				if (block) {
					block->set_visible(false);
				}
			}
		};

		SubdivideAction subdivide_action;
		subdivide_action.self = this;

		UnsubdivideAction unsubdivide_action;
		unsubdivide_action.self = this;

		_lod_octree.update(viewer_pos, subdivide_action, unsubdivide_action);
	}
}

void VoxelLodTerrain::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_provider", "provider"), &VoxelLodTerrain::set_provider);
	ClassDB::bind_method(D_METHOD("get_provider"), &VoxelLodTerrain::get_provider);

	ClassDB::bind_method(D_METHOD("set_view_distance", "distance_in_voxels"), &VoxelLodTerrain::set_view_distance);
	ClassDB::bind_method(D_METHOD("get_view_distance"), &VoxelLodTerrain::get_view_distance);

	ClassDB::bind_method(D_METHOD("get_viewer_path"), &VoxelLodTerrain::get_viewer_path);
	ClassDB::bind_method(D_METHOD("set_viewer_path", "path"), &VoxelLodTerrain::set_viewer_path);

	ClassDB::bind_method(D_METHOD("set_lod_count", "lod_count"), &VoxelLodTerrain::set_lod_count);
	ClassDB::bind_method(D_METHOD("get_lod_count"), &VoxelLodTerrain::get_lod_count);

	ClassDB::bind_method(D_METHOD("get_block_state", "block_pos", "lod"), &VoxelLodTerrain::get_block_state);
	ClassDB::bind_method(D_METHOD("is_block_meshed", "block_pos", "lod"), &VoxelLodTerrain::is_block_meshed);
	ClassDB::bind_method(D_METHOD("is_block_shown", "block_pos", "lod"), &VoxelLodTerrain::is_block_shown);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "provider", PROPERTY_HINT_RESOURCE_TYPE, "VoxelProvider"), "set_provider", "get_provider");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "view_distance"), "set_view_distance", "get_view_distance");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "viewer_path"), "set_viewer_path", "get_viewer_path");

	BIND_ENUM_CONSTANT(BLOCK_NONE);
	BIND_ENUM_CONSTANT(BLOCK_LOAD);
	BIND_ENUM_CONSTANT(BLOCK_UPDATE_NOT_SENT);
	BIND_ENUM_CONSTANT(BLOCK_UPDATE_SENT);
	BIND_ENUM_CONSTANT(BLOCK_IDLE);
}
