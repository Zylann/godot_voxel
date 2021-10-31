#include "voxel_lod_terrain.h"
#include "../constants/voxel_string_names.h"
#include "../edition/voxel_tool_lod_terrain.h"
#include "../meshers/transvoxel/voxel_mesher_transvoxel.h"
#include "../server/voxel_server.h"
#include "../util/funcs.h"
#include "../util/godot/funcs.h"
#include "../util/macros.h"
#include "../util/profiling.h"
#include "../util/profiling_clock.h"
#include "instancing/voxel_instancer.h"

#include <core/core_string_names.h>
#include <core/engine.h>
#include <scene/3d/mesh_instance.h>
#include <scene/resources/packed_scene.h>

namespace {

Ref<ArrayMesh> build_mesh(const Vector<Array> surfaces, Mesh::PrimitiveType primitive, int compression_flags,
		Ref<Material> material) {
	VOXEL_PROFILE_SCOPE();
	Ref<ArrayMesh> mesh;
	mesh.instance();

	unsigned int surface_index = 0;
	for (int i = 0; i < surfaces.size(); ++i) {
		Array surface = surfaces[i];

		if (surface.empty()) {
			continue;
		}

		CRASH_COND(surface.size() != Mesh::ARRAY_MAX);
		if (!is_surface_triangulated(surface)) {
			continue;
		}

		// TODO Use `add_surface`, it's about 20% faster after measuring in Tracy (though we may see if Godot 4 expects the same)
		mesh->add_surface_from_arrays(primitive, surface, Array(), compression_flags);
		mesh->surface_set_material(surface_index, material);
		// No multi-material supported yet
		++surface_index;
	}

	// Debug code to highlight vertex sharing
	/*if (mesh->get_surface_count() > 0) {
		Array wireframe_surface = generate_debug_seams_wireframe_surface(mesh, 0);
		if (wireframe_surface.size() > 0) {
			const int wireframe_surface_index = mesh->get_surface_count();
			mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, wireframe_surface);
			Ref<SpatialMaterial> line_material;
			line_material.instance();
			line_material->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
			line_material->set_albedo(Color(1.0, 0.0, 1.0));
			mesh->surface_set_material(wireframe_surface_index, line_material);
		}
	}*/

	if (is_mesh_empty(mesh)) {
		mesh = Ref<Mesh>();
	}

	return mesh;
}

// To use on loaded blocks
static inline void schedule_mesh_update(VoxelMeshBlock *block, std::vector<Vector3i> &blocks_pending_update) {
	if (block->get_mesh_state() != VoxelMeshBlock::MESH_UPDATE_NOT_SENT) {
		if (block->active) {
			// Schedule an update
			block->set_mesh_state(VoxelMeshBlock::MESH_UPDATE_NOT_SENT);
			blocks_pending_update.push_back(block->position);
		} else {
			// Just mark it as needing update, so the visibility system will schedule its update when needed
			block->set_mesh_state(VoxelMeshBlock::MESH_NEED_UPDATE);
		}
	}
}

struct BeforeUnloadDataAction {
	std::vector<VoxelLodTerrain::BlockToSave> &blocks_to_save;
	bool save;

	void operator()(VoxelDataBlock *block) {
		// Save if modified
		// TODO Don't ask for save if the stream doesn't support it!
		if (save && block->is_modified()) {
			//print_line(String("Scheduling save for block {0}").format(varray(block->position.to_vec3())));
			VoxelLodTerrain::BlockToSave b;
			// We don't copy since the block will be unloaded anyways
			b.voxels = block->get_voxels_shared();
			b.position = block->position;
			b.lod = block->lod_index;
			blocks_to_save.push_back(b);
		}
	}
};

struct BeforeUnloadMeshAction {
	std::vector<Ref<ShaderMaterial>> &shader_material_pool;

	void operator()(VoxelMeshBlock *block) {
		VOXEL_PROFILE_SCOPE_NAMED("Recycle material");
		// Recycle material
		Ref<ShaderMaterial> sm = block->get_shader_material();
		if (sm.is_valid()) {
			shader_material_pool.push_back(sm);
			block->set_shader_material(Ref<ShaderMaterial>());
		}
	}
};

struct ScheduleSaveAction {
	std::vector<VoxelLodTerrain::BlockToSave> &blocks_to_save;

	void operator()(VoxelDataBlock *block) {
		// Save if modified
		// TODO Don't ask for save if the stream doesn't support it!
		if (block->is_modified()) {
			//print_line(String("Scheduling save for block {0}").format(varray(block->position.to_vec3())));
			VoxelLodTerrain::BlockToSave b;

			b.voxels = gd_make_shared<VoxelBufferInternal>();
			{
				RWLockRead lock(block->get_voxels().get_lock());
				block->get_voxels_const().duplicate_to(*b.voxels, true);
			}

			b.position = block->position;
			b.lod = block->lod_index;
			blocks_to_save.push_back(b);
			block->set_modified(false);
		}
	}
};

static inline uint64_t get_ticks_msec() {
	return OS::get_singleton()->get_ticks_msec();
}

} // namespace

VoxelLodTerrain::VoxelLodTerrain() {
	// Note: don't do anything heavy in the constructor.
	// Godot may create and destroy dozens of instances of all node types on startup,
	// due to how ClassDB gets its default values.

	PRINT_VERBOSE("Construct VoxelLodTerrain");

	set_notify_transform(true);

	// Doing this to setup the defaults
	set_process_mode(_process_mode);

	// Infinite by default
	_bounds_in_voxels = Box3i::from_center_extents(Vector3i(0), Vector3i(VoxelConstants::MAX_VOLUME_EXTENT));

	struct ApplyMeshUpdateTask : public IVoxelTimeSpreadTask {
		void run() override {
			if (!VoxelServer::get_singleton()->is_volume_valid(volume_id)) {
				// The node can have been destroyed while this task was still pending
				PRINT_VERBOSE("Cancelling ApplyMeshUpdateTask, volume_id is invalid");
				return;
			}
			self->apply_mesh_update(data);
		}
		uint32_t volume_id = 0;
		VoxelLodTerrain *self = nullptr;
		VoxelServer::BlockMeshOutput data;
	};

	// Mesh updates are spread over frames by scheduling them in a task runner of VoxelServer,
	// but instead of using a reception buffer we use a callback,
	// because this kind of task scheduling would otherwise delay the update by 1 frame
	_reception_buffers.callback_data = this;
	_reception_buffers.mesh_output_callback = [](void *cb_data, const VoxelServer::BlockMeshOutput &ob) {
		VoxelLodTerrain *self = reinterpret_cast<VoxelLodTerrain *>(cb_data);
		ApplyMeshUpdateTask *task = memnew(ApplyMeshUpdateTask);
		task->volume_id = self->get_volume_id();
		task->self = self;
		task->data = ob;
		VoxelServer::get_singleton()->push_time_spread_task(task);
	};

	_volume_id = VoxelServer::get_singleton()->add_volume(&_reception_buffers, VoxelServer::VOLUME_SPARSE_OCTREE);
	VoxelServer::get_singleton()->set_volume_octree_lod_distance(_volume_id, get_lod_distance());

	// TODO Being able to set a LOD smaller than the stream is probably a bad idea,
	// Because it prevents edits from propagating up to the last one, they will be left out of sync
	set_lod_count(4);

	set_lod_distance(48.f);

	// For ease of use in editor
	Ref<VoxelMesherTransvoxel> default_mesher;
	default_mesher.instance();
	_mesher = default_mesher;
}

VoxelLodTerrain::~VoxelLodTerrain() {
	PRINT_VERBOSE("Destroy VoxelLodTerrain");
	VoxelServer::get_singleton()->remove_volume(_volume_id);
	// Instancer can take care of itself
}

Ref<Material> VoxelLodTerrain::get_material() const {
	return _material;
}

void VoxelLodTerrain::set_material(Ref<Material> p_material) {
	_material = p_material;
}

unsigned int VoxelLodTerrain::get_data_block_size() const {
	return _lods[0].data_map.get_block_size();
}

unsigned int VoxelLodTerrain::get_data_block_size_pow2() const {
	return _lods[0].data_map.get_block_size_pow2();
}

unsigned int VoxelLodTerrain::get_mesh_block_size_pow2() const {
	return _lods[0].mesh_map.get_block_size_pow2();
}

unsigned int VoxelLodTerrain::get_mesh_block_size() const {
	return _lods[0].mesh_map.get_block_size();
}

void VoxelLodTerrain::set_stream(Ref<VoxelStream> p_stream) {
	if (p_stream == _stream) {
		return;
	}

	_stream = p_stream;

#ifdef TOOLS_ENABLED
	if (_stream.is_valid()) {
		if (Engine::get_singleton()->is_editor_hint()) {
			Ref<Script> script = _stream->get_script();
			if (script.is_valid()) {
				// Safety check. It's too easy to break threads by making a script reload.
				// You can turn it back on, but be careful.
				_run_stream_in_editor = false;
				_change_notify();
			}
		}
	}
#endif

	_on_stream_params_changed();
}

Ref<VoxelStream> VoxelLodTerrain::get_stream() const {
	return _stream;
}

void VoxelLodTerrain::set_generator(Ref<VoxelGenerator> p_generator) {
	if (p_generator == _generator) {
		return;
	}

	_generator = p_generator;

#ifdef TOOLS_ENABLED
	if (_generator.is_valid()) {
		if (Engine::get_singleton()->is_editor_hint()) {
			Ref<Script> script = _generator->get_script();
			if (script.is_valid()) {
				// Safety check. It's too easy to break threads by making a script reload.
				// You can turn it back on, but be careful.
				_run_stream_in_editor = false;
				_change_notify();
			}
		}
	}
#endif

	_on_stream_params_changed();
}

Ref<VoxelGenerator> VoxelLodTerrain::get_generator() const {
	return _generator;
}

void VoxelLodTerrain::set_mesher(Ref<VoxelMesher> p_mesher) {
	if (_mesher == p_mesher) {
		return;
	}

	stop_updater();

	_mesher = p_mesher;

	if (_mesher.is_valid()) {
		start_updater();
		remesh_all_blocks();
	}

	update_configuration_warning();
}

Ref<VoxelMesher> VoxelLodTerrain::get_mesher() const {
	return _mesher;
}

void VoxelLodTerrain::_on_stream_params_changed() {
	stop_streamer();
	stop_updater();

	if (_stream.is_valid()) {
		const int stream_block_size_po2 = _stream->get_block_size_po2();
		_set_block_size_po2(stream_block_size_po2);

		// TODO We have to figure out streams that have a LOD requirement
		// const int stream_lod_count = _stream->get_lod_count();
		// _set_lod_count(min(stream_lod_count, get_lod_count()));
	}

	VoxelServer::get_singleton()->set_volume_data_block_size(_volume_id, get_data_block_size());
	VoxelServer::get_singleton()->set_volume_render_block_size(_volume_id, get_mesh_block_size());

	reset_maps();

	if ((_stream.is_valid() || _generator.is_valid()) &&
			(Engine::get_singleton()->is_editor_hint() == false || _run_stream_in_editor)) {
		start_streamer();
		start_updater();
	}

	// The whole map might change, so make all area dirty
	for (unsigned int i = 0; i < _lod_count; ++i) {
		Lod &lod = _lods[i];
		lod.last_view_distance_data_blocks = 0;
		lod.last_view_distance_mesh_blocks = 0;
	}

	update_configuration_warning();
}

/*void VoxelLodTerrain::set_data_block_size_po2(unsigned int p_block_size_po2) {
	ERR_FAIL_COND(p_block_size_po2 < 1);
	ERR_FAIL_COND(p_block_size_po2 > 32);

	unsigned int block_size_po2 = p_block_size_po2;
	if (_stream.is_valid()) {
		block_size_po2 = _stream->get_block_size_po2();
	}

	if (block_size_po2 == get_data_block_size_pow2()) {
		return;
	}

	_on_stream_params_changed();
}*/

void VoxelLodTerrain::set_mesh_block_size(unsigned int mesh_block_size) {
	// Mesh block size cannot be smaller than data block size, for now
	mesh_block_size = clamp(mesh_block_size, get_data_block_size(), VoxelConstants::MAX_BLOCK_SIZE);

	unsigned int po2;
	switch (mesh_block_size) {
		case 16:
			po2 = 4;
			break;
		case 32:
			po2 = 5;
			break;
		default:
			mesh_block_size = 16;
			po2 = 4;
			break;
	}
	if (mesh_block_size == get_mesh_block_size()) {
		return;
	}

	// Reset mesh maps
	for (unsigned int lod_index = 0; lod_index < _lod_count; ++lod_index) {
		Lod &lod = _lods[lod_index];
		if (_instancer != nullptr) {
			// Unload instances
			VoxelInstancer *instancer = _instancer;
			lod.mesh_map.for_all_blocks([lod_index, instancer](VoxelMeshBlock *block) {
				instancer->on_mesh_block_exit(block->position, lod_index);
			});
		}
		// Unload mesh blocks
		lod.mesh_map.for_all_blocks(BeforeUnloadMeshAction{ _shader_material_pool });
		lod.mesh_map.create(po2, lod_index);
		// Reset view distance cache so they will be re-entered
		lod.last_view_distance_mesh_blocks = 0;
	}

	// Reset LOD octrees
	LodOctree::NoDestroyAction nda;
	for (Map<Vector3i, OctreeItem>::Element *E = _lod_octrees.front(); E; E = E->next()) {
		OctreeItem &item = E->value();
		item.octree.create_from_lod_count(get_mesh_block_size(), _lod_count, nda);
	}

	VoxelServer::get_singleton()->set_volume_render_block_size(_volume_id, mesh_block_size);

	// Update voxel bounds because block size change can affect octree size
	set_voxel_bounds(_bounds_in_voxels);
}

void VoxelLodTerrain::_set_block_size_po2(int p_block_size_po2) {
	_lods[0].data_map.create(p_block_size_po2, 0);
}

void VoxelLodTerrain::set_mesh_block_active(VoxelMeshBlock &block, bool active) {
	if (block.active == active) {
		return;
	}

	block.active = active;

	if (_lod_fade_duration == 0.f) {
		block.set_visible(active);
		return;
	}

	VoxelMeshBlock::FadingState fading_state;
	// Initial progress has to be set too because it sometimes happens that a LOD must appear before its parent
	// finished fading in. So the parent will have to fade out from solid with the same duration.
	float initial_progress;
	if (active) {
		block.set_visible(true);
		fading_state = VoxelMeshBlock::FADING_IN;
		initial_progress = 0.f;
	} else {
		fading_state = VoxelMeshBlock::FADING_OUT;
		initial_progress = 1.f;
	}

	if (block.fading_state != fading_state) {
		if (block.fading_state == VoxelMeshBlock::FADING_NONE) {
			Lod &lod = _lods[block.lod_index];
			// Must not have duplicates
			ERR_FAIL_COND(lod.fading_blocks.has(block.position));
			lod.fading_blocks.insert(block.position, &block);
		}
		block.fading_state = fading_state;
		block.fading_progress = initial_progress;
	}
}

inline int get_octree_size_po2(const VoxelLodTerrain &self) {
	return self.get_mesh_block_size_pow2() + self.get_lod_count() - 1;
}

bool VoxelLodTerrain::is_area_editable(Box3i p_voxel_box) const {
	const Box3i voxel_box = p_voxel_box.clipped(_bounds_in_voxels);
	const Lod &lod0 = _lods[0];
	const bool all_blocks_present = lod0.data_map.is_area_fully_loaded(voxel_box);
	return all_blocks_present;
}

uint64_t VoxelLodTerrain::get_voxel(Vector3i pos, unsigned int channel, uint64_t defval) const {
	Vector3i block_pos = pos >> get_data_block_size_pow2();
	for (unsigned int lod_index = 0; lod_index < _lod_count; ++lod_index) {
		const Lod &lod = _lods[lod_index];
		const VoxelDataBlock *block = lod.data_map.get_block(block_pos);
		if (block != nullptr) {
			return lod.data_map.get_voxel(pos, channel);
		}
		// Fallback on lower LOD
		block_pos = block_pos >> 1;
	}
	return defval;
}

bool VoxelLodTerrain::try_set_voxel_without_update(Vector3i pos, unsigned int channel, uint64_t value) {
	const Vector3i block_pos_lod0 = pos >> get_data_block_size_pow2();
	Lod &lod0 = _lods[0];
	VoxelDataBlock *block = lod0.data_map.get_block(block_pos_lod0);
	if (block != nullptr) {
		lod0.data_map.set_voxel(value, pos, channel);
		return true;
	} else {
		return false;
	}
}

void VoxelLodTerrain::copy(Vector3i p_origin_voxels, VoxelBufferInternal &dst_buffer, uint8_t channels_mask) const {
	const Lod &lod0 = _lods[0];
	lod0.data_map.copy(p_origin_voxels, dst_buffer, channels_mask);
}

// Marks intersecting blocks in the area as modified, updates LODs and schedules remeshing.
// The provided box must be at LOD0 coordinates.
void VoxelLodTerrain::post_edit_area(Box3i p_box) {
	// TODO Better decoupling is needed here.
	// In the past this padding was necessary for mesh blocks because visuals depend on neighbor voxels.
	// So when editing voxels at the boundary of two mesh blocks, both must update.
	// However on data blocks it doesn't make sense, neighbors are not affected (at least for now).
	// this can cause false positive errors as if we were editing a block that's not loaded (coming up as null).
	// For now, this is worked around by ignoring cases where blocks are null,
	// But it might mip more lods than necessary when editing on borders.
	const Box3i box = p_box.padded(1);
	const Box3i bbox = box.downscaled(get_data_block_size());

	bbox.for_each_cell([this](Vector3i block_pos_lod0) {
		Lod &lod0 = _lods[0];
		VoxelDataBlock *block = lod0.data_map.get_block(block_pos_lod0);
		//ERR_FAIL_COND(block == nullptr);
		if (block == nullptr) {
			return;
		}

		block->set_modified(true);

		if (!block->get_needs_lodding()) {
			block->set_needs_lodding(true);
			lod0.blocks_pending_lodding.push_back(block_pos_lod0);
		}
	});

	if (_instancer != nullptr) {
		_instancer->on_area_edited(p_box);
	}
}

Ref<VoxelTool> VoxelLodTerrain::get_voxel_tool() {
	VoxelToolLodTerrain *vt = memnew(VoxelToolLodTerrain(this));
	// Set to most commonly used channel on this kind of terrain
	vt->set_channel(VoxelBufferInternal::CHANNEL_SDF);
	return Ref<VoxelTool>(vt);
}

int VoxelLodTerrain::get_view_distance() const {
	return _view_distance_voxels;
}

// TODO Needs to be clamped dynamically, to avoid the user accidentally setting blowing up memory.
// It used to be clamped to a hardcoded value, but now it may depend on LOD count and boundaries
void VoxelLodTerrain::set_view_distance(int p_distance_in_voxels) {
	ERR_FAIL_COND(p_distance_in_voxels <= 0);
	// Note: this is a hint distance, the terrain will attempt to have this radius filled with loaded voxels.
	// It is possible for blocks to still load beyond that distance.
	_view_distance_voxels = p_distance_in_voxels;
}

void VoxelLodTerrain::start_updater() {
	Ref<VoxelMesherBlocky> blocky_mesher = _mesher;
	if (blocky_mesher.is_valid()) {
		Ref<VoxelLibrary> library = blocky_mesher->get_library();
		if (library.is_valid()) {
			// TODO Any way to execute this function just after the TRES resource loader has finished to load?
			// VoxelLibrary should be baked ahead of time, like MeshLibrary
			library->bake();
		}
	}

	VoxelServer::get_singleton()->set_volume_mesher(_volume_id, _mesher);
}

void VoxelLodTerrain::stop_updater() {
	struct ResetMeshStateAction {
		void operator()(VoxelMeshBlock *block) {
			if (block->get_mesh_state() == VoxelMeshBlock::MESH_UPDATE_SENT) {
				block->set_mesh_state(VoxelMeshBlock::MESH_UPDATE_NOT_SENT);
			}
		}
	};

	VoxelServer::get_singleton()->set_volume_mesher(_volume_id, Ref<VoxelMesher>());

	// TODO We can still receive a few mesh delayed mesh updates after this. Is it a problem?
	//_reception_buffers.mesh_output.clear();

	for (unsigned int i = 0; i < _lods.size(); ++i) {
		Lod &lod = _lods[i];
		lod.blocks_pending_update.clear();

		ResetMeshStateAction a;
		lod.mesh_map.for_all_blocks(a);
	}
}

void VoxelLodTerrain::start_streamer() {
	VoxelServer::get_singleton()->set_volume_stream(_volume_id, _stream);
	VoxelServer::get_singleton()->set_volume_generator(_volume_id, _generator);
}

void VoxelLodTerrain::stop_streamer() {
	VoxelServer::get_singleton()->set_volume_stream(_volume_id, Ref<VoxelStream>());
	VoxelServer::get_singleton()->set_volume_generator(_volume_id, Ref<VoxelGenerator>());

	for (unsigned int i = 0; i < _lods.size(); ++i) {
		Lod &lod = _lods[i];
		lod.loading_blocks.clear();
		lod.blocks_to_load.clear();
	}

	_reception_buffers.data_output.clear();
}

void VoxelLodTerrain::set_lod_distance(float p_lod_distance) {
	if (p_lod_distance == _lod_distance) {
		return;
	}

	_lod_distance = clamp(p_lod_distance, VoxelConstants::MINIMUM_LOD_DISTANCE, VoxelConstants::MAXIMUM_LOD_DISTANCE);

	for (Map<Vector3i, OctreeItem>::Element *E = _lod_octrees.front(); E; E = E->next()) {
		OctreeItem &item = E->value();
		item.octree.set_lod_distance(_lod_distance);

		// Because `set_lod_distance` may clamp it...
		_lod_distance = item.octree.get_lod_distance();
	}

	VoxelServer::get_singleton()->set_volume_octree_lod_distance(_volume_id, get_lod_distance());
}

float VoxelLodTerrain::get_lod_distance() const {
	return _lod_distance;
}

void VoxelLodTerrain::set_lod_count(int p_lod_count) {
	ERR_FAIL_COND(p_lod_count >= (int)VoxelConstants::MAX_LOD);
	ERR_FAIL_COND(p_lod_count < 1);

	if (get_lod_count() != p_lod_count) {
		_set_lod_count(p_lod_count);
	}
}

void VoxelLodTerrain::_set_lod_count(int p_lod_count) {
	CRASH_COND(p_lod_count >= (int)VoxelConstants::MAX_LOD);
	CRASH_COND(p_lod_count < 1);

	_lod_count = p_lod_count;

	LodOctree::NoDestroyAction nda;

	for (Map<Vector3i, OctreeItem>::Element *E = _lod_octrees.front(); E; E = E->next()) {
		OctreeItem &item = E->value();
		item.octree.create_from_lod_count(get_mesh_block_size(), p_lod_count, nda);
	}

	// Not entirely required, but changing LOD count at runtime is rarely needed
	reset_maps();
}

void VoxelLodTerrain::reset_maps() {
	// Clears all blocks and reconfigures maps to account for new LOD count and block sizes

	// Don't reset while streaming, the result can be dirty?
	//CRASH_COND(_stream_thread != nullptr);

	for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		Lod &lod = _lods[lod_index];

		// Instance new maps if we have more lods, or clear them otherwise
		if (lod_index < _lod_count) {
			lod.data_map.create(lod.data_map.get_block_size_pow2(), lod_index);
			lod.mesh_map.create(lod.mesh_map.get_block_size_pow2(), lod_index);
			lod.blocks_to_load.clear();
			lod.last_view_distance_data_blocks = 0;
			lod.last_view_distance_mesh_blocks = 0;

		} else {
			lod.data_map.clear();
			lod.mesh_map.clear();
		}

		lod.deferred_collision_updates.clear();
	}

	// Reset previous state caches to force rebuilding the view area
	_last_octree_region_box = Box3i();
	_lod_octrees.clear();
}

int VoxelLodTerrain::get_lod_count() const {
	return _lod_count;
}

void VoxelLodTerrain::set_generate_collisions(bool enabled) {
	_generate_collisions = enabled;
}

void VoxelLodTerrain::set_collision_lod_count(int lod_count) {
	ERR_FAIL_COND(lod_count < 0);
	_collision_lod_count = static_cast<unsigned int>(min(lod_count, get_lod_count()));
}

int VoxelLodTerrain::get_collision_lod_count() const {
	return _collision_lod_count;
}

void VoxelLodTerrain::set_collision_layer(int layer) {
	_collision_layer = layer;
	for (unsigned int lod_index = 0; lod_index < _lod_count; ++lod_index) {
		_lods[lod_index].mesh_map.for_all_blocks([layer](VoxelMeshBlock *block) {
			block->set_collision_layer(layer);
		});
	}
}

int VoxelLodTerrain::get_collision_layer() const {
	return _collision_layer;
}

void VoxelLodTerrain::set_collision_mask(int mask) {
	_collision_mask = mask;
	for (unsigned int lod_index = 0; lod_index < _lod_count; ++lod_index) {
		_lods[lod_index].mesh_map.for_all_blocks([mask](VoxelMeshBlock *block) {
			block->set_collision_mask(mask);
		});
	}
}

int VoxelLodTerrain::get_collision_mask() const {
	return _collision_mask;
}

void VoxelLodTerrain::set_collision_margin(float margin) {
	_collision_margin = margin;
	for (unsigned int lod_index = 0; lod_index < _lod_count; ++lod_index) {
		_lods[lod_index].mesh_map.for_all_blocks([margin](VoxelMeshBlock *block) {
			block->set_collision_margin(margin);
		});
	}
}

float VoxelLodTerrain::get_collision_margin() const {
	return _collision_margin;
}

int VoxelLodTerrain::get_data_block_region_extent() const {
	return VoxelServer::get_octree_lod_block_region_extent(_lod_distance, get_data_block_size());
}

int VoxelLodTerrain::get_mesh_block_region_extent() const {
	return VoxelServer::get_octree_lod_block_region_extent(_lod_distance, get_mesh_block_size());
}

Vector3 VoxelLodTerrain::voxel_to_data_block_position(Vector3 vpos, int lod_index) const {
	ERR_FAIL_COND_V(lod_index < 0, Vector3());
	ERR_FAIL_COND_V(lod_index >= get_lod_count(), Vector3());
	const Lod &lod = _lods[lod_index];
	Vector3i bpos = lod.data_map.voxel_to_block(Vector3i::from_floored(vpos)) >> lod_index;
	return bpos.to_vec3();
}

Vector3 VoxelLodTerrain::voxel_to_mesh_block_position(Vector3 vpos, int lod_index) const {
	ERR_FAIL_COND_V(lod_index < 0, Vector3());
	ERR_FAIL_COND_V(lod_index >= get_lod_count(), Vector3());
	const Lod &lod = _lods[lod_index];
	Vector3i bpos = lod.mesh_map.voxel_to_block(Vector3i::from_floored(vpos)) >> lod_index;
	return bpos.to_vec3();
}

void VoxelLodTerrain::set_process_mode(ProcessMode mode) {
	_process_mode = mode;
	set_process(_process_mode == PROCESS_MODE_IDLE);
	set_physics_process(_process_mode == PROCESS_MODE_PHYSICS);
}

void VoxelLodTerrain::_notification(int p_what) {
	switch (p_what) {
		// TODO Should use NOTIFICATION_INTERNAL_PROCESS instead?
		case NOTIFICATION_PROCESS:
			if (_process_mode == PROCESS_MODE_IDLE) {
				// Can't do that in enter tree because Godot is "still setting up children".
				// Can't do that in ready either because Godot says node state is locked.
				// This hack is quite miserable.
				VoxelServerUpdater::ensure_existence(get_tree());
				_process(get_process_delta_time());
			}
			break;

		// TODO Should use NOTIFICATION_INTERNAL_PHYSICS_PROCESS instead?
		case NOTIFICATION_PHYSICS_PROCESS:
			if (_process_mode == PROCESS_MODE_PHYSICS) {
				// Can't do that in enter tree because Godot is "still setting up children".
				// Can't do that in ready either because Godot says node state is locked.
				// This hack is quite miserable.
				VoxelServerUpdater::ensure_existence(get_tree());
				_process(get_physics_process_delta_time());
				break;
			}

		case NOTIFICATION_EXIT_TREE:
			break;

		case NOTIFICATION_ENTER_WORLD: {
			World *world = *get_world();
			for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
				_lods[lod_index].mesh_map.for_all_blocks([world](VoxelMeshBlock *block) {
					block->set_world(world);
				});
			}
#ifdef TOOLS_ENABLED
			if (is_showing_gizmos()) {
				_debug_renderer.set_world(is_visible_in_tree() ? world : nullptr);
			}
#endif
			// DEBUG
			//set_show_gizmos(true);
		} break;

		case NOTIFICATION_EXIT_WORLD: {
			for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
				_lods[lod_index].mesh_map.for_all_blocks([](VoxelMeshBlock *block) {
					block->set_world(nullptr);
				});
			}
#ifdef TOOLS_ENABLED
			_debug_renderer.set_world(nullptr);
#endif
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			const bool visible = is_visible();
			for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
				_lods[lod_index].mesh_map.for_all_blocks([visible](VoxelMeshBlock *block) {
					block->set_parent_visible(visible);
				});
			}
#ifdef TOOLS_ENABLED
			if (is_showing_gizmos()) {
				_debug_renderer.set_world(is_visible_in_tree() ? *get_world() : nullptr);
			}
#endif
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			VOXEL_PROFILE_SCOPE_NAMED("VoxelLodTerrain::NOTIFICATION_TRANSFORM_CHANGED");

			const Transform transform = get_global_transform();
			VoxelServer::get_singleton()->set_volume_transform(_volume_id, transform);

			if (!is_inside_tree()) {
				// The transform and other properties can be set by the scene loader,
				// before we enter the tree
				return;
			}

			for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
				_lods[lod_index].mesh_map.for_all_blocks([&transform](VoxelMeshBlock *block) {
					block->set_parent_transform(transform);
				});
			}
		} break;

		default:
			break;
	}
}

Vector3 VoxelLodTerrain::get_local_viewer_pos() const {
	// Pick this by default
	Vector3 pos = (_lods[0].last_viewer_data_block_pos << _lods[0].data_map.get_block_size_pow2()).to_vec3();

	// TODO Support for multiple viewers, this is a placeholder implementation
	VoxelServer::get_singleton()->for_each_viewer([&pos](const VoxelServer::Viewer &viewer, uint32_t viewer_id) {
		pos = viewer.world_position;
	});

	const Transform world_to_local = get_global_transform().affine_inverse();
	pos = world_to_local.xform(pos);
	return pos;
}

void VoxelLodTerrain::try_schedule_loading_with_neighbors(const Vector3i &p_data_block_pos, int lod_index) {
	Lod &lod = _lods[lod_index];

	const int p = lod.data_map.get_block_size_pow2() + lod_index;

	const int bound_min_x = _bounds_in_voxels.pos.x >> p;
	const int bound_min_y = _bounds_in_voxels.pos.y >> p;
	const int bound_min_z = _bounds_in_voxels.pos.z >> p;
	const int bound_max_x = (_bounds_in_voxels.pos.x + _bounds_in_voxels.size.x) >> p;
	const int bound_max_y = (_bounds_in_voxels.pos.y + _bounds_in_voxels.size.y) >> p;
	const int bound_max_z = (_bounds_in_voxels.pos.z + _bounds_in_voxels.size.z) >> p;

	const int min_x = max(p_data_block_pos.x - 1, bound_min_x);
	const int min_y = max(p_data_block_pos.y - 1, bound_min_y);
	const int min_z = max(p_data_block_pos.z - 1, bound_min_z);
	const int max_x = min(p_data_block_pos.x + 2, bound_max_x);
	const int max_y = min(p_data_block_pos.y + 2, bound_max_y);
	const int max_z = min(p_data_block_pos.z + 2, bound_max_z);

	Vector3i bpos;
	for (bpos.y = min_y; bpos.y < max_y; ++bpos.y) {
		for (bpos.z = min_z; bpos.z < max_z; ++bpos.z) {
			for (bpos.x = min_x; bpos.x < max_x; ++bpos.x) {
				VoxelDataBlock *block = lod.data_map.get_block(bpos);

				if (block == nullptr) {
					if (!lod.has_loading_block(bpos)) {
						lod.blocks_to_load.push_back(bpos);
						lod.loading_blocks.insert(bpos);
					}
				}
			}
		}
	}
}

bool VoxelLodTerrain::is_block_surrounded(const Vector3i &p_bpos, int lod_index, const VoxelDataMap &map) const {
	const int p = map.get_block_size_pow2() + lod_index;

	const int bound_min_x = _bounds_in_voxels.pos.x >> p;
	const int bound_min_y = _bounds_in_voxels.pos.y >> p;
	const int bound_min_z = _bounds_in_voxels.pos.z >> p;
	const int bound_max_x = (_bounds_in_voxels.pos.x + _bounds_in_voxels.size.x) >> p;
	const int bound_max_y = (_bounds_in_voxels.pos.y + _bounds_in_voxels.size.y) >> p;
	const int bound_max_z = (_bounds_in_voxels.pos.z + _bounds_in_voxels.size.z) >> p;

	const int min_x = max(p_bpos.x - 1, bound_min_x);
	const int min_y = max(p_bpos.y - 1, bound_min_y);
	const int min_z = max(p_bpos.z - 1, bound_min_z);
	const int max_x = min(p_bpos.x + 2, bound_max_x);
	const int max_y = min(p_bpos.y + 2, bound_max_y);
	const int max_z = min(p_bpos.z + 2, bound_max_z);

	Vector3i bpos;
	for (bpos.y = min_y; bpos.y < max_y; ++bpos.y) {
		for (bpos.z = min_z; bpos.z < max_z; ++bpos.z) {
			for (bpos.x = min_x; bpos.x < max_x; ++bpos.x) {
				if (bpos != p_bpos && !map.has_block(bpos)) {
					return false;
				}
			}
		}
	}
	return true;
}

bool VoxelLodTerrain::check_block_loaded_and_meshed(const Vector3i &p_mesh_block_pos, int lod_index) {
	Lod &lod = _lods[lod_index];

	const int mesh_block_size = get_mesh_block_size();
	const int data_block_size = get_data_block_size();

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_V(!check_block_sizes(data_block_size, mesh_block_size), false);
#endif

	if (mesh_block_size > data_block_size) {
		const int factor = mesh_block_size / data_block_size;
		const Vector3i data_block_pos0 = p_mesh_block_pos * factor;

		bool loaded = true;

		for (int z = 0; z < factor; ++z) {
			for (int x = 0; x < factor; ++x) {
				for (int y = 0; y < factor; ++y) {
					const Vector3i data_block_pos(data_block_pos0 + Vector3i(x, y, z));
					VoxelDataBlock *data_block = lod.data_map.get_block(data_block_pos);

					if (data_block == nullptr) {
						loaded = false;
						// TODO This is quite lossy in this case, if we ask for 8 blocks in an octant
						try_schedule_loading_with_neighbors(data_block_pos, lod_index);
					}
				}
			}
		}

		if (!loaded) {
			return false;
		}

	} else if (mesh_block_size == data_block_size) {
		const Vector3i data_block_pos = p_mesh_block_pos;
		VoxelDataBlock *block = lod.data_map.get_block(data_block_pos);
		if (block == nullptr) {
			try_schedule_loading_with_neighbors(data_block_pos, lod_index);
			return false;
		}
	}

	VoxelMeshBlock *mesh_block = lod.mesh_map.get_block(p_mesh_block_pos);
	if (mesh_block == nullptr) {
		mesh_block = VoxelMeshBlock::create(p_mesh_block_pos, mesh_block_size, lod_index);
		lod.mesh_map.set_block(p_mesh_block_pos, mesh_block);
	}

	return check_block_mesh_updated(mesh_block);
}

bool VoxelLodTerrain::check_block_mesh_updated(VoxelMeshBlock *block) {
	VOXEL_PROFILE_SCOPE();

	CRASH_COND(block == nullptr);
	Lod &lod = _lods[block->lod_index];

	switch (block->get_mesh_state()) {
		case VoxelMeshBlock::MESH_NEVER_UPDATED:
		case VoxelMeshBlock::MESH_NEED_UPDATE: {
			const int mesh_block_size = get_mesh_block_size();
			const int data_block_size = get_data_block_size();
#ifdef DEBUG_ENABLED
			ERR_FAIL_COND_V(!check_block_sizes(data_block_size, mesh_block_size), false);
#endif
			// Find data block neighbors positions
			const int factor = mesh_block_size / data_block_size;
			const Vector3i data_block_pos0 = factor * block->position;
			const Box3i data_box(data_block_pos0 - Vector3i(1), Vector3i(factor) + Vector3i(2));
			const Box3i bounds = _bounds_in_voxels.downscaled(data_block_size);
			FixedArray<Vector3i, 56> neighbor_positions;
			unsigned int neighbor_positions_count = 0;
			data_box.for_inner_outline([bounds, &neighbor_positions, &neighbor_positions_count](Vector3i pos) {
				if (bounds.contains(pos)) {
					neighbor_positions[neighbor_positions_count] = pos;
					++neighbor_positions_count;
				}
			});

			// Check if neighbors are loaded
			bool surrounded = true;
			for (unsigned int i = 0; i < neighbor_positions_count; ++i) {
				const Vector3i npos = neighbor_positions[i];
				if (!lod.data_map.has_block(npos)) {
					// That neighbor is not loaded
					surrounded = false;
					if (!lod.has_loading_block(npos)) {
						// Schedule loading for that neighbor
						lod.blocks_to_load.push_back(npos);
						lod.loading_blocks.insert(npos);
					}
				}
			}

			if (surrounded) {
				lod.blocks_pending_update.push_back(block->position);
				block->set_mesh_state(VoxelMeshBlock::MESH_UPDATE_NOT_SENT);
			}

			return false;
		}

		case VoxelMeshBlock::MESH_UPDATE_NOT_SENT:
		case VoxelMeshBlock::MESH_UPDATE_SENT:
			return false;

		case VoxelMeshBlock::MESH_UP_TO_DATE:
			return true;

		default:
			CRASH_NOW();
			break;
	}

	return true;
}

void VoxelLodTerrain::send_block_data_requests() {
	// Blocks to load
	const bool request_instances = _instancer != nullptr;
	for (unsigned int lod_index = 0; lod_index < _lod_count; ++lod_index) {
		Lod &lod = _lods[lod_index];

		for (unsigned int i = 0; i < lod.blocks_to_load.size(); ++i) {
			const Vector3i block_pos = lod.blocks_to_load[i];
			VoxelServer::get_singleton()->request_block_load(_volume_id, block_pos, lod_index, request_instances);
		}

		lod.blocks_to_load.clear();
	}

	// Blocks to save
	for (unsigned int i = 0; i < _blocks_to_save.size(); ++i) {
		PRINT_VERBOSE(String("Requesting save of block {0} lod {1}")
							  .format(varray(_blocks_to_save[i].position.to_vec3(), _blocks_to_save[i].lod)));
		BlockToSave &b = _blocks_to_save[i];
		VoxelServer::get_singleton()->request_voxel_block_save(_volume_id, b.voxels, b.position, b.lod);
	}

	_blocks_to_save.clear();
}

void VoxelLodTerrain::_process(float delta) {
	VOXEL_PROFILE_SCOPE();

	if (get_lod_count() == 0) {
		// If there isn't a LOD 0, there is nothing to load
		return;
	}

	// Get viewer location in voxel space
	const Vector3 viewer_pos = get_local_viewer_pos();

	_stats.dropped_block_loads = 0;
	_stats.dropped_block_meshs = 0;
	_stats.blocked_lods = 0;

	// Here we go...

	// Update pending LOD data modifications due to edits.
	// These are deferred from edits so we can batch them.
	// It has to happen first because blocks can be unloaded afterwards.
	flush_pending_lod_edits();

	ProfilingClock profiling_clock;

	// Unload data blocks falling out of block region extent
	{
		VOXEL_PROFILE_SCOPE_NAMED("Sliding box data unload");
		// TODO Could it actually be enough to have a rolling update on all blocks?

		// This should be the same distance relatively to each LOD
		const int data_block_region_extent = get_data_block_region_extent();

		// Ignore largest lod because it can extend a little beyond due to the view distance setting.
		// Instead, those blocks are unloaded by the octree forest management.
		// Iterating from big to small LOD so we can exit earlier if bounds don't intersect.
		for (int lod_index = get_lod_count() - 2; lod_index >= 0; --lod_index) {
			VOXEL_PROFILE_SCOPE();
			Lod &lod = _lods[lod_index];

			// Each LOD keeps a box of loaded blocks, and only some of the blocks will get polygonized.
			// The player can edit them so changes can be propagated to lower lods.

			unsigned int block_size_po2 = _lods[0].data_map.get_block_size_pow2() + lod_index;
			Vector3i viewer_block_pos_within_lod =
					VoxelDataMap::voxel_to_block_b(Vector3i::from_floored(viewer_pos), block_size_po2);

			const Box3i bounds_in_blocks = Box3i(
					_bounds_in_voxels.pos >> block_size_po2,
					_bounds_in_voxels.size >> block_size_po2);

			const Box3i new_box =
					Box3i::from_center_extents(viewer_block_pos_within_lod, Vector3i(data_block_region_extent));
			const Box3i prev_box =
					Box3i::from_center_extents(
							lod.last_viewer_data_block_pos, Vector3i(lod.last_view_distance_data_blocks));

			if (!new_box.intersects(bounds_in_blocks) && !prev_box.intersects(bounds_in_blocks)) {
				// If this box doesn't intersect either now or before, there is no chance a smaller one will
				break;
			}

			// Eliminate pending blocks that aren't needed

			// This vector must be empty at this point.
			ERR_FAIL_COND(!lod.blocks_to_load.empty());

			if (prev_box != new_box) {
				VOXEL_PROFILE_SCOPE_NAMED("Unload data");
				prev_box.difference(new_box, [this, lod_index](Box3i out_of_range_box) {
					out_of_range_box.for_each_cell([=](Vector3i pos) {
						//print_line(String("Immerge {0}").format(varray(pos.to_vec3())));
						unload_data_block(pos, lod_index);
					});
				});
			}

			{
				VOXEL_PROFILE_SCOPE_NAMED("Cancel updates");
				// Cancel block updates that are not within the padded region
				// (since neighbors are always required to remesh)

				const Box3i padded_new_box = new_box.padded(-1);
				Box3i mesh_box;
				if (get_mesh_block_size() > get_data_block_size()) {
					const int factor = get_mesh_block_size() / get_data_block_size();
					mesh_box = padded_new_box.downscaled_inner(factor);
				} else {
					mesh_box = padded_new_box;
				}

				unordered_remove_if(lod.blocks_pending_update, [&lod, mesh_box](Vector3i bpos) {
					if (mesh_box.contains(bpos)) {
						return false;
					} else {
						VoxelMeshBlock *block = lod.mesh_map.get_block(bpos);
						if (block != nullptr) {
							block->set_mesh_state(VoxelMeshBlock::MESH_NEED_UPDATE);
						}
						return true;
					}
				});
			}

			lod.last_viewer_data_block_pos = viewer_block_pos_within_lod;
			lod.last_view_distance_data_blocks = data_block_region_extent;
		}
	}

	// Unload mesh blocks falling out of block region extent
	{
		VOXEL_PROFILE_SCOPE_NAMED("Sliding box mesh unload");
		// TODO Could it actually be enough to have a rolling update on all blocks?

		// This should be the same distance relatively to each LOD
		const int mesh_block_region_extent = get_mesh_block_region_extent();

		// Ignore largest lod because it can extend a little beyond due to the view distance setting.
		// Instead, those blocks are unloaded by the octree forest management.
		// Iterating from big to small LOD so we can exit earlier if bounds don't intersect.
		for (int lod_index = get_lod_count() - 2; lod_index >= 0; --lod_index) {
			VOXEL_PROFILE_SCOPE();
			Lod &lod = _lods[lod_index];

			unsigned int block_size_po2 = _lods[0].mesh_map.get_block_size_pow2() + lod_index;
			Vector3i viewer_block_pos_within_lod =
					VoxelMeshMap::voxel_to_block_b(Vector3i::from_floored(viewer_pos), block_size_po2);

			const Box3i bounds_in_blocks = Box3i(
					_bounds_in_voxels.pos >> block_size_po2,
					_bounds_in_voxels.size >> block_size_po2);

			const Box3i new_box =
					Box3i::from_center_extents(viewer_block_pos_within_lod, Vector3i(mesh_block_region_extent));
			const Box3i prev_box =
					Box3i::from_center_extents(lod.last_viewer_mesh_block_pos, Vector3i(lod.last_view_distance_mesh_blocks));

			if (!new_box.intersects(bounds_in_blocks) && !prev_box.intersects(bounds_in_blocks)) {
				// If this box doesn't intersect either now or before, there is no chance a smaller one will
				break;
			}

			// Eliminate pending blocks that aren't needed

			if (prev_box != new_box) {
				VOXEL_PROFILE_SCOPE_NAMED("Unload meshes");
				prev_box.difference(new_box, [this, lod_index](Box3i out_of_range_box) {
					out_of_range_box.for_each_cell([=](Vector3i pos) {
						//print_line(String("Immerge {0}").format(varray(pos.to_vec3())));
						unload_mesh_block(pos, lod_index);
					});
				});
			}

			{
				VOXEL_PROFILE_SCOPE_NAMED("Cancel updates");
				// Cancel block updates that are not within the new region
				unordered_remove_if(lod.blocks_pending_update, [&lod, new_box](Vector3i bpos) {
					return !new_box.contains(bpos);
				});
			}

			lod.last_viewer_mesh_block_pos = viewer_block_pos_within_lod;
			lod.last_view_distance_mesh_blocks = mesh_block_region_extent;
		}
	}

	// Create and remove octrees in a grid around the viewer.
	// Mesh blocks drive the loading of voxel data and visuals.
	{
		VOXEL_PROFILE_SCOPE_NAMED("Sliding box octrees");
		// TODO Investigate if multi-octree can produce cracks in the terrain (so far I haven't noticed)

		const unsigned int octree_size_po2 = get_octree_size_po2(*this);
		const unsigned int octree_size = 1 << octree_size_po2;
		const unsigned int octree_region_extent = 1 + _view_distance_voxels / (1 << octree_size_po2);

		const Vector3i viewer_octree_pos =
				(Vector3i::from_floored(viewer_pos) + Vector3i(octree_size / 2)) >> octree_size_po2;

		const Box3i bounds_in_octrees = _bounds_in_voxels.downscaled(octree_size);

		const Box3i new_box = Box3i::from_center_extents(viewer_octree_pos, Vector3i(octree_region_extent))
									  .clipped(bounds_in_octrees);
		const Box3i prev_box = _last_octree_region_box;

		if (new_box != prev_box) {
			struct CleanOctreeAction {
				VoxelLodTerrain *self;
				Vector3i block_offset_lod0;

				void operator()(Vector3i node_pos, unsigned int lod_index) {
					Lod &lod = self->_lods[lod_index];

					Vector3i bpos = node_pos + (block_offset_lod0 >> lod_index);

					VoxelMeshBlock *block = lod.mesh_map.get_block(bpos);
					if (block != nullptr) {
						self->set_mesh_block_active(*block, false);
					}
				}
			};

			struct ExitAction {
				VoxelLodTerrain *self;
				void operator()(const Vector3i &pos) {
					Map<Vector3i, OctreeItem>::Element *E = self->_lod_octrees.find(pos);
					if (E == nullptr) {
						return;
					}

					OctreeItem &item = E->value();
					const Vector3i block_pos_maxlod = E->key();

					const unsigned int last_lod_index = self->get_lod_count() - 1;

					// We just drop the octree and hide blocks it was considering as visible.
					// Normally such octrees shouldn't bee too deep as they will likely be at the edge
					// of the loaded area, unless the player teleported far away.
					CleanOctreeAction a;
					a.self = self;
					a.block_offset_lod0 = block_pos_maxlod << last_lod_index;
					item.octree.clear(a);

					self->_lod_octrees.erase(E);

					// Unload last lod from here, as it may extend a bit further than the others.
					// Other LODs are unloaded earlier using a sliding region.
					self->unload_mesh_block(pos, last_lod_index);
					// TODO Unload data block too?
				}
			};

			struct EnterAction {
				VoxelLodTerrain *self;
				int block_size;
				void operator()(const Vector3i &pos) {
					// That's a new cell we are entering, shouldn't be anything there
					CRASH_COND(self->_lod_octrees.has(pos));

					// Create new octree
					// TODO Use ObjectPool to store them, deletion won't be cheap
					Map<Vector3i, OctreeItem>::Element *E = self->_lod_octrees.insert(pos, OctreeItem());
					CRASH_COND(E == nullptr);
					OctreeItem &item = E->value();
					LodOctree::NoDestroyAction nda;
					item.octree.create_from_lod_count(block_size, self->get_lod_count(), nda);
					item.octree.set_lod_distance(self->get_lod_distance());
				}
			};

			ExitAction exit_action;
			exit_action.self = this;

			EnterAction enter_action;
			enter_action.self = this;
			enter_action.block_size = get_mesh_block_size();

			{
				VOXEL_PROFILE_SCOPE_NAMED("Unload octrees");
				prev_box.difference(new_box, [exit_action](Box3i out_of_range_box) {
					out_of_range_box.for_each_cell(exit_action);
				});
			}
			{
				VOXEL_PROFILE_SCOPE_NAMED("Load octrees");
				new_box.difference(prev_box, [enter_action](Box3i box_to_load) {
					box_to_load.for_each_cell(enter_action);
				});
			}
		}

		_last_octree_region_box = new_box;
	}

	CRASH_COND(_blocks_pending_transition_update.size() != 0);

	const bool stream_enabled = (_stream.is_valid() || _generator.is_valid()) &&
								(Engine::get_singleton()->is_editor_hint() == false || _run_stream_in_editor);

	// Find which blocks we need to load and see, within each octree
	if (stream_enabled) {
		VOXEL_PROFILE_SCOPE_NAMED("Update octrees");

		// TODO Maintain a vector to make iteration faster?
		for (Map<Vector3i, OctreeItem>::Element *E = _lod_octrees.front(); E; E = E->next()) {
			VOXEL_PROFILE_SCOPE();

			OctreeItem &item = E->value();
			Vector3i block_pos_maxlod = E->key();
			Vector3i block_offset_lod0 = block_pos_maxlod << (get_lod_count() - 1);

			struct OctreeActions {
				VoxelLodTerrain *self = nullptr;
				Vector3i block_offset_lod0;
				unsigned int blocked_count = 0;

				void create_child(Vector3i node_pos, int lod_index, LodOctree::NodeData &data) {
					Lod &lod = self->_lods[lod_index];
					Vector3i bpos = node_pos + (block_offset_lod0 >> lod_index);
					VoxelMeshBlock *block = lod.mesh_map.get_block(bpos);

					// Never show a child that hasn't been meshed
					CRASH_COND(block == nullptr);
					CRASH_COND(block->get_mesh_state() != VoxelMeshBlock::MESH_UP_TO_DATE);

					self->set_mesh_block_active(*block, true);
					self->add_transition_update(block);
					self->add_transition_updates_around(bpos, lod_index);
				}

				void destroy_child(Vector3i node_pos, int lod_index) {
					Lod &lod = self->_lods[lod_index];
					Vector3i bpos = node_pos + (block_offset_lod0 >> lod_index);
					VoxelMeshBlock *block = lod.mesh_map.get_block(bpos);

					if (block != nullptr) {
						self->set_mesh_block_active(*block, false);
						self->add_transition_updates_around(bpos, lod_index);
					}
				}

				void show_parent(Vector3i node_pos, int lod_index) {
					Lod &lod = self->_lods[lod_index];
					Vector3i bpos = node_pos + (block_offset_lod0 >> lod_index);
					VoxelMeshBlock *block = lod.mesh_map.get_block(bpos);

					// If we teleport far away, the area we were in is going to merge,
					// and blocks may have been unloaded completely.
					// So in that case it's normal to not find any block.
					// Otherwise, there must always be a visible parent in the end, unless the octree vanished.
					if (block != nullptr && block->get_mesh_state() == VoxelMeshBlock::MESH_UP_TO_DATE) {
						self->set_mesh_block_active(*block, true);
						self->add_transition_update(block);
						self->add_transition_updates_around(bpos, lod_index);
					}
				}

				void hide_parent(Vector3i node_pos, int lod_index) {
					destroy_child(node_pos, lod_index); // Same
				}

				bool can_create_root(int lod_index) {
					Vector3i offset = block_offset_lod0 >> lod_index;
					return self->check_block_loaded_and_meshed(offset, lod_index);
				}

				bool can_split(Vector3i node_pos, int child_lod_index, LodOctree::NodeData &data) {
					VOXEL_PROFILE_SCOPE();
					Vector3i offset = block_offset_lod0 >> child_lod_index;
					bool can = true;

					// Can only subdivide if higher detail meshes are ready to be shown, otherwise it will produce holes
					for (int i = 0; i < 8; ++i) {
						// Get block pos local-to-region
						Vector3i child_pos = LodOctree::get_child_position(node_pos, i);
						// Convert to local-to-terrain
						child_pos += offset;
						// We have to ping ALL children, because the reason we are here is we want them loaded
						can &= self->check_block_loaded_and_meshed(child_pos, child_lod_index);
					}

					// Can only subdivide if blocks of a higher LOD index are present around,
					// otherwise it will cause cracks.
					// Need to check meshes, not voxels?
					// const int lod_index = child_lod_index + 1;
					// if (lod_index < self->get_lod_count()) {
					// 	const Vector3i parent_offset = block_offset_lod0 >> lod_index;
					// 	const Lod &lod = self->_lods[lod_index];
					// 	can &= self->is_block_surrounded(node_pos + parent_offset, lod_index, lod.map);
					// }

					if (!can) {
						++blocked_count;
					}

					return can;
				}

				bool can_join(Vector3i node_pos, int parent_lod_index) {
					VOXEL_PROFILE_SCOPE();
					// Can only unsubdivide if the parent mesh is ready
					Lod &lod = self->_lods[parent_lod_index];

					Vector3i bpos = node_pos + (block_offset_lod0 >> parent_lod_index);
					VoxelMeshBlock *block = lod.mesh_map.get_block(bpos);

					if (block == nullptr) {
						// The block got unloaded. Exceptionally, we can join.
						// There will always be a grand-parent because we never destroy them when they split,
						// and we never create a child without creating a parent first.
						return true;
					}

					// The block is loaded (?) but the mesh isn't up to date, we need to ping and wait.
					bool can = self->check_block_mesh_updated(block);

					if (!can) {
						++blocked_count;
					}

					return can;
				}
			};

			OctreeActions octree_actions;
			octree_actions.self = this;
			octree_actions.block_offset_lod0 = block_offset_lod0;

			Vector3 relative_viewer_pos = viewer_pos - get_mesh_block_size() * block_offset_lod0.to_vec3();
			item.octree.update(relative_viewer_pos, octree_actions);

			// Ideally, this stat should stabilize to zero.
			// If not, something in block management prevents LODs from properly show up and should be fixed.
			_stats.blocked_lods += octree_actions.blocked_count;
		}

		{
			VOXEL_PROFILE_SCOPE_NAMED("Transition updates");
			process_transition_updates();
		}
	}

	CRASH_COND(_blocks_pending_transition_update.size() != 0);

	_stats.time_detect_required_blocks = profiling_clock.restart();

	// It's possible the user didn't set a stream yet, or it is turned off
	if (stream_enabled) {
		send_block_data_requests();
	}

	_stats.time_request_blocks_to_load = profiling_clock.restart();

	// Get block loading responses
	// Note: if block loading is too fast, this can cause stutters.
	// It should only happen on first load, though.
	{
		VOXEL_PROFILE_SCOPE_NAMED("Data loading responses");

		for (size_t reception_index = 0; reception_index < _reception_buffers.data_output.size(); ++reception_index) {
			VOXEL_PROFILE_SCOPE();
			VoxelServer::BlockDataOutput &ob = _reception_buffers.data_output[reception_index];

			if (ob.type == VoxelServer::BlockDataOutput::TYPE_SAVE) {
				// That's a save confirmation event.
				// Note: in the future, if blocks don't get copied before being sent for saving,
				// we will need to use block versionning to know when we can reset the `modified` flag properly

				// TODO Now that's the case. Use version? Or just keep copying?
				continue;
			}

			if (ob.lod >= _lod_count) {
				// That block was requested at a time where LOD was higher... drop it
				++_stats.dropped_block_loads;
				continue;
			}

			Lod &lod = _lods[ob.lod];

			{
				std::unordered_set<Vector3i>::iterator it = lod.loading_blocks.find(ob.position);
				if (it == lod.loading_blocks.end()) {
					// That block was not requested, or is no longer needed. drop it...
					++_stats.dropped_block_loads;
					continue;
				}

				lod.loading_blocks.erase(it);
			}

			if (ob.dropped) {
				// That block was dropped by the data loader thread, but we were still expecting it...
				// This is most likely caused by the loader not keeping up with the speed at which the player is moving.
				// We should recover with the removal from `loading_blocks` so it will be re-queried again later...

				//				print_line(String("Received a block loading drop while we were still expecting it: lod{0} ({1}, {2}, {3})")
				//								   .format(varray(ob.lod, ob.position.x, ob.position.y, ob.position.z)));

				++_stats.dropped_block_loads;
				continue;
			}

			if (ob.voxels->get_size() != Vector3i(lod.data_map.get_block_size())) {
				// Voxel block size is incorrect, drop it
				ERR_PRINT("Block size obtained from stream is different from expected size");
				++_stats.dropped_block_loads;
				continue;
			}

			// Store buffer
			VoxelDataBlock *block = lod.data_map.set_block_buffer(ob.position, ob.voxels);
			CRASH_COND(block == nullptr);

			if (_instancer != nullptr && ob.instances != nullptr) {
				VoxelServer::BlockDataOutput &wob = _reception_buffers.data_output[reception_index];
				_instancer->on_data_block_loaded(wob.position, wob.lod, std::move(wob.instances));
			}
		}

		_reception_buffers.data_output.clear();
	}

	process_fading_blocks(delta);

	_stats.time_process_load_responses = profiling_clock.restart();

	// Send mesh update requests
	{
		VOXEL_PROFILE_SCOPE_NAMED("Send mesh requests");

		const int render_to_data_factor = get_mesh_block_size() / get_data_block_size();

		for (unsigned int lod_index = 0; lod_index < _lod_count; ++lod_index) {
			VOXEL_PROFILE_SCOPE();
			Lod &lod = _lods[lod_index];

			for (unsigned int bi = 0; bi < lod.blocks_pending_update.size(); ++bi) {
				VOXEL_PROFILE_SCOPE();
				const Vector3i mesh_block_pos = lod.blocks_pending_update[bi];

				VoxelMeshBlock *block = lod.mesh_map.get_block(mesh_block_pos);
				// A block must have been allocated before we ask for a mesh update
				ERR_CONTINUE(block == nullptr);
				// All blocks we get here must be in the scheduled state
				ERR_CONTINUE(block->get_mesh_state() != VoxelMeshBlock::MESH_UPDATE_NOT_SENT);

				// Get block and its neighbors
				VoxelServer::BlockMeshInput mesh_request;
				mesh_request.render_block_position = mesh_block_pos;
				mesh_request.lod = lod_index;

				const Box3i data_box =
						Box3i(render_to_data_factor * mesh_block_pos, Vector3i(render_to_data_factor)).padded(1);

				// Iteration order matters for thread access.
				// The array also implicitely encodes block position due to the convention being used,
				// so there is no need to also include positions in the request
				data_box.for_each_cell_zxy([&mesh_request, &lod](Vector3i data_block_pos) {
					VoxelDataBlock *nblock = lod.data_map.get_block(data_block_pos);
					// The block can actually be null on some occasions. Not sure yet if it's that bad
					//CRASH_COND(nblock == nullptr);
					if (nblock != nullptr) {
						mesh_request.data_blocks[mesh_request.data_blocks_count] = nblock->get_voxels_shared();
					}
					++mesh_request.data_blocks_count;
				});

				VoxelServer::get_singleton()->request_block_mesh(_volume_id, mesh_request);

				block->set_mesh_state(VoxelMeshBlock::MESH_UPDATE_SENT);
			}

			lod.blocks_pending_update.clear();
		}
	}

	_stats.time_request_blocks_to_update = profiling_clock.restart();

	// TODO This could go into time spread tasks too
	process_deferred_collision_updates(VoxelServer::get_singleton()->get_main_thread_time_budget_usec());

#ifdef TOOLS_ENABLED
	if (is_showing_gizmos() && is_visible_in_tree()) {
		update_gizmos();
	}
#endif
}

void VoxelLodTerrain::apply_mesh_update(const VoxelServer::BlockMeshOutput &ob) {
	// The following is done on the main thread because Godot doesn't really support multithreaded Mesh allocation.
	// This also proved to be very slow compared to the meshing process itself...
	// hopefully Vulkan will allow us to upload graphical resources without stalling rendering as they upload?
	VOXEL_PROFILE_SCOPE();

	ERR_FAIL_COND(!is_inside_tree());

	if (ob.lod >= _lod_count) {
		// Sorry, LOD configuration changed, drop that mesh
		++_stats.dropped_block_meshs;
		return;
	}

	Lod &lod = _lods[ob.lod];

	VoxelMeshBlock *block = lod.mesh_map.get_block(ob.position);
	if (block == nullptr) {
		// That block is no longer loaded, drop the result
		++_stats.dropped_block_meshs;
		return;
	}

	if (ob.type == VoxelServer::BlockMeshOutput::TYPE_DROPPED) {
		// That block is loaded, but its meshing request was dropped.
		// TODO Not sure what to do in this case, the code sending update queries has to be tweaked
		PRINT_VERBOSE("Received a block mesh drop while we were still expecting it");
		++_stats.dropped_block_meshs;
		return;
	}

	if (block->get_mesh_state() == VoxelMeshBlock::MESH_UPDATE_SENT) {
		block->set_mesh_state(VoxelMeshBlock::MESH_UP_TO_DATE);
	}

	const VoxelMesher::Output mesh_data = ob.surfaces;

	Ref<ArrayMesh> mesh = build_mesh(
			mesh_data.surfaces,
			mesh_data.primitive_type,
			mesh_data.compression_flags,
			_material);

	bool has_collision = _generate_collisions;
	if (has_collision && _collision_lod_count != 0) {
		has_collision = ob.lod < _collision_lod_count;
	}

	if (block->got_first_mesh_update == false) {
		block->got_first_mesh_update = true;

		// TODO Need a more generic API for this kind of stuff
		if (_instancer != nullptr && ob.surfaces.surfaces.size() > 0) {
			// TODO The mesh could come from an edited region!
			// We would have to know if specific voxels got edited, or different from the generator
			_instancer->on_mesh_block_enter(ob.position, ob.lod, ob.surfaces.surfaces[0]);
		}

		// Lazy initialization

		//print_line(String("Adding block {0} at lod {1}").format(varray(eo.block_position.to_vec3(), eo.lod)));
		//set_mesh_block_active(*block, false);
		block->set_parent_visible(is_visible());
		block->set_world(get_world());

		Ref<ShaderMaterial> shader_material = _material;
		if (shader_material.is_valid() && block->get_shader_material().is_null()) {
			VOXEL_PROFILE_SCOPE();

			// Pooling shader materials is necessary for now, to avoid stuttering in the editor.
			// Due to a signal used to keep the inspector up to date, even though these
			// material copies will never be seen in the inspector
			// See https://github.com/godotengine/godot/issues/34741
			Ref<ShaderMaterial> sm;
			if (_shader_material_pool.size() > 0) {
				sm = _shader_material_pool.back();
				// The joys of pooling materials
				sm->set_shader_param(VoxelStringNames::get_singleton()->u_transition_mask, 0);
				_shader_material_pool.pop_back();
			} else {
				sm = shader_material->duplicate(false);
			}

			// Set individual shader material, because each block can have dynamic parameters,
			// used to smooth seams without re-uploading meshes and allow to implement LOD fading
			block->set_shader_material(sm);
		}
	}

	block->set_mesh(mesh);
	{
		VOXEL_PROFILE_SCOPE();
		for (unsigned int dir = 0; dir < mesh_data.transition_surfaces.size(); ++dir) {
			Ref<ArrayMesh> transition_mesh = build_mesh(
					mesh_data.transition_surfaces[dir],
					mesh_data.primitive_type,
					mesh_data.compression_flags,
					_material);

			block->set_transition_mesh(transition_mesh, dir);
		}
	}

	const uint32_t now = get_ticks_msec();
	if (has_collision) {
		if (_collision_update_delay == 0 ||
				static_cast<int>(now - block->last_collider_update_time) > _collision_update_delay) {
			block->set_collision_mesh(mesh_data.surfaces, get_tree()->is_debugging_collisions_hint(), this,
					_collision_margin);
			block->set_collision_layer(_collision_layer);
			block->set_collision_mask(_collision_mask);
			block->last_collider_update_time = now;
			block->has_deferred_collider_update = false;
			block->deferred_collider_data.clear();
		} else {
			if (!block->has_deferred_collider_update) {
				lod.deferred_collision_updates.push_back(ob.position);
				block->has_deferred_collider_update = true;
			}
			block->deferred_collider_data = mesh_data.surfaces;
		}
	}

	block->set_parent_transform(get_global_transform());
}

void VoxelLodTerrain::process_deferred_collision_updates(uint32_t timeout_msec) {
	VOXEL_PROFILE_SCOPE();

	for (unsigned int lod_index = 0; lod_index < _lod_count; ++lod_index) {
		Lod &lod = _lods[lod_index];

		for (unsigned int i = 0; i < lod.deferred_collision_updates.size(); ++i) {
			const Vector3i block_pos = lod.deferred_collision_updates[i];
			VoxelMeshBlock *block = lod.mesh_map.get_block(block_pos);

			if (block == nullptr || block->has_deferred_collider_update == false) {
				// Block was unloaded or no longer needs a collision update
				unordered_remove(lod.deferred_collision_updates, i);
				--i;
				continue;
			}

			const uint32_t now = get_ticks_msec();

			if (static_cast<int>(now - block->last_collider_update_time) > _collision_update_delay) {
				block->set_collision_mesh(
						block->deferred_collider_data, get_tree()->is_debugging_collisions_hint(), this,
						_collision_margin);
				block->set_collision_layer(_collision_layer);
				block->set_collision_mask(_collision_mask);
				block->last_collider_update_time = now;
				block->has_deferred_collider_update = false;
				block->deferred_collider_data.clear();

				unordered_remove(lod.deferred_collision_updates, i);
				--i;
			}

			// We always process at least one, then we to check the timeout
			if (get_ticks_msec() >= timeout_msec) {
				return;
			}
		}
	}
}

void VoxelLodTerrain::process_fading_blocks(float delta) {
	VOXEL_PROFILE_SCOPE();

	const float speed = _lod_fade_duration < 0.001f ? 99999.f : delta / _lod_fade_duration;

	for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		Lod &lod = _lods[lod_index];

		Map<Vector3i, VoxelMeshBlock *>::Element *e = lod.fading_blocks.front();

		while (e != nullptr) {
			VoxelMeshBlock *block = e->value();
			// The collection of fading blocks must only contain fading blocks
			ERR_FAIL_COND(block->fading_state == VoxelMeshBlock::FADING_NONE);

			const bool finished = block->update_fading(speed);

			if (finished) {
				Map<Vector3i, VoxelMeshBlock *>::Element *next = e->next();
				lod.fading_blocks.erase(e);
				e = next;

			} else {
				e = e->next();
			}
		}
	}
}

void VoxelLodTerrain::flush_pending_lod_edits() {
	// Propagates edits performed so far to other LODs.
	// These LODs must be currently in memory, otherwise terrain data will miss it.
	// This is currently ensured by the fact we load blocks in a "pyramidal" way,
	// i.e there is no way for a block to be loaded if its parent LOD isn't loaded already.
	// In the future we may implement storing of edits to be applied later if blocks can't be found.

	//ProfilingClock profiling_clock;

	const int data_to_mesh_factor = get_mesh_block_size() / get_data_block_size();

	// Make sure LOD0 gets updates even if _lod_count is 1
	Lod &lod0 = _lods[0];
	for (unsigned int i = 0; i < lod0.blocks_pending_lodding.size(); ++i) {
		const Vector3i data_block_pos = lod0.blocks_pending_lodding[i];
		VoxelDataBlock *data_block = lod0.data_map.get_block(data_block_pos);
		ERR_CONTINUE(data_block == nullptr);
		data_block->set_needs_lodding(false);

		const Vector3i mesh_block_pos = data_block_pos.floordiv(data_to_mesh_factor);
		VoxelMeshBlock *mesh_block = lod0.mesh_map.get_block(mesh_block_pos);
		if (mesh_block != nullptr) {
			// If a mesh exists here, it will need an update.
			// If there is no mesh, it will probably get created later when we come closer to it
			schedule_mesh_update(mesh_block, lod0.blocks_pending_update);
		}
	}

	const int half_bs = get_data_block_size() >> 1;

	// Process downscales upwards in pairs of consecutive LODs.
	// This ensures we don't process multiple times the same blocks.
	// Only LOD0 is editable at the moment, so we'll downscale from there
	for (unsigned int dst_lod_index = 1; dst_lod_index < _lod_count; ++dst_lod_index) {
		Lod &src_lod = _lods[dst_lod_index - 1];
		Lod &dst_lod = _lods[dst_lod_index];

		for (unsigned int i = 0; i < src_lod.blocks_pending_lodding.size(); ++i) {
			const Vector3i src_bpos = src_lod.blocks_pending_lodding[i];
			const Vector3i dst_bpos = src_bpos >> 1;

			VoxelDataBlock *src_block = src_lod.data_map.get_block(src_bpos);
			VoxelDataBlock *dst_block = dst_lod.data_map.get_block(dst_bpos);

			src_block->set_needs_lodding(false);

			if (dst_block == nullptr) {
				ERR_PRINT(String("Destination block {0} not found when cascading edits on LOD {1}")
								  .format(varray(dst_bpos.to_vec3(), dst_lod_index)));
				continue;
			}

			// The block and its lower LODs are expected to be available.
			// Otherwise it means the function was called too late
			CRASH_COND(src_block == nullptr);
			//CRASH_COND(dst_block == nullptr);

			{
				const Vector3i mesh_block_pos = dst_bpos.floordiv(data_to_mesh_factor);
				VoxelMeshBlock *mesh_block = dst_lod.mesh_map.get_block(mesh_block_pos);
				if (mesh_block != nullptr) {
					schedule_mesh_update(mesh_block, dst_lod.blocks_pending_update);
				}
				// If there is no mesh, it will probably get created later when we come closer to it
			}

			dst_block->set_modified(true);

			if (dst_lod_index != _lod_count - 1 && !dst_block->get_needs_lodding()) {
				dst_block->set_needs_lodding(true);
				dst_lod.blocks_pending_lodding.push_back(dst_bpos);
			}

			const Vector3i rel = src_bpos - (dst_bpos << 1);

			// Update lower LOD
			// This must always be done after an edit before it gets saved, otherwise LODs won't match and it will look ugly.
			// TODO Optimization: try to narrow to edited region instead of taking whole block
			{
				RWLockWrite lock(src_block->get_voxels().get_lock());
				src_block->get_voxels().downscale_to(
						dst_block->get_voxels(), Vector3i(), src_block->get_voxels_const().get_size(), rel * half_bs);
			}
		}

		src_lod.blocks_pending_lodding.clear();
	}

	// Make sure LOD0 has its list cleared, because in case there is only 1 LOD,
	// the chain of updates above will not be entered
	lod0.blocks_pending_lodding.clear();

	//	uint64_t time_spent = profiling_clock.restart();
	//	if (time_spent > 10) {
	//		print_line(String("Took {0} us to update lods").format(varray(time_spent)));
	//	}
}

void VoxelLodTerrain::set_instancer(VoxelInstancer *instancer) {
	if (_instancer != nullptr && instancer != nullptr) {
		ERR_FAIL_COND_MSG(_instancer != nullptr, "No more than one VoxelInstancer per terrain");
	}
	_instancer = instancer;
}

void VoxelLodTerrain::unload_data_block(Vector3i block_pos, int lod_index) {
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND(lod_index >= get_lod_count());

	Lod &lod = _lods[lod_index];

	lod.data_map.remove_block(block_pos, BeforeUnloadDataAction{ _blocks_to_save, _stream.is_valid() });

	lod.loading_blocks.erase(block_pos);

	// if (_instancer != nullptr) {
	// 	_instancer->on_block_exit(block_pos, lod_index);
	// }

	// No need to remove things from blocks_pending_load,
	// This vector is filled and cleared immediately in the main process.
	// It is a member only to re-use its capacity memory over frames.
}

void VoxelLodTerrain::unload_mesh_block(Vector3i block_pos, int lod_index) {
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND(lod_index >= get_lod_count());

	Lod &lod = _lods[lod_index];

	lod.mesh_map.remove_block(block_pos, BeforeUnloadMeshAction{ _shader_material_pool });

	lod.fading_blocks.erase(block_pos);

	if (_instancer != nullptr) {
		_instancer->on_mesh_block_exit(block_pos, lod_index);
	}

	// Blocks in the update queue will be cancelled in _process,
	// because it's too expensive to linear-search all blocks for each block
}

// This function is primarily intented for editor use cases at the moment.
// It will be slower than using the instancing generation events,
// because it has to query VisualServer, which then allocates and decodes vertex buffers (assuming they are cached).
Array VoxelLodTerrain::get_mesh_block_surface(Vector3i block_pos, int lod_index) const {
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND_V(lod_index >= static_cast<int>(_lod_count), Array());
	const Lod &lod = _lods[lod_index];
	const VoxelMeshBlock *block = lod.mesh_map.get_block(block_pos);
	if (block != nullptr) {
		Ref<Mesh> mesh = block->get_mesh();
		if (mesh.is_valid()) {
			return mesh->surface_get_arrays(0);
		}
	}
	return Array();
}

Vector<Vector3i> VoxelLodTerrain::get_meshed_block_positions_at_lod(int lod_index) const {
	Vector<Vector3i> positions;
	ERR_FAIL_COND_V(lod_index >= static_cast<int>(_lod_count), positions);
	const Lod &lod = _lods[lod_index];
	lod.mesh_map.for_all_blocks([&positions](const VoxelMeshBlock *block) {
		if (block->has_mesh()) {
			positions.push_back(block->position);
		}
	});
	return positions;
}

void VoxelLodTerrain::save_all_modified_blocks(bool with_copy) {
	flush_pending_lod_edits();

	if (_stream.is_valid()) {
		for (unsigned int i = 0; i < _lod_count; ++i) {
			// That may cause a stutter, so should be used when the player won't notice
			_lods[i].data_map.for_all_blocks(ScheduleSaveAction{ _blocks_to_save });
		}

		if (_instancer != nullptr && _stream->supports_instance_blocks()) {
			_instancer->save_all_modified_blocks();
		}
	}

	// And flush immediately
	send_block_data_requests();
}

void VoxelLodTerrain::add_transition_update(VoxelMeshBlock *block) {
	if (!block->pending_transition_update) {
		_blocks_pending_transition_update.push_back(block);
		block->pending_transition_update = true;
	}
}

void VoxelLodTerrain::add_transition_updates_around(Vector3i block_pos, int lod_index) {
	Lod &lod = _lods[lod_index];

	for (int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
		Vector3i npos = block_pos + Cube::g_side_normals[dir];
		VoxelMeshBlock *nblock = lod.mesh_map.get_block(npos);

		if (nblock != nullptr) {
			add_transition_update(nblock);
		}
	}
	// TODO If a block appears at lod, neighbor blocks at lod-1 need to be updated.
	// or maybe get_transition_mask needs a different approach that also looks at higher lods?
}

void VoxelLodTerrain::process_transition_updates() {
	for (unsigned int i = 0; i < _blocks_pending_transition_update.size(); ++i) {
		VoxelMeshBlock *block = _blocks_pending_transition_update[i];
		CRASH_COND(block == nullptr);

		if (block->active) {
			block->set_transition_mask(get_transition_mask(block->position, block->lod_index));
		}

		block->pending_transition_update = false;
	}

	_blocks_pending_transition_update.clear();
}

uint8_t VoxelLodTerrain::get_transition_mask(Vector3i block_pos, int lod_index) const {
	uint8_t transition_mask = 0;

	if (lod_index + 1 >= static_cast<int>(_lod_count)) {
		return transition_mask;
	}

	const Lod &lower_lod = _lods[lod_index + 1];
	const Lod &lod = _lods[lod_index];

	const Vector3i lower_pos = block_pos >> 1;
	const Vector3i upper_pos = block_pos << 1;

	// Based on octree rules, and the fact it must have run before, check neighbor blocks of same LOD:
	// If one is missing or not visible, it means either of the following:
	// - The neighbor at lod+1 is visible or not loaded (there must be a transition)
	// - The neighbor at lod-1 is visible (no transition)

	uint8_t visible_neighbors_of_same_lod = 0;
	for (int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
		Vector3i npos = block_pos + Cube::g_side_normals[dir];

		const VoxelMeshBlock *nblock = lod.mesh_map.get_block(npos);

		if (nblock != nullptr && nblock->active) {
			visible_neighbors_of_same_lod |= (1 << dir);
		}
	}

	if (visible_neighbors_of_same_lod != 0b111111) {
		// At least one neighbor isn't visible.
		// Check for neighbors at different LOD (there can be only one kind on a given side)
		for (int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
			int dir_mask = (1 << dir);

			if (visible_neighbors_of_same_lod & dir_mask) {
				continue;
			}

			const Vector3i side_normal = Cube::g_side_normals[dir];
			const Vector3i lower_neighbor_pos = (block_pos + side_normal) >> 1;

			if (lower_neighbor_pos != lower_pos) {
				const VoxelMeshBlock *lower_neighbor_block = lower_lod.mesh_map.get_block(lower_neighbor_pos);

				if (lower_neighbor_block != nullptr && lower_neighbor_block->active) {
					// The block has a visible neighbor of lower LOD
					transition_mask |= dir_mask;
					continue;
				}
			}

			if (lod_index > 0) {
				// Check upper LOD neighbors.
				// There are always 4 on each side, checking any is enough

				Vector3i upper_neighbor_pos = upper_pos;
				for (unsigned int i = 0; i < Vector3i::AXIS_COUNT; ++i) {
					if (side_normal[i] == -1) {
						--upper_neighbor_pos[i];
					} else if (side_normal[i] == 1) {
						upper_neighbor_pos[i] += 2;
					}
				}

				const Lod &upper_lod = _lods[lod_index - 1];
				const VoxelMeshBlock *upper_neighbor_block = upper_lod.mesh_map.get_block(upper_neighbor_pos);

				if (upper_neighbor_block == nullptr || upper_neighbor_block->active == false) {
					// The block has no visible neighbor yet. World border? Assume lower LOD.
					transition_mask |= dir_mask;
				}
			}
		}
	}

	return transition_mask;
}

const VoxelLodTerrain::Stats &VoxelLodTerrain::get_stats() const {
	return _stats;
}

Dictionary VoxelLodTerrain::_b_get_statistics() const {
	Dictionary d;

	int deferred_collision_updates = 0;
	for (unsigned int lod_index = 0; lod_index < _lod_count; ++lod_index) {
		const Lod &lod = _lods[lod_index];
		deferred_collision_updates += lod.deferred_collision_updates.size();
	}

	// Breakdown of time spent in _process
	d["time_detect_required_blocks"] = _stats.time_detect_required_blocks;
	d["time_request_blocks_to_load"] = _stats.time_request_blocks_to_load;
	d["time_process_load_responses"] = _stats.time_process_load_responses;
	d["time_request_blocks_to_update"] = _stats.time_request_blocks_to_update;

	d["dropped_block_loads"] = _stats.dropped_block_loads;
	d["dropped_block_meshs"] = _stats.dropped_block_meshs;
	d["updated_blocks"] = _stats.updated_blocks;
	d["blocked_lods"] = _stats.blocked_lods;

	return d;
}

void VoxelLodTerrain::set_run_stream_in_editor(bool enable) {
	if (enable == _run_stream_in_editor) {
		return;
	}

	_run_stream_in_editor = enable;

	if (Engine::get_singleton()->is_editor_hint()) {
		if (_run_stream_in_editor) {
			_on_stream_params_changed();

		} else {
			// This is expected to block the main thread until the streaming thread is done.
			stop_streamer();
		}
	}
}

bool VoxelLodTerrain::is_stream_running_in_editor() const {
	return _run_stream_in_editor;
}

void VoxelLodTerrain::restart_stream() {
	_on_stream_params_changed();
}

void VoxelLodTerrain::remesh_all_blocks() {
	for (unsigned int lod_index = 0; lod_index < _lod_count; ++lod_index) {
		Lod &lod = _lods[lod_index];
		lod.mesh_map.for_all_blocks([&lod](VoxelMeshBlock *block) {
			schedule_mesh_update(block, lod.blocks_pending_update);
		});
	}
}

void VoxelLodTerrain::set_voxel_bounds(Box3i p_box) {
	_bounds_in_voxels =
			p_box.clipped(Box3i::from_center_extents(Vector3i(), Vector3i(VoxelConstants::MAX_VOLUME_EXTENT)));
	// Round to octree size
	const int octree_size = get_mesh_block_size() << (get_lod_count() - 1);
	_bounds_in_voxels = _bounds_in_voxels.snapped(octree_size);
	// Can't have a smaller region than one octree
	for (unsigned i = 0; i < Vector3i::AXIS_COUNT; ++i) {
		if (_bounds_in_voxels.size[i] < octree_size) {
			_bounds_in_voxels.size[i] = octree_size;
		}
	}
}

void VoxelLodTerrain::set_collision_update_delay(int delay_msec) {
	_collision_update_delay = clamp(delay_msec, 0, 4000);
}

int VoxelLodTerrain::get_collision_update_delay() const {
	return _collision_update_delay;
}

void VoxelLodTerrain::set_lod_fade_duration(float seconds) {
	_lod_fade_duration = clamp(seconds, 0.f, 1.f);
}

float VoxelLodTerrain::get_lod_fade_duration() const {
	return _lod_fade_duration;
}

String VoxelLodTerrain::get_configuration_warning() const {
	String w = VoxelNode::get_configuration_warning();
	if (!w.empty()) {
		return w;
	}
	Ref<VoxelMesher> mesher = get_mesher();
	if (mesher.is_valid() && !mesher->supports_lod()) {
		return TTR("The assigned mesher does not support level of detail (LOD), results may be unexpected.");
	}
	return String();
}

void VoxelLodTerrain::_b_save_modified_blocks() {
	save_all_modified_blocks(true);
}

void VoxelLodTerrain::_b_set_voxel_bounds(AABB aabb) {
	ERR_FAIL_COND(!is_valid_size(aabb.size));
	set_voxel_bounds(Box3i(Vector3i::from_rounded(aabb.position), Vector3i::from_rounded(aabb.size)));
}

AABB VoxelLodTerrain::_b_get_voxel_bounds() const {
	const Box3i b = get_voxel_bounds();
	return AABB(b.pos.to_vec3(), b.size.to_vec3());
}

// DEBUG LAND

Array VoxelLodTerrain::debug_raycast_mesh_block(Vector3 world_origin, Vector3 world_direction) const {
	const Transform world_to_local = get_global_transform().affine_inverse();
	Vector3 pos = world_to_local.xform(world_origin);
	const Vector3 dir = world_to_local.basis.xform(world_direction);
	const float max_distance = 256;
	const float step = 2.f;
	float distance = 0.f;

	Array hits;
	while (distance < max_distance && hits.size() == 0) {
		for (unsigned int lod_index = 0; lod_index < _lod_count; ++lod_index) {
			const Lod &lod = _lods[lod_index];
			const Vector3i bpos = lod.mesh_map.voxel_to_block(Vector3i::from_floored(pos)) >> lod_index;
			const VoxelMeshBlock *block = lod.mesh_map.get_block(bpos);
			if (block != nullptr && block->is_visible() && block->has_mesh()) {
				Dictionary d;
				d["position"] = block->position.to_vec3();
				d["lod"] = block->lod_index;
				hits.append(d);
			}
		}
		distance += step;
		pos += dir * step;
	}

	return hits;
}

Dictionary VoxelLodTerrain::debug_get_data_block_info(Vector3 fbpos, int lod_index) const {
	Dictionary d;
	ERR_FAIL_COND_V(lod_index < 0, d);
	ERR_FAIL_COND_V(lod_index >= get_lod_count(), d);

	const Lod &lod = _lods[lod_index];
	Vector3i bpos = Vector3i::from_floored(fbpos);

	int loading_state = 0;
	const VoxelDataBlock *block = lod.data_map.get_block(bpos);

	if (block != nullptr) {
		loading_state = 2;

	} else if (lod.has_loading_block(bpos)) {
		loading_state = 1;
	}

	d["loading_state"] = loading_state;
	return d;
}

Dictionary VoxelLodTerrain::debug_get_mesh_block_info(Vector3 fbpos, int lod_index) const {
	Dictionary d;
	ERR_FAIL_COND_V(lod_index < 0, d);
	ERR_FAIL_COND_V(lod_index >= get_lod_count(), d);

	const Lod &lod = _lods[lod_index];
	Vector3i bpos = Vector3i::from_floored(fbpos);

	bool loaded = false;
	bool meshed = false;
	bool visible = false;
	bool active = false;
	int mesh_state = VoxelMeshBlock::MESH_NEVER_UPDATED;
	const VoxelMeshBlock *block = lod.mesh_map.get_block(bpos);

	if (block != nullptr) {
		loaded = true;
		meshed = block->has_mesh();
		mesh_state = block->get_mesh_state();
		visible = block->is_visible();
		active = block->active;
		d["transition_mask"] = block->get_transition_mask();
		// This can highlight possible bugs between the current state and what it should be
		d["recomputed_transition_mask"] = get_transition_mask(block->position, block->lod_index);
	}

	d["loaded"] = loaded;
	d["meshed"] = meshed;
	d["mesh_state"] = mesh_state;
	d["visible"] = visible;
	d["active"] = active;
	return d;
}

Array VoxelLodTerrain::debug_get_octree_positions() const {
	Array positions;
	positions.resize(_lod_octrees.size());
	int i = 0;
	for (Map<Vector3i, OctreeItem>::Element *E = _lod_octrees.front(); E; E = E->next()) {
		positions[i++] = E->key().to_vec3();
	}
	return positions;
}

Array VoxelLodTerrain::debug_get_octrees_detailed() const {
	// [
	//     Vector3,
	//     Octree,
	//     ...
	// ]
	// Octree [
	//     state: State,
	//     Octree[8] or null
	// ]
	// State {
	//     0: no block
	//     1: no mesh
	//     2: mesh
	// }

	struct L {
		static void read_node(const LodOctree &octree, const LodOctree::Node *node, Vector3i position, int lod_index,
				const VoxelLodTerrain *self, Array &out_data) {
			ERR_FAIL_COND(lod_index < 0);
			Variant state;

			const Lod &lod = self->_lods[lod_index];
			const VoxelMeshBlock *block = lod.mesh_map.get_block(position);
			if (block == nullptr) {
				state = 0;
			} else {
				if (block->get_mesh_state() == VoxelMeshBlock::MESH_UP_TO_DATE) {
					state = 2;
				} else {
					state = 1;
				}
			}

			out_data.append(state);

			if (node->has_children()) {
				Array children_data;
				for (unsigned int i = 0; i < 8; ++i) {
					Array child_data;
					const LodOctree::Node *child = octree.get_child(node, i);
					const Vector3i child_pos = LodOctree::get_child_position(position, i);
					read_node(octree, child, child_pos, lod_index - 1, self, child_data);
					children_data.append(child_data);
				}
				out_data.append(children_data);

			} else {
				out_data.append(Variant());
			}
		}
	};

	Array forest_data;

	for (const Map<Vector3i, OctreeItem>::Element *e = _lod_octrees.front(); e; e = e->next()) {
		const LodOctree &octree = e->value().octree;
		const LodOctree::Node *root = octree.get_root();
		Array root_data;
		const Vector3i octree_pos = e->key();
		L::read_node(octree, root, octree_pos, get_lod_count() - 1, this, root_data);
		forest_data.append(octree_pos.to_vec3());
		forest_data.append(root_data);
	}

	return forest_data;
}

#ifdef TOOLS_ENABLED

void VoxelLodTerrain::update_gizmos() {
	VOXEL_PROFILE_SCOPE();

	VoxelDebug::DebugRenderer &dr = _debug_renderer;
	dr.begin();

	const Transform parent_transform = get_global_transform();

	// Octree bounds
	if (_show_octree_bounds_gizmos) {
		const int octree_size = 1 << get_octree_size_po2(*this);
		const Basis local_octree_basis = Basis().scaled(Vector3(octree_size, octree_size, octree_size));
		for (Map<Vector3i, OctreeItem>::Element *e = _lod_octrees.front(); e; e = e->next()) {
			const Transform local_transform(local_octree_basis, (e->key() * octree_size).to_vec3());
			dr.draw_box(parent_transform * local_transform, VoxelDebug::ID_OCTREE_BOUNDS);
		}
	}

	// Volume bounds
	if (_show_volume_bounds_gizmos) {
		const float bounds_in_voxels_len = _bounds_in_voxels.size.length();
		if (bounds_in_voxels_len < 10000) {
			const Vector3 margin = Vector3(1, 1, 1) * bounds_in_voxels_len * 0.0025f;
			const Vector3 size = _bounds_in_voxels.size.to_vec3();
			const Transform local_transform(Basis().scaled(size + margin * 2.f), _bounds_in_voxels.pos.to_vec3() - margin);
			dr.draw_box(parent_transform * local_transform, VoxelDebug::ID_VOXEL_BOUNDS);
		}
	}

	// Octree nodes
	if (_show_octree_node_gizmos) {
		// That can be expensive to draw
		const int mesh_block_size = get_mesh_block_size();
		const float lod_count_f = _lod_count;
		for (Map<Vector3i, OctreeItem>::Element *e = _lod_octrees.front(); e; e = e->next()) {
			const LodOctree &octree = e->value().octree;

			const Vector3i block_pos_maxlod = e->key();
			const Vector3i block_offset_lod0 = block_pos_maxlod << (get_lod_count() - 1);

			octree.for_each_leaf([&dr, block_offset_lod0, mesh_block_size, parent_transform, lod_count_f](
										 Vector3i node_pos, int lod_index, const LodOctree::NodeData &data) {
				//
				const int size = mesh_block_size << lod_index;
				const Vector3i voxel_pos = mesh_block_size * ((node_pos << lod_index) + block_offset_lod0);
				const Transform local_transform(Basis().scaled(Vector3(size, size, size)), voxel_pos.to_vec3());
				const Transform t = parent_transform * local_transform;
				// Squaring because lower lod indexes are more interesting to see, so we give them more contrast.
				// Also this might be better with sRGB?
				float g = squared(max(1.f - float(lod_index) / lod_count_f, 0.f));
				dr.draw_box_mm(t, Color8(255, uint8_t(g * 254.f), 0, 255));
			});
		}
	}

	dr.end();
}

void VoxelLodTerrain::set_show_gizmos(bool enable) {
	_show_gizmos_enabled = enable;
	if (_show_gizmos_enabled) {
		_debug_renderer.set_world(is_visible_in_tree() ? *get_world() : nullptr);
	} else {
		_debug_renderer.clear();
	}
}

#endif

Array VoxelLodTerrain::_b_debug_print_sdf_top_down(Vector3 center, Vector3 extents) const {
	ERR_FAIL_COND_V(!is_valid_size(extents), Array());

	Array image_array;
	image_array.resize(get_lod_count());

	for (unsigned int lod_index = 0; lod_index < _lod_count; ++lod_index) {
		const Box3i world_box = Box3i::from_center_extents(
				Vector3i::from_floored(center) >> lod_index, Vector3i::from_floored(extents) >> lod_index);

		if (world_box.size.volume() == 0) {
			continue;
		}

		VoxelBufferInternal buffer;
		buffer.create(world_box.size);

		const Lod &lod = _lods[lod_index];

		world_box.for_each_cell([&](const Vector3i &world_pos) {
			const float v = lod.data_map.get_voxel_f(world_pos, VoxelBuffer::CHANNEL_SDF);
			const Vector3i rpos = world_pos - world_box.pos;
			buffer.set_voxel_f(v, rpos.x, rpos.y, rpos.z, VoxelBuffer::CHANNEL_SDF);
		});

		Ref<Image> image = buffer.debug_print_sdf_to_image_top_down();
		image_array[lod_index] = image;
	}

	return image_array;
}

int VoxelLodTerrain::_b_debug_get_mesh_block_count() const {
	int sum = 0;
	for (unsigned int lod_index = 0; lod_index < _lod_count; ++lod_index) {
		sum += _lods[lod_index].mesh_map.get_block_count();
	}
	return sum;
}

int VoxelLodTerrain::_b_debug_get_data_block_count() const {
	int sum = 0;
	for (unsigned int lod_index = 0; lod_index < _lod_count; ++lod_index) {
		sum += _lods[lod_index].data_map.get_block_count();
	}
	return sum;
}

Error VoxelLodTerrain::_b_debug_dump_as_scene(String fpath) const {
	Spatial *root = memnew(Spatial);
	root->set_name(get_name());

	for (unsigned int lod_index = 0; lod_index < _lod_count; ++lod_index) {
		const Lod &lod = _lods[lod_index];

		lod.mesh_map.for_all_blocks([root](const VoxelMeshBlock *block) {
			block->for_each_mesh_instance_with_transform([root, block](const DirectMeshInstance &dmi, Transform t) {
				Ref<Mesh> mesh = dmi.get_mesh();

				if (mesh.is_valid()) {
					MeshInstance *mi = memnew(MeshInstance);
					mi->set_mesh(mesh);
					mi->set_transform(t);
					// TODO Transition mesh visibility?
					mi->set_visible(block->is_visible());
					root->add_child(mi);
					// The owner must be set after adding to parent
					mi->set_owner(root);
				}
			});
		});
	}

	Ref<PackedScene> scene = memnew(PackedScene);
	Error err = scene->pack(root);
	if (err != OK) {
		return err;
	}
	err = ResourceSaver::save(fpath, scene, ResourceSaver::FLAG_BUNDLE_RESOURCES);

	memdelete(root);

	return err;
}

void VoxelLodTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_material", "material"), &VoxelLodTerrain::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &VoxelLodTerrain::get_material);

	ClassDB::bind_method(D_METHOD("set_view_distance", "distance_in_voxels"), &VoxelLodTerrain::set_view_distance);
	ClassDB::bind_method(D_METHOD("get_view_distance"), &VoxelLodTerrain::get_view_distance);

	ClassDB::bind_method(D_METHOD("get_generate_collisions"), &VoxelLodTerrain::get_generate_collisions);
	ClassDB::bind_method(D_METHOD("set_generate_collisions", "enabled"), &VoxelLodTerrain::set_generate_collisions);

	ClassDB::bind_method(D_METHOD("get_collision_lod_count"), &VoxelLodTerrain::get_collision_lod_count);
	ClassDB::bind_method(D_METHOD("set_collision_lod_count", "count"), &VoxelLodTerrain::set_collision_lod_count);

	ClassDB::bind_method(D_METHOD("get_collision_layer"), &VoxelLodTerrain::get_collision_layer);
	ClassDB::bind_method(D_METHOD("set_collision_layer", "layer"), &VoxelLodTerrain::set_collision_layer);

	ClassDB::bind_method(D_METHOD("get_collision_mask"), &VoxelLodTerrain::get_collision_mask);
	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &VoxelLodTerrain::set_collision_mask);

	ClassDB::bind_method(D_METHOD("get_collision_margin"), &VoxelLodTerrain::get_collision_margin);
	ClassDB::bind_method(D_METHOD("set_collision_margin", "margin"), &VoxelLodTerrain::set_collision_margin);

	ClassDB::bind_method(D_METHOD("get_collision_update_delay"), &VoxelLodTerrain::get_collision_update_delay);
	ClassDB::bind_method(D_METHOD("set_collision_update_delay", "delay_msec"),
			&VoxelLodTerrain::set_collision_update_delay);

	ClassDB::bind_method(D_METHOD("get_lod_fade_duration"), &VoxelLodTerrain::get_lod_fade_duration);
	ClassDB::bind_method(D_METHOD("set_lod_fade_duration", "seconds"), &VoxelLodTerrain::set_lod_fade_duration);

	ClassDB::bind_method(D_METHOD("set_lod_count", "lod_count"), &VoxelLodTerrain::set_lod_count);
	ClassDB::bind_method(D_METHOD("get_lod_count"), &VoxelLodTerrain::get_lod_count);

	ClassDB::bind_method(D_METHOD("set_lod_distance", "lod_distance"), &VoxelLodTerrain::set_lod_distance);
	ClassDB::bind_method(D_METHOD("get_lod_distance"), &VoxelLodTerrain::get_lod_distance);

	ClassDB::bind_method(D_METHOD("get_mesh_block_size"), &VoxelLodTerrain::get_mesh_block_size);
	ClassDB::bind_method(D_METHOD("set_mesh_block_size"), &VoxelLodTerrain::set_mesh_block_size);

	ClassDB::bind_method(D_METHOD("get_data_block_size"), &VoxelLodTerrain::get_data_block_size);
	ClassDB::bind_method(D_METHOD("get_data_block_region_extent"), &VoxelLodTerrain::get_data_block_region_extent);

	ClassDB::bind_method(D_METHOD("get_statistics"), &VoxelLodTerrain::_b_get_statistics);
	ClassDB::bind_method(D_METHOD("voxel_to_data_block_position", "lod_index"),
			&VoxelLodTerrain::voxel_to_data_block_position);
	ClassDB::bind_method(D_METHOD("voxel_to_mesh_block_position", "lod_index"),
			&VoxelLodTerrain::voxel_to_mesh_block_position);

	ClassDB::bind_method(D_METHOD("get_voxel_tool"), &VoxelLodTerrain::get_voxel_tool);
	ClassDB::bind_method(D_METHOD("save_modified_blocks"), &VoxelLodTerrain::_b_save_modified_blocks);

	ClassDB::bind_method(D_METHOD("set_run_stream_in_editor"), &VoxelLodTerrain::set_run_stream_in_editor);
	ClassDB::bind_method(D_METHOD("is_stream_running_in_editor"), &VoxelLodTerrain::is_stream_running_in_editor);

	ClassDB::bind_method(D_METHOD("set_voxel_bounds"), &VoxelLodTerrain::_b_set_voxel_bounds);
	ClassDB::bind_method(D_METHOD("get_voxel_bounds"), &VoxelLodTerrain::_b_get_voxel_bounds);

	ClassDB::bind_method(D_METHOD("set_process_mode", "mode"), &VoxelLodTerrain::set_process_mode);
	ClassDB::bind_method(D_METHOD("get_process_mode"), &VoxelLodTerrain::get_process_mode);

	ClassDB::bind_method(D_METHOD("debug_raycast_mesh_block", "origin", "dir"),
			&VoxelLodTerrain::debug_raycast_mesh_block);
	ClassDB::bind_method(D_METHOD("debug_get_data_block_info", "block_pos", "lod"),
			&VoxelLodTerrain::debug_get_data_block_info);
	ClassDB::bind_method(D_METHOD("debug_get_mesh_block_info", "block_pos", "lod"),
			&VoxelLodTerrain::debug_get_mesh_block_info);
	ClassDB::bind_method(D_METHOD("debug_get_octrees_detailed"), &VoxelLodTerrain::debug_get_octrees_detailed);
	ClassDB::bind_method(D_METHOD("debug_print_sdf_top_down", "center", "extents"),
			&VoxelLodTerrain::_b_debug_print_sdf_top_down);
	ClassDB::bind_method(D_METHOD("debug_get_mesh_block_count"), &VoxelLodTerrain::_b_debug_get_mesh_block_count);
	ClassDB::bind_method(D_METHOD("debug_get_data_block_count"), &VoxelLodTerrain::_b_debug_get_data_block_count);
	ClassDB::bind_method(D_METHOD("debug_dump_as_scene", "path"), &VoxelLodTerrain::_b_debug_dump_as_scene);

	//ClassDB::bind_method(D_METHOD("_on_stream_params_changed"), &VoxelLodTerrain::_on_stream_params_changed);

	BIND_ENUM_CONSTANT(PROCESS_MODE_IDLE);
	BIND_ENUM_CONSTANT(PROCESS_MODE_PHYSICS);
	BIND_ENUM_CONSTANT(PROCESS_MODE_DISABLED);

	ADD_GROUP("Bounds", "");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "view_distance"), "set_view_distance", "get_view_distance");
	ADD_PROPERTY(PropertyInfo(Variant::AABB, "voxel_bounds"), "set_voxel_bounds", "get_voxel_bounds");

	ADD_GROUP("Level of detail", "");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod_count"), "set_lod_count", "get_lod_count");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "lod_distance"), "set_lod_distance", "get_lod_distance");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod_fade_duration"), "set_lod_fade_duration", "get_lod_fade_duration");

	ADD_GROUP("Material", "");

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "Material"),
			"set_material", "get_material");

	ADD_GROUP("Collisions", "");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generate_collisions"),
			"set_generate_collisions", "get_generate_collisions");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS),
			"set_collision_layer", "get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS),
			"set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_lod_count"),
			"set_collision_lod_count", "get_collision_lod_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_update_delay"),
			"set_collision_update_delay", "get_collision_update_delay");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "collision_margin"), "set_collision_margin", "get_collision_margin");

	ADD_GROUP("Advanced", "");

	// TODO Probably should be in parent class?
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "run_stream_in_editor"),
			"set_run_stream_in_editor", "is_stream_running_in_editor");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_block_size"), "set_mesh_block_size", "get_mesh_block_size");
}
