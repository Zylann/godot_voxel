#include "voxel_lod_terrain.h"
#include "../math/rect3i.h"
#include "../util/profiling_clock.h"
#include "voxel_map.h"
#include "voxel_mesh_updater.h"
#include <core/engine.h>

const uint32_t MAIN_THREAD_MESHING_BUDGET_MS = 8;

VoxelLodTerrain::VoxelLodTerrain() {

	print_line("Construct VoxelLodTerrain");

	_lods[0].map.instance();

	set_lod_count(8);
	set_lod_split_scale(3);

	reset_updater();
}

VoxelLodTerrain::~VoxelLodTerrain() {

	print_line("Destroy VoxelLodTerrain");

	if (_stream_thread) {
		memdelete(_stream_thread);
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

Ref<VoxelStream> VoxelLodTerrain::get_stream() const {
	return _stream;
}

unsigned int VoxelLodTerrain::get_block_size() const {
	return _lods[0].map->get_block_size();
}

unsigned int VoxelLodTerrain::get_block_size_pow2() const {
	return _lods[0].map->get_block_size_pow2();
}

void VoxelLodTerrain::set_stream(Ref<VoxelStream> p_stream) {
	if (p_stream != _stream) {

		if (_stream_thread) {
			memdelete(_stream_thread);
			_stream_thread = nullptr;
		}

		_stream = p_stream;
		_stream_thread = memnew(VoxelDataLoader(1, _stream, get_block_size_pow2()));

		// The whole map might change, so make all area dirty
		// TODO Actually, we should regenerate the whole map, not just update all its blocks
		make_all_view_dirty_deferred();
	}
}

void VoxelLodTerrain::make_all_view_dirty_deferred() {
	for (unsigned int i = 0; i < get_lod_count(); ++i) {
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

	lod.loading_blocks.erase(block_pos);

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

	_block_updater = memnew(VoxelMeshUpdater(2, params));

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

		unsigned int bs = get_block_size();
		LodOctree<bool>::NoDestroyAction nda;
		_lod_octree.create_from_lod_count(bs, p_lod_count, nda);

		for (unsigned int lod_index = 0; lod_index < MAX_LOD; ++lod_index) {

			Lod &lod = _lods[lod_index];

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

unsigned int VoxelLodTerrain::get_lod_count() const {
	return _lod_octree.get_lod_count();
}

void VoxelLodTerrain::set_viewer_path(NodePath path) {
	_viewer_path = path;
}

NodePath VoxelLodTerrain::get_viewer_path() const {
	return _viewer_path;
}

int VoxelLodTerrain::get_block_region_extent() const {
	// This is the radius of blocks around the viewer in which we may load them.
	// It depends on the LOD split scale, which tells how close to a block we need to be for it to subdivide.
	// Each LOD is fractal so that value is the same for each of them.
	return static_cast<int>(_lod_octree.get_split_scale()) * 2 + 2;
}

Dictionary VoxelLodTerrain::get_block_info(Vector3 fbpos, unsigned int lod_index) const {
	// Gets some info useful for debugging
	Dictionary d;
	ERR_FAIL_COND_V(lod_index >= get_lod_count(), d);

	const Lod &lod = _lods[lod_index];
	Vector3i bpos(fbpos);

	bool meshed = false;
	bool visible = false;
	int loading_state = 0;
	const VoxelBlock *block = lod.map->get_block(bpos);
	if (block) {
		meshed = block->has_been_meshed();
		visible = block->is_visible();
		loading_state = 2;
	} else if (lod.loading_blocks.has(bpos)) {
		loading_state = 1;
	}

#ifdef TOOLS_ENABLED
	int debug_unexpected_drop_time = -10000;
	const int *ptr = lod.debug_unexpected_load_drop_time.getptr(bpos);
	if (ptr) {
		debug_unexpected_drop_time = *ptr;
	}
	d["debug_unexpected_drop_time"] = debug_unexpected_drop_time;
#endif

	d["loading"] = loading_state;
	d["meshed"] = meshed;
	d["visible"] = visible;
	return d;
}

Vector3 VoxelLodTerrain::voxel_to_block_position(Vector3 vpos, unsigned int lod_index) const {
	ERR_FAIL_COND_V(lod_index >= get_lod_count(), Vector3());
	const Lod &lod = _lods[lod_index];
	Vector3i bpos = lod.map->voxel_to_block(Vector3i(vpos)) >> lod_index;
	return bpos.to_vec3();
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
			{
				EnterWorldAction ewa(*get_world());
				for_all_blocks(ewa);
			}
			break;

		case NOTIFICATION_EXIT_WORLD:
			{
				ExitWorldAction ewa;
				for_all_blocks(ewa);
			}
			break;

		case NOTIFICATION_VISIBILITY_CHANGED:
			{
				SetVisibilityAction sva(is_visible());
				for_all_blocks(sva);
			}
			break;

			// TODO Listen for transform changes

		default:
			break;
	}
}

Vector3 VoxelLodTerrain::get_viewer_pos(Vector3 &out_direction) const {

	if (Engine::get_singleton()->is_editor_hint()) {

		// TODO Use editor's camera here
		return Vector3();

	} else {

		Spatial *viewer = get_viewer();

		if (viewer) {
			out_direction = -viewer->get_global_transform().basis.get_axis(Vector3::AXIS_Z);
			return viewer->get_global_transform().origin;
		}
	}

	return Vector3();
}

void VoxelLodTerrain::try_schedule_loading_with_neighbors(const Vector3i &p_bpos, unsigned int lod_index) {
	CRASH_COND(lod_index >= get_lod_count());
	Lod &lod = _lods[lod_index];

	Vector3i bpos;

	for (int y = -1; y < 2; ++y) {
		for (int z = -1; z < 2; ++z) {
			for (int x = -1; x < 2; ++x) {

				bpos.x = p_bpos.x + x;
				bpos.y = p_bpos.y + y;
				bpos.z = p_bpos.z + z;

				VoxelBlock *block = lod.map->get_block(bpos);

				if (block == nullptr) {
					if (!lod.loading_blocks.has(bpos)) {
						lod.blocks_to_load.push_back(bpos);
						lod.loading_blocks.insert(bpos);
					}
				}
			}
		}
	}
}

bool VoxelLodTerrain::check_block_loaded_and_updated(const Vector3i &p_bpos, unsigned int lod_index) {
	CRASH_COND(lod_index >= get_lod_count());
	Lod &lod = _lods[lod_index];

	VoxelBlock *block = lod.map->get_block(p_bpos);

	if (block == nullptr) {
		try_schedule_loading_with_neighbors(p_bpos, lod_index);
		return false;
	}

	if (!block->has_been_meshed()) {
		if (!block->is_mesh_update_scheduled()) {
			if (lod.map->is_block_surrounded(block->position)) {

				lod.blocks_pending_update.push_back(block->position);
				block->set_mesh_state(VoxelBlock::MESH_UPDATE_NOT_SENT);

			} else {
				try_schedule_loading_with_neighbors(p_bpos, lod_index);
			}
		}
		return false;
	}

	return true;
}

//static void remove_positions_outside_box(
//		std::vector<Vector3i> &positions,
//		Rect3i box,
//		Set<Vector3i> &position_set) {

//	for (unsigned int i = 0; i < positions.size(); ++i) {
//		const Vector3i bpos = positions[i];
//		if (!box.contains(bpos)) {
//			int last = positions.size() - 1;
//			positions[i] = positions[last];
//			positions.resize(last);
//			position_set.erase(bpos);
//			--i;
//		}
//	}
//}

void VoxelLodTerrain::_process() {

	if (get_lod_count() == 0) {
		// If there isn't a LOD 0, there is nothing to load
		return;
	}

	OS &os = *OS::get_singleton();

	Vector3 viewer_direction;
	Vector3 viewer_pos = get_viewer_pos(viewer_direction);
	Vector3i viewer_block_pos = _lods[0].map->voxel_to_block(viewer_pos);

	ProfilingClock profiling_clock;

	_stats.dropped_block_loads = 0;
	_stats.dropped_block_meshs = 0;

	// Here we go...

	// Remove blocks falling out of block region extent
	{
		// This should be the same distance relatively to each LOD
		int block_region_extent = get_block_region_extent();

		for (unsigned int lod_index = 0; lod_index < get_lod_count(); ++lod_index) {
			Lod &lod = _lods[lod_index];

			// Each LOD keeps a box of loaded blocks, and only some of the blocks will get polygonized.
			// The player can edit them so changes can be propagated to lower lods.

			unsigned int block_size_po2 = _lods[0].map->get_block_size_pow2() + lod_index;
			Vector3i viewer_block_pos_within_lod = VoxelMap::voxel_to_block_b(viewer_pos, block_size_po2);

			Rect3i new_box = Rect3i::from_center_extents(viewer_block_pos_within_lod, Vector3i(block_region_extent));
			Rect3i prev_box = Rect3i::from_center_extents(lod.last_viewer_block_pos, Vector3i(lod.last_view_distance_blocks));

			// Eliminate pending blocks that aren't needed

			// No need to do it on those arrays, they are always clear at this point.
			// Let's assert so it will pop on your face the day that assumption changes
			CRASH_COND(!lod.blocks_to_load.empty());
			CRASH_COND(!lod.blocks_pending_update.empty());
			//remove_positions_outside_box(lod.blocks_to_load, new_box, lod.loading_blocks);
			//remove_positions_outside_box(lod.blocks_pending_update, new_box, lod.loading_blocks);

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
							}
						}
					}
				}
			}

			lod.last_viewer_block_pos = viewer_block_pos_within_lod;
			lod.last_view_distance_blocks = block_region_extent;
		}
	}

	// Find which blocks we need to load and see
	{
		struct SubdivideAction {
			VoxelLodTerrain *self;
			unsigned int blocked_count = 0;

			bool can_do(LodOctree<bool>::Node *node, unsigned int lod_index) {
				CRASH_COND(lod_index == 0);
				unsigned int child_lod_index = lod_index - 1;
				bool can = true;
				// Can only subdivide if higher detail meshes are ready to be shown, otherwise it will produce holes
				for (int i = 0; i < 8; ++i) {
					Vector3i child_pos = LodOctree<bool>::get_child_position(node->position, i);
					can &= self->check_block_loaded_and_updated(child_pos, child_lod_index);
				}
				if (!can) {
					++blocked_count;
				}
				return can;
			}

			bool operator()(LodOctree<bool>::Node *node, unsigned int lod_index) {
				Lod &lod = self->_lods[lod_index];
				Vector3i bpos = node->position;
				VoxelBlock *block = lod.map->get_block(bpos);
				CRASH_COND(block == nullptr);
				CRASH_COND(!block->has_been_meshed()); // Never show a block that hasn't been meshed
				block->set_visible(true);
				return true;
			}
		};

		struct UnsubdivideAction {
			VoxelLodTerrain *self;
			unsigned int blocked_count = 0;

			bool can_do(LodOctree<bool>::Node *node, unsigned int lod_index) {
				// Can only unsubdivide if the parent mesh is ready
				const Vector3i &bpos = node->position;
				bool can = self->check_block_loaded_and_updated(bpos, lod_index);
				if (!can) {
					++blocked_count;
				}
				return can;
			}

			void operator()(LodOctree<bool>::Node *node, unsigned int lod_index) {
				Lod &lod = self->_lods[lod_index];
				const Vector3i &bpos = node->position;
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

		// Ideally, this stat should stabilize to zero.
		// If not, something in block management prevents LODs to properly show up and should be fixed.
		_stats.blocked_lods = subdivide_action.blocked_count + unsubdivide_action.blocked_count;
	}

	// Send block loading requests
	{
		VoxelDataLoader::Input input;
		input.priority_position = viewer_block_pos;
		input.priority_direction = viewer_direction;
		input.use_exclusive_region = true;
		input.exclusive_region_extent = get_block_region_extent();

		for (unsigned int lod_index = 0; lod_index < get_lod_count(); ++lod_index) {
			Lod &lod = _lods[lod_index];

			for (unsigned int i = 0; i < lod.blocks_to_load.size(); ++i) {
				VoxelDataLoader::InputBlock input_block;
				input_block.position = lod.blocks_to_load[i];
				input_block.lod = lod_index;
				input.blocks.push_back(input_block);
			}

			lod.blocks_to_load.clear();
		}

		//print_line(String("Sending {0}").format(varray(input.blocks_to_emerge.size())));
		_stream_thread->push(input);
	}

	_stats.time_request_blocks_to_load = profiling_clock.restart();

	// Get block loading responses
	// Note: if block loading is too fast, this can cause stutters.
	// It should only happen on first load, though.
	{
		VoxelDataLoader::Output output;
		_stream_thread->pop(output);
		_stats.stream = output.stats;

		//print_line(String("Loaded {0} blocks").format(varray(output.emerged_blocks.size())));

		for (int i = 0; i < output.blocks.size(); ++i) {

			const VoxelDataLoader::OutputBlock &ob = output.blocks[i];

			if (ob.lod >= get_lod_count()) {
				// That block was requested at a time where LOD was higher... drop it
				++_stats.dropped_block_loads;
				continue;
			}

			Lod &lod = _lods[ob.lod];

			Set<Vector3i>::Element *E = lod.loading_blocks.find(ob.position);
			if (E == nullptr) {
				// That block was not requested, or is no longer needed. drop it...
				++_stats.dropped_block_loads;
				continue;
			}

			lod.loading_blocks.erase(E);

			if (ob.drop_hint) {
				// That block was dropped by the data loader thread, but we were still expecting it...
				// This is not good, because it means the loader is out of sync due to a bug.
				// We can recover with the removal from `loading_blocks` so it will be re-queried again later...
				print_line(String("Received a block loading drop while we were still expecting it: lod{0} ({1}, {2}, {3})")
								   .format(varray(ob.lod, ob.position.x, ob.position.y, ob.position.z)));
#ifdef TOOLS_ENABLED
				lod.debug_unexpected_load_drop_time[ob.position] = OS::get_singleton()->get_ticks_msec();
#endif
				++_stats.dropped_block_loads;
				continue;
			}

			if (ob.data.voxels_loaded->get_size() != lod.map->get_block_size()) {
				// Voxel block size is incorrect, drop it
				ERR_PRINT("Block size obtained from stream is different from expected size");
				++_stats.dropped_block_loads;
				continue;
			}

			// Store buffer
			VoxelBlock *block = lod.map->set_block_buffer(ob.position, ob.data.voxels_loaded);
			//print_line(String("Adding block {0} at lod {1}").format(varray(eo.block_position.to_vec3(), eo.lod)));
			// The block will be made visible and meshed only by LodOctree
			block->set_visible(false);
		}
	}

	_stats.time_process_load_responses = profiling_clock.restart();

	// Send mesh updates
	{
		VoxelMeshUpdater::Input input;
		input.priority_position = viewer_block_pos;
		input.priority_direction = viewer_direction;
		input.use_exclusive_region = true;
		input.exclusive_region_extent = get_block_region_extent();

		for (unsigned int lod_index = 0; lod_index < get_lod_count(); ++lod_index) {
			Lod &lod = _lods[lod_index];

			for (unsigned int i = 0; i < lod.blocks_pending_update.size(); ++i) {
				Vector3i block_pos = lod.blocks_pending_update[i];

				VoxelBlock *block = lod.map->get_block(block_pos);
				CRASH_COND(block == nullptr);
				CRASH_COND(block->get_mesh_state() != VoxelBlock::MESH_UPDATE_NOT_SENT);

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
				iblock.data.voxels = nbuffer;
				iblock.position = block_pos;
				iblock.lod = lod_index;
				input.blocks.push_back(iblock);

				block->set_mesh_state(VoxelBlock::MESH_UPDATE_SENT);
			}

			lod.blocks_pending_update.clear();
		}

		//print_line(String("Sending {0} updates").format(varray(input.blocks.size())));
		_block_updater->push(input);
	}

	_stats.time_request_blocks_to_update = profiling_clock.restart();

	// Receive mesh updates
	{
		{
			VoxelMeshUpdater::Output output;
			_block_updater->pop(output);
			_stats.updater = output.stats;

			for (int i = 0; i < output.blocks.size(); ++i) {
				const VoxelMeshUpdater::OutputBlock &ob = output.blocks[i];

				if (ob.lod >= get_lod_count()) {
					// Sorry, LOD configuration changed, drop that mesh
					++_stats.dropped_block_meshs;
					continue;
				}

				_blocks_pending_main_thread_update.push_back(ob);
			}
		}

		Ref<World> world = get_world();
		uint32_t timeout = os.get_ticks_msec() + MAIN_THREAD_MESHING_BUDGET_MS; // Allocate milliseconds max to upload meshes

		unsigned int queue_index = 0;

		// The following is done on the main thread because Godot doesn't really support multithreaded Mesh allocation.
		// This also proved to be very slow compared to the meshing process itself...
		// hopefully Vulkan will allow us to upload graphical resources without stalling rendering as they upload?

		for (; queue_index < _blocks_pending_main_thread_update.size() && os.get_ticks_msec() < timeout; ++queue_index) {

			const VoxelMeshUpdater::OutputBlock &ob = _blocks_pending_main_thread_update[queue_index];

			if (ob.lod >= get_lod_count()) {
				// Sorry, LOD configuration changed, drop that mesh
				++_stats.dropped_block_meshs;
				continue;
			}

			Lod &lod = _lods[ob.lod];

			VoxelBlock *block = lod.map->get_block(ob.position);
			if (block == NULL) {
				// That block is no longer loaded, drop the result
				++_stats.dropped_block_meshs;
				continue;
			}

			if (ob.drop_hint) {
				// That block is loaded, but its meshing request was dropped.
				// TODO Not sure what to do in this case, the code sending update queries has to be tweaked
				print_line("Received a block mesh drop while we were still expecting it");
				++_stats.dropped_block_meshs;
				continue;
			}

			if (block->get_mesh_state() == VoxelBlock::MESH_UPDATE_SENT) {
				block->set_mesh_state(VoxelBlock::MESH_UP_TO_DATE);
			}

			Ref<ArrayMesh> mesh;
			mesh.instance();

			unsigned int surface_index = 0;
			const VoxelMeshUpdater::OutputBlockData &data = ob.data;
			for (int i = 0; i < data.smooth_surfaces.surfaces.size(); ++i) {

				Array surface = data.smooth_surfaces.surfaces[i];
				if (surface.empty()) {
					continue;
				}

				CRASH_COND(surface.size() != Mesh::ARRAY_MAX);
				mesh->add_surface_from_arrays(data.smooth_surfaces.primitive_type, surface);
				mesh->surface_set_material(surface_index, _material);
				// No multi-material supported yet
				++surface_index;
			}

			if (is_mesh_empty(mesh)) {
				mesh = Ref<Mesh>();
			}

			block->set_mesh(mesh, world);
			block->mark_been_meshed();
		}

		shift_up(_blocks_pending_main_thread_update, queue_index);
	}

	_stats.time_process_update_responses = profiling_clock.restart();

	_stats.time_process_lod = profiling_clock.restart();
}

Dictionary VoxelLodTerrain::get_stats() const {

	Dictionary process;
	process["time_request_blocks_to_load"] = _stats.time_request_blocks_to_load;
	process["time_process_load_responses"] = _stats.time_process_load_responses;
	process["time_request_blocks_to_update"] = _stats.time_request_blocks_to_update;
	process["time_process_update_responses"] = _stats.time_process_update_responses;
	process["time_process_lod"] = _stats.time_process_lod;

	Dictionary d;
	d["stream"] = VoxelDataLoader::Mgr::to_dictionary(_stats.stream);
	d["updater"] = VoxelMeshUpdater::Mgr::to_dictionary(_stats.updater);
	d["process"] = process;
	d["blocked_lods"] = _stats.blocked_lods;
	d["dropped_block_loads"] = _stats.dropped_block_loads;
	d["dropped_block_meshs"] = _stats.dropped_block_meshs;

	return d;
}

void VoxelLodTerrain::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_stream", "stream"), &VoxelLodTerrain::set_stream);
	ClassDB::bind_method(D_METHOD("get_stream"), &VoxelLodTerrain::get_stream);

	ClassDB::bind_method(D_METHOD("set_material", "material"), &VoxelLodTerrain::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &VoxelLodTerrain::get_material);

	ClassDB::bind_method(D_METHOD("set_view_distance", "distance_in_voxels"), &VoxelLodTerrain::set_view_distance);
	ClassDB::bind_method(D_METHOD("get_view_distance"), &VoxelLodTerrain::get_view_distance);

	ClassDB::bind_method(D_METHOD("get_viewer_path"), &VoxelLodTerrain::get_viewer_path);
	ClassDB::bind_method(D_METHOD("set_viewer_path", "path"), &VoxelLodTerrain::set_viewer_path);

	ClassDB::bind_method(D_METHOD("set_lod_count", "lod_count"), &VoxelLodTerrain::set_lod_count);
	ClassDB::bind_method(D_METHOD("get_lod_count"), &VoxelLodTerrain::get_lod_count);

	ClassDB::bind_method(D_METHOD("set_lod_split_scale", "lod_split_scale"), &VoxelLodTerrain::set_lod_split_scale);
	ClassDB::bind_method(D_METHOD("get_lod_split_scale"), &VoxelLodTerrain::get_lod_split_scale);

	ClassDB::bind_method(D_METHOD("get_block_region_extent"), &VoxelLodTerrain::get_block_region_extent);
	ClassDB::bind_method(D_METHOD("get_block_info", "block_pos", "lod"), &VoxelLodTerrain::get_block_info);
	ClassDB::bind_method(D_METHOD("get_stats"), &VoxelLodTerrain::get_stats);
	ClassDB::bind_method(D_METHOD("voxel_to_block_position", "lod_index"), &VoxelLodTerrain::voxel_to_block_position);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "stream", PROPERTY_HINT_RESOURCE_TYPE, "VoxelStream"), "set_stream", "get_stream");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "view_distance"), "set_view_distance", "get_view_distance");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod_count"), "set_lod_count", "get_lod_count");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "lod_split_scale"), "set_lod_split_scale", "get_lod_split_scale");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "viewer_path"), "set_viewer_path", "get_viewer_path");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_material", "get_material");
}
