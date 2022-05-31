#include "voxel_lod_terrain.h"
#include "../../constants/voxel_string_names.h"
#include "../../edition/voxel_tool_lod_terrain.h"
#include "../../meshers/transvoxel/voxel_mesher_transvoxel.h"
#include "../../server/voxel_server_gd.h"
#include "../../server/voxel_server_updater.h"
#include "../../storage/voxel_buffer_gd.h"
#include "../../util/container_funcs.h"
#include "../../util/godot/funcs.h"
#include "../../util/log.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../../util/profiling_clock.h"
#include "../../util/string_funcs.h"
#include "../../util/tasks/async_dependency_tracker.h"
#include "../../util/thread/mutex.h"
#include "../../util/thread/rw_lock.h"
#include "../instancing/voxel_instancer.h"
#include "voxel_lod_terrain_update_task.h"

#include <core/config/engine.h>
#include <core/core_string_names.h>
#include <scene/3d/mesh_instance_3d.h>
#include <scene/resources/packed_scene.h>

namespace zylann::voxel {

namespace {

Ref<ArrayMesh> build_mesh(
		Span<const Array> surfaces, Mesh::PrimitiveType primitive, int flags, Ref<Material> material) {
	ZN_PROFILE_SCOPE();
	Ref<ArrayMesh> mesh;

	unsigned int surface_index = 0;
	for (unsigned int i = 0; i < surfaces.size(); ++i) {
		Array surface = surfaces[i];

		if (surface.is_empty()) {
			continue;
		}

		CRASH_COND(surface.size() != Mesh::ARRAY_MAX);
		if (!is_surface_triangulated(surface)) {
			continue;
		}

		if (mesh.is_null()) {
			mesh.instantiate();
		}

		// TODO Use `add_surface`, it's about 20% faster after measuring in Tracy (though we may see if Godot 4 expects
		// the same)
		mesh->add_surface_from_arrays(primitive, surface, Array(), Dictionary(), flags);
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

	if (mesh.is_valid() && is_mesh_empty(**mesh)) {
		mesh = Ref<Mesh>();
	}

	return mesh;
}

struct BeforeUnloadMeshAction {
	std::vector<Ref<ShaderMaterial>> &shader_material_pool;

	void operator()(VoxelMeshBlockVLT &block) {
		ZN_PROFILE_SCOPE_NAMED("Recycle material");
		// Recycle material
		Ref<ShaderMaterial> sm = block.get_shader_material();
		if (sm.is_valid()) {
			shader_material_pool.push_back(sm);
			block.set_shader_material(Ref<ShaderMaterial>());
		}
	}
};

struct ScheduleSaveAction {
	std::vector<VoxelLodTerrainUpdateData::BlockToSave> &blocks_to_save;

	void operator()(VoxelDataBlock &block) {
		// Save if modified
		// TODO Don't ask for save if the stream doesn't support it!
		if (block.is_modified()) {
			//print_line(String("Scheduling save for block {0}").format(varray(block->position.to_vec3())));
			VoxelLodTerrainUpdateData::BlockToSave b;

			b.voxels = make_shared_instance<VoxelBufferInternal>();
			{
				RWLockRead lock(block.get_voxels().get_lock());
				block.get_voxels_const().duplicate_to(*b.voxels, true);
			}

			b.position = block.position;
			b.lod = block.lod_index;
			blocks_to_save.push_back(b);
			block.set_modified(false);
		}
	}
};

static inline uint64_t get_ticks_msec() {
	return Time::get_singleton()->get_ticks_msec();
}

} // namespace

VoxelLodTerrain::VoxelLodTerrain() {
	// Note: don't do anything heavy in the constructor.
	// Godot may create and destroy dozens of instances of all node types on startup,
	// due to how ClassDB gets its default values.

	ZN_PRINT_VERBOSE("Construct VoxelLodTerrain");

	_data = make_shared_instance<VoxelDataLodMap>();
	_update_data = make_shared_instance<VoxelLodTerrainUpdateData>();
	_update_data->task_is_complete = true;
	_streaming_dependency = make_shared_instance<StreamingDependency>();
	_meshing_dependency = make_shared_instance<MeshingDependency>();

	set_notify_transform(true);

	// Doing this to setup the defaults
	set_process_callback(_process_callback);

	// Infinite by default
	_update_data->settings.bounds_in_voxels =
			Box3i::from_center_extents(Vector3i(), Vector3iUtil::create(constants::MAX_VOLUME_EXTENT));

	struct ApplyMeshUpdateTask : public ITimeSpreadTask {
		void run(TimeSpreadTaskContext &ctx) override {
			if (!VoxelServer::get_singleton().is_volume_valid(volume_id)) {
				// The node can have been destroyed while this task was still pending
				ZN_PRINT_VERBOSE("Cancelling ApplyMeshUpdateTask, volume_id is invalid");
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
	VoxelServer::VolumeCallbacks callbacks;
	callbacks.data = this;
	callbacks.mesh_output_callback = [](void *cb_data, const VoxelServer::BlockMeshOutput &ob) {
		VoxelLodTerrain *self = reinterpret_cast<VoxelLodTerrain *>(cb_data);
		ApplyMeshUpdateTask *task = memnew(ApplyMeshUpdateTask);
		task->volume_id = self->get_volume_id();
		task->self = self;
		task->data = ob;
		VoxelServer::get_singleton().push_main_thread_time_spread_task(task);
	};
	callbacks.data_output_callback = [](void *cb_data, VoxelServer::BlockDataOutput &ob) {
		VoxelLodTerrain *self = reinterpret_cast<VoxelLodTerrain *>(cb_data);
		self->apply_data_block_response(ob);
	};

	_volume_id = VoxelServer::get_singleton().add_volume(callbacks, VoxelServer::VOLUME_SPARSE_OCTREE);
	VoxelServer::get_singleton().set_volume_octree_lod_distance(_volume_id, get_lod_distance());

	// TODO Being able to set a LOD smaller than the stream is probably a bad idea,
	// Because it prevents edits from propagating up to the last one, they will be left out of sync
	set_lod_count(4);

	set_lod_distance(48.f);
}

VoxelLodTerrain::~VoxelLodTerrain() {
	ZN_PRINT_VERBOSE("Destroy VoxelLodTerrain");
	abort_async_edits();
	VoxelServer::get_singleton().remove_volume(_volume_id);
	// Instancer can take care of itself
}

Ref<Material> VoxelLodTerrain::get_material() const {
	return _material;
}

void VoxelLodTerrain::set_material(Ref<Material> p_material) {
	// TODO Update existing block surfaces
	_material = p_material;
}

unsigned int VoxelLodTerrain::get_data_block_size() const {
	return _data->lods[0].map.get_block_size();
}

unsigned int VoxelLodTerrain::get_data_block_size_pow2() const {
	return _data->lods[0].map.get_block_size_pow2();
}

unsigned int VoxelLodTerrain::get_mesh_block_size_pow2() const {
	return _update_data->settings.mesh_block_size_po2;
}

unsigned int VoxelLodTerrain::get_mesh_block_size() const {
	return 1 << _update_data->settings.mesh_block_size_po2;
}

void VoxelLodTerrain::set_stream(Ref<VoxelStream> p_stream) {
	if (p_stream == _stream) {
		return;
	}

	_stream = p_stream;

	_streaming_dependency->valid = false;
	_streaming_dependency = make_shared_instance<StreamingDependency>();
	_streaming_dependency->stream = _stream;
	_streaming_dependency->generator = _generator;
	_streaming_dependency->valid = true;

#ifdef TOOLS_ENABLED
	if (p_stream.is_valid()) {
		if (Engine::get_singleton()->is_editor_hint()) {
			Ref<Script> script = p_stream->get_script();
			if (script.is_valid()) {
				// Safety check. It's too easy to break threads by making a script reload.
				// You can turn it back on, but be careful.
				_update_data->settings.run_stream_in_editor = false;
				notify_property_list_changed();
			}
		}
	}
#endif

	_on_stream_params_changed();
}

Ref<VoxelStream> VoxelLodTerrain::get_stream() const {
	return _streaming_dependency->stream;
}

void VoxelLodTerrain::set_generator(Ref<VoxelGenerator> p_generator) {
	if (p_generator == _generator) {
		return;
	}

	_generator = p_generator;

	_meshing_dependency->valid = false;
	_meshing_dependency = make_shared_instance<MeshingDependency>();
	_meshing_dependency->mesher = _mesher;
	_meshing_dependency->generator = p_generator;
	_meshing_dependency->valid = true;

	_streaming_dependency->valid = false;
	_streaming_dependency = make_shared_instance<StreamingDependency>();
	_streaming_dependency->stream = _stream;
	_streaming_dependency->generator = p_generator;
	_streaming_dependency->valid = true;

#ifdef TOOLS_ENABLED
	if (p_generator.is_valid()) {
		if (Engine::get_singleton()->is_editor_hint()) {
			Ref<Script> script = p_generator->get_script();
			if (script.is_valid()) {
				// Safety check. It's too easy to break threads by making a script reload.
				// You can turn it back on, but be careful.
				_update_data->settings.run_stream_in_editor = false;
				notify_property_list_changed();
			}
		}
	}
#endif

	_on_stream_params_changed();
}

Ref<VoxelGenerator> VoxelLodTerrain::get_generator() const {
	return _generator;
}

void VoxelLodTerrain::_on_gi_mode_changed() {
	const GIMode gi_mode = get_gi_mode();
	for (unsigned int lod_index = 0; lod_index < _update_data->state.lods.size(); ++lod_index) {
		_mesh_maps_per_lod[lod_index].for_each_block([gi_mode](VoxelMeshBlockVLT &block) { //
			block.set_gi_mode(DirectMeshInstance::GIMode(gi_mode));
		});
	}
}

void VoxelLodTerrain::set_mesher(Ref<VoxelMesher> p_mesher) {
	if (_mesher == p_mesher) {
		return;
	}

	stop_updater();

	_mesher = p_mesher;

	_meshing_dependency->valid = false;
	_meshing_dependency = make_shared_instance<MeshingDependency>();
	_meshing_dependency->mesher = _mesher;
	_meshing_dependency->generator = _generator;
	_meshing_dependency->valid = true;

	if (_mesher.is_valid()) {
		start_updater();
		remesh_all_blocks();
	}

	update_configuration_warnings();
}

Ref<VoxelMesher> VoxelLodTerrain::get_mesher() const {
	return _mesher;
}

void VoxelLodTerrain::_on_stream_params_changed() {
	stop_streamer();
	stop_updater();

	if (_stream.is_valid()) {
		//const int stream_block_size_po2 = _stream->get_block_size_po2();
		//_set_block_size_po2(stream_block_size_po2);

		// TODO We have to figure out streams that have a LOD requirement
		// const int stream_lod_count = _stream->get_lod_count();
		// _set_lod_count(min(stream_lod_count, get_lod_count()));

		if (_update_data->settings.full_load_mode && !_stream->supports_loading_all_blocks()) {
			ERR_PRINT("The chosen stream does not supports loading all blocks. Full load mode cannot be used.");
			_update_data->wait_for_end_of_task();
			_update_data->settings.full_load_mode = false;
#ifdef TOOLS_ENABLED
			notify_property_list_changed();
#endif
		}
	}

	VoxelServer::get_singleton().set_volume_data_block_size(_volume_id, get_data_block_size());
	VoxelServer::get_singleton().set_volume_render_block_size(_volume_id, get_mesh_block_size());

	reset_maps();
	// TODO Size other than 16 is not really supported though.
	// also this code isn't right, it doesnt update the other lods
	//_data->lods[0].map.create(p_block_size_po2, 0);

	if ((_stream.is_valid() || _generator.is_valid()) &&
			(Engine::get_singleton()->is_editor_hint() == false || _update_data->settings.run_stream_in_editor)) {
		start_streamer();
		start_updater();
	}

	_update_data->wait_for_end_of_task();
	_update_data->state.force_update_octrees_next_update = true;

	// The whole map might change, so make all area dirty
	for (unsigned int i = 0; i < _update_data->settings.lod_count; ++i) {
		VoxelLodTerrainUpdateData::Lod &lod = _update_data->state.lods[i];
		lod.last_view_distance_data_blocks = 0;
		lod.last_view_distance_mesh_blocks = 0;
	}

	update_configuration_warnings();
}

void VoxelLodTerrain::set_mesh_block_size(unsigned int mesh_block_size) {
	// Mesh block size cannot be smaller than data block size, for now
	mesh_block_size = math::clamp(mesh_block_size, get_data_block_size(), constants::MAX_BLOCK_SIZE);

	// Only these sizes are allowed at the moment. This stuff is still not supported in a generic way yet,
	// some code still exploits the fact it's a multiple of data block size, for performance
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

	_update_data->wait_for_end_of_task();
	_update_data->settings.mesh_block_size_po2 = po2;
	_update_data->state.force_update_octrees_next_update = true;

	VoxelLodTerrainUpdateData::State &state = _update_data->state;
	const unsigned int lod_count = _update_data->settings.lod_count;

	// Reset mesh maps
	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
		VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
		if (_instancer != nullptr) {
			// Unload instances
			VoxelInstancer *instancer = _instancer;
			mesh_map.for_each_block([lod_index, instancer](VoxelMeshBlockVLT &block) {
				instancer->on_mesh_block_exit(block.position, lod_index);
			});
		}
		// Unload mesh blocks
		mesh_map.for_each_block(BeforeUnloadMeshAction{ _shader_material_pool });
		mesh_map.clear();
		// Reset view distance cache so they will be re-entered
		lod.last_view_distance_mesh_blocks = 0;
	}

	// Doing this after because `on_mesh_block_exit` may use the old size
	if (_instancer != nullptr) {
		_instancer->set_mesh_block_size_po2(mesh_block_size);
	}

	// Reset LOD octrees
	LodOctree::NoDestroyAction nda;
	for (Map<Vector3i, VoxelLodTerrainUpdateData::OctreeItem>::Element *E = state.lod_octrees.front(); E;
			E = E->next()) {
		VoxelLodTerrainUpdateData::OctreeItem &item = E->value();
		item.octree.create(lod_count, nda);
	}

	VoxelServer::get_singleton().set_volume_render_block_size(_volume_id, mesh_block_size);

	// Update voxel bounds because block size change can affect octree size
	set_voxel_bounds(_update_data->settings.bounds_in_voxels);
}

void VoxelLodTerrain::set_full_load_mode_enabled(bool enabled) {
	if (enabled != _update_data->settings.full_load_mode) {
		_update_data->wait_for_end_of_task();
		_update_data->settings.full_load_mode = enabled;
		_update_data->state.force_update_octrees_next_update = true;
		_on_stream_params_changed();
	}
}

bool VoxelLodTerrain::is_full_load_mode_enabled() const {
	return _update_data->settings.full_load_mode;
}

void VoxelLodTerrain::set_threaded_update_enabled(bool enabled) {
	if (enabled != _threaded_update_enabled) {
		if (_threaded_update_enabled) {
			_update_data->wait_for_end_of_task();
		}
		_threaded_update_enabled = enabled;
	}
}

bool VoxelLodTerrain::is_threaded_update_enabled() const {
	return _threaded_update_enabled;
}

void VoxelLodTerrain::set_mesh_block_active(VoxelMeshBlockVLT &block, bool active) {
	if (block.active == active) {
		return;
	}

	block.active = active;

	if (_lod_fade_duration == 0.f) {
		block.set_visible(active);
		return;
	}

	VoxelMeshBlockVLT::FadingState fading_state;
	// Initial progress has to be set too because it sometimes happens that a LOD must appear before its parent
	// finished fading in. So the parent will have to fade out from solid with the same duration.
	float initial_progress;
	if (active) {
		block.set_visible(true);
		fading_state = VoxelMeshBlockVLT::FADING_IN;
		initial_progress = 0.f;
	} else {
		fading_state = VoxelMeshBlockVLT::FADING_OUT;
		initial_progress = 1.f;
	}

	if (block.fading_state != fading_state) {
		if (block.fading_state == VoxelMeshBlockVLT::FADING_NONE) {
			Map<Vector3i, VoxelMeshBlockVLT *> &fading_blocks = _fading_blocks_per_lod[block.lod_index];
			// Must not have duplicates
			ERR_FAIL_COND(fading_blocks.has(block.position));
			fading_blocks.insert(block.position, &block);
		}
		block.fading_state = fading_state;
		block.fading_progress = initial_progress;
	}
}

bool VoxelLodTerrain::is_area_editable(Box3i p_voxel_box) const {
	if (_update_data->settings.full_load_mode) {
		return true;
	}
	const Box3i voxel_box = p_voxel_box.clipped(_update_data->settings.bounds_in_voxels);
	VoxelDataLodMap::Lod &data_lod0 = _data->lods[0];
	{
		RWLockRead rlock(data_lod0.map_lock);
		const bool all_blocks_present = data_lod0.map.is_area_fully_loaded(voxel_box);
		return all_blocks_present;
	}
}

inline std::shared_ptr<VoxelBufferInternal> try_get_voxel_buffer_with_lock(
		const VoxelDataLodMap::Lod &data_lod, Vector3i block_pos) {
	RWLockRead rlock(data_lod.map_lock);
	const VoxelDataBlock *block = data_lod.map.get_block(block_pos);
	if (block == nullptr) {
		return nullptr;
	}
	return block->get_voxels_shared();
}

inline VoxelSingleValue get_voxel_with_lock(VoxelBufferInternal &vb, Vector3i pos, unsigned int channel) {
	VoxelSingleValue v;
	if (channel == VoxelBufferInternal::CHANNEL_SDF) {
		RWLockRead rlock(vb.get_lock());
		v.f = vb.get_voxel_f(pos.x, pos.y, pos.z, channel);
	} else {
		RWLockRead rlock(vb.get_lock());
		v.i = vb.get_voxel(pos, channel);
	}
	return v;
}

VoxelSingleValue VoxelLodTerrain::get_voxel(Vector3i pos, unsigned int channel, VoxelSingleValue defval) {
	if (!_update_data->settings.bounds_in_voxels.contains(pos)) {
		return defval;
	}

	Vector3i block_pos = pos >> get_data_block_size_pow2();

	if (_update_data->settings.full_load_mode) {
		const VoxelDataLodMap::Lod &data_lod0 = _data->lods[0];
		std::shared_ptr<VoxelBufferInternal> voxels = try_get_voxel_buffer_with_lock(data_lod0, block_pos);
		if (voxels == nullptr) {
			if (_generator.is_valid()) {
				return _generator->generate_single(pos, channel);
			}
		} else {
			const Vector3i rpos = data_lod0.map.to_local(pos);
			VoxelSingleValue v;
			RWLockRead rlock(voxels->get_lock());
			if (channel == VoxelBufferInternal::CHANNEL_SDF) {
				v.f = voxels->get_voxel_f(rpos.x, rpos.y, rpos.z, channel);
			} else {
				v.i = voxels->get_voxel(rpos, channel);
			}
			return v;
		}
		return defval;

	} else {
		Vector3i voxel_pos = pos;
		for (unsigned int lod_index = 0; lod_index < _update_data->settings.lod_count; ++lod_index) {
			const VoxelDataLodMap::Lod &data_lod = _data->lods[lod_index];
			std::shared_ptr<VoxelBufferInternal> voxels = try_get_voxel_buffer_with_lock(data_lod, block_pos);
			if (voxels != nullptr) {
				return get_voxel_with_lock(*voxels, data_lod.map.to_local(voxel_pos), channel);
			}
			// Fallback on lower LOD
			block_pos = block_pos >> 1;
			voxel_pos = voxel_pos >> 1;
		}
		return defval;
	}
}

bool VoxelLodTerrain::try_set_voxel_without_update(Vector3i pos, unsigned int channel, uint64_t value) {
	const Vector3i block_pos_lod0 = pos >> get_data_block_size_pow2();
	VoxelDataLodMap::Lod &data_lod0 = _data->lods[0];
	const Vector3i block_pos = data_lod0.map.voxel_to_block(pos);
	std::shared_ptr<VoxelBufferInternal> voxels = try_get_voxel_buffer_with_lock(data_lod0, block_pos);
	if (voxels == nullptr) {
		if (!_update_data->settings.full_load_mode) {
			return false;
		}
		if (_generator.is_valid()) {
			voxels = make_shared_instance<VoxelBufferInternal>();
			voxels->create(Vector3iUtil::create(get_data_block_size()));
			VoxelGenerator::VoxelQueryData q{ *voxels, pos, 0 };
			_generator->generate_block(q);
			RWLockWrite wlock(data_lod0.map_lock);
			if (data_lod0.map.has_block(block_pos_lod0)) {
				// A block was loaded by another thread, cancel our edit.
				return false;
			}
			data_lod0.map.set_block_buffer(block_pos_lod0, voxels, true);
		}
	}
	// If it turns out to be a problem, use CoW?
	RWLockWrite lock(voxels->get_lock());
	voxels->set_voxel(value, data_lod0.map.to_local(pos), channel);
	return true;
}

void VoxelLodTerrain::copy(Vector3i p_origin_voxels, VoxelBufferInternal &dst_buffer, uint8_t channels_mask) {
	const VoxelDataLodMap::Lod &data_lod0 = _data->lods[0];
	if (_update_data->settings.full_load_mode && _generator.is_valid()) {
		RWLockRead rlock(data_lod0.map_lock);
		data_lod0.map.copy(p_origin_voxels, dst_buffer, channels_mask, *_generator,
				[](void *callback_data, VoxelBufferInternal &voxels, Vector3i pos) {
					VoxelGenerator *generator = reinterpret_cast<VoxelGenerator *>(callback_data);
					VoxelGenerator::VoxelQueryData q{ voxels, pos, 0 };
					generator->generate_block(q);
				});
	} else {
		RWLockRead rlock(data_lod0.map_lock);
		data_lod0.map.copy(p_origin_voxels, dst_buffer, channels_mask);
	}
}

// Marks intersecting blocks in the area as modified, updates LODs and schedules remeshing.
// The provided box must be at LOD0 coordinates.
void VoxelLodTerrain::post_edit_area(Box3i p_box) {
	ZN_PROFILE_SCOPE();
	// TODO Better decoupling is needed here.
	// In the past this padding was necessary for mesh blocks because visuals depend on neighbor voxels.
	// So when editing voxels at the boundary of two mesh blocks, both must update.
	// However on data blocks it doesn't make sense, neighbors are not affected (at least for now).
	// this can cause false positive errors as if we were editing a block that's not loaded (coming up as null).
	// For now, this is worked around by ignoring cases where blocks are null,
	// But it might mip more lods than necessary when editing on borders.
	const Box3i box = p_box.padded(1);
	const Box3i bbox = box.downscaled(get_data_block_size());

	VoxelDataLodMap::Lod &data_lod0 = _data->lods[0];
	{
		RWLockRead rlock(data_lod0.map_lock);
		MutexLock lock(_update_data->state.blocks_pending_lodding_lod0_mutex);

		bbox.for_each_cell([this, &data_lod0](Vector3i block_pos_lod0) {
			VoxelDataBlock *block = data_lod0.map.get_block(block_pos_lod0);
			//ERR_FAIL_COND(block == nullptr);
			if (block == nullptr) {
				return;
			}

			//RWLockWrite wlock(block->get_voxels_shared()->get_lock());
			block->set_modified(true);

			// TODO That boolean is also modified by the threaded update task (always set to false)
			if (!block->get_needs_lodding()) {
				block->set_needs_lodding(true);
				_update_data->state.blocks_pending_lodding_lod0.push_back(block_pos_lod0);
			}
		});
	}

	if (_instancer != nullptr) {
		_instancer->on_area_edited(p_box);
	}
}

void VoxelLodTerrain::push_async_edit(IThreadedTask *task, Box3i box, std::shared_ptr<AsyncDependencyTracker> tracker) {
	CRASH_COND(task == nullptr);
	CRASH_COND(tracker == nullptr);

	VoxelLodTerrainUpdateData::AsyncEdit e;
	e.box = box;
	e.task = task;
	e.task_tracker = tracker;

	VoxelLodTerrainUpdateData::State &state = _update_data->state;
	MutexLock lock(state.pending_async_edits_mutex);
	state.pending_async_edits.push_back(e);
}

Ref<VoxelTool> VoxelLodTerrain::get_voxel_tool() {
	VoxelToolLodTerrain *vt = memnew(VoxelToolLodTerrain(this));
	// Set to most commonly used channel on this kind of terrain
	vt->set_channel(VoxelBufferInternal::CHANNEL_SDF);
	return Ref<VoxelTool>(vt);
}

int VoxelLodTerrain::get_view_distance() const {
	return _update_data->settings.view_distance_voxels;
}

// TODO Needs to be clamped dynamically, to avoid the user accidentally setting blowing up memory.
// It used to be clamped to a hardcoded value, but now it may depend on LOD count and boundaries
void VoxelLodTerrain::set_view_distance(int p_distance_in_voxels) {
	ERR_FAIL_COND(p_distance_in_voxels <= 0);
	// Note: this is a hint distance, the terrain will attempt to have this radius filled with loaded voxels.
	// It is possible for blocks to still load beyond that distance.
	_update_data->wait_for_end_of_task();
	_update_data->settings.view_distance_voxels = p_distance_in_voxels;
	_update_data->state.force_update_octrees_next_update = true;
}

void VoxelLodTerrain::start_updater() {
	Ref<VoxelMesherBlocky> blocky_mesher = _mesher;
	if (blocky_mesher.is_valid()) {
		Ref<VoxelBlockyLibrary> library = blocky_mesher->get_library();
		if (library.is_valid()) {
			// TODO Any way to execute this function just after the TRES resource loader has finished to load?
			// VoxelLibrary should be baked ahead of time, like MeshLibrary
			library->bake();
		}
	}

	VoxelServer::get_singleton().set_volume_mesher(_volume_id, _mesher);
}

void VoxelLodTerrain::stop_updater() {
	VoxelServer::get_singleton().set_volume_mesher(_volume_id, Ref<VoxelMesher>());

	// TODO We can still receive a few mesh delayed mesh updates after this. Is it a problem?
	//_reception_buffers.mesh_output.clear();

	_update_data->wait_for_end_of_task();

	for (unsigned int i = 0; i < _update_data->state.lods.size(); ++i) {
		VoxelLodTerrainUpdateData::Lod &lod = _update_data->state.lods[i];
		lod.blocks_pending_update.clear();

		for (auto it = lod.mesh_map_state.map.begin(); it != lod.mesh_map_state.map.end(); ++it) {
			VoxelLodTerrainUpdateData::MeshBlockState &mesh_block = it->second;
			if (mesh_block.state == VoxelLodTerrainUpdateData::MESH_UPDATE_SENT) {
				mesh_block.state = VoxelLodTerrainUpdateData::MESH_UPDATE_NOT_SENT;
			}
		}
	}
}

void VoxelLodTerrain::start_streamer() {
	VoxelServer::get_singleton().set_volume_stream(_volume_id, _stream);
	VoxelServer::get_singleton().set_volume_generator(_volume_id, _generator);

	if (_update_data->settings.full_load_mode && _stream.is_valid()) {
		// TODO May want to defer this to be sure it's not done multiple times.
		// This would be a side-effect of setting properties one by one, either by scene loader or by script
		VoxelServer::get_singleton().request_all_stream_blocks(_volume_id);
	}
}

void VoxelLodTerrain::stop_streamer() {
	VoxelServer::get_singleton().set_volume_stream(_volume_id, Ref<VoxelStream>());
	VoxelServer::get_singleton().set_volume_generator(_volume_id, Ref<VoxelGenerator>());

	_update_data->wait_for_end_of_task();

	for (unsigned int i = 0; i < _update_data->state.lods.size(); ++i) {
		VoxelLodTerrainUpdateData::Lod &lod = _update_data->state.lods[i];
		lod.loading_blocks.clear();
	}

	//_reception_buffers.data_output.clear();
}

void VoxelLodTerrain::set_lod_distance(float p_lod_distance) {
	if (p_lod_distance == _update_data->settings.lod_distance) {
		return;
	}

	_update_data->wait_for_end_of_task();

	// Distance must be greater than a threshold,
	// otherwise lods will decimate too fast and it will look messy
	const float lod_distance =
			math::clamp(p_lod_distance, constants::MINIMUM_LOD_DISTANCE, constants::MAXIMUM_LOD_DISTANCE);
	_update_data->settings.lod_distance = lod_distance;
	_update_data->state.force_update_octrees_next_update = true;
	VoxelServer::get_singleton().set_volume_octree_lod_distance(_volume_id, get_lod_distance());

	if (_instancer != nullptr) {
		_instancer->set_mesh_lod_distance(lod_distance);
	}
}

float VoxelLodTerrain::get_lod_distance() const {
	return _update_data->settings.lod_distance;
}

void VoxelLodTerrain::set_lod_count(int p_lod_count) {
	ERR_FAIL_COND(p_lod_count >= (int)constants::MAX_LOD);
	ERR_FAIL_COND(p_lod_count < 1);

	if (get_lod_count() != p_lod_count) {
		_set_lod_count(p_lod_count);
	}
}

void VoxelLodTerrain::_set_lod_count(int p_lod_count) {
	CRASH_COND(p_lod_count >= (int)constants::MAX_LOD);
	CRASH_COND(p_lod_count < 1);

	_update_data->wait_for_end_of_task();

	_update_data->settings.lod_count = p_lod_count;
	_update_data->state.force_update_octrees_next_update = true;

	LodOctree::NoDestroyAction nda;

	for (Map<Vector3i, VoxelLodTerrainUpdateData::OctreeItem>::Element *E = _update_data->state.lod_octrees.front(); E;
			E = E->next()) {
		VoxelLodTerrainUpdateData::OctreeItem &item = E->value();
		item.octree.create(p_lod_count, nda);
	}

	// Not entirely required, but changing LOD count at runtime is rarely needed
	reset_maps();
}

void VoxelLodTerrain::reset_maps() {
	// Clears all blocks and reconfigures maps to account for new LOD count and block sizes

	// Don't reset while streaming, the result can be dirty?
	//CRASH_COND(_stream_thread != nullptr);

	_update_data->wait_for_end_of_task();

	const unsigned int lod_count = _update_data->settings.lod_count;
	VoxelLodTerrainUpdateData::State &state = _update_data->state;

	// Make a new one, so if threads still reference the old one it will be a different copy
	_data = make_shared_instance<VoxelDataLodMap>();
	_data->lod_count = lod_count;

	for (unsigned int lod_index = 0; lod_index < state.lods.size(); ++lod_index) {
		VoxelDataLodMap::Lod &data_lod = _data->lods[lod_index];
		// Instance new maps if we have more lods, or clear them otherwise
		if (lod_index < lod_count) {
			data_lod.map.create(data_lod.map.get_block_size_pow2(), lod_index);
		} else {
			data_lod.map.clear();
		}
	}

	for (unsigned int lod_index = 0; lod_index < state.lods.size(); ++lod_index) {
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
		VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];

		// Instance new maps if we have more lods, or clear them otherwise
		if (lod_index < lod_count) {
			mesh_map.clear();
			// Reset view distance cache so blocks will be re-entered due to the difference
			lod.last_view_distance_data_blocks = 0;
			lod.last_view_distance_mesh_blocks = 0;
		} else {
			mesh_map.clear();
		}

		lod.mesh_map_state.map.clear();

		lod.mesh_blocks_to_activate.clear();
		lod.mesh_blocks_to_deactivate.clear();
		lod.mesh_blocks_to_unload.clear();
		lod.mesh_blocks_to_update_transitions.clear();

		_deferred_collision_updates_per_lod[lod_index].clear();
	}

	abort_async_edits();

	// Reset previous state caches to force rebuilding the view area
	state.last_octree_region_box = Box3i();
	state.lod_octrees.clear();
}

int VoxelLodTerrain::get_lod_count() const {
	return _update_data->settings.lod_count;
}

void VoxelLodTerrain::set_generate_collisions(bool enabled) {
	_generate_collisions = enabled;
}

void VoxelLodTerrain::set_collision_lod_count(int lod_count) {
	ERR_FAIL_COND(lod_count < 0);
	_collision_lod_count = static_cast<unsigned int>(math::min(lod_count, get_lod_count()));
}

int VoxelLodTerrain::get_collision_lod_count() const {
	return _collision_lod_count;
}

void VoxelLodTerrain::set_collision_layer(int layer) {
	const unsigned int lod_count = _update_data->settings.lod_count;

	_collision_layer = layer;
	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
		mesh_map.for_each_block([layer](VoxelMeshBlockVLT &block) { //
			block.set_collision_layer(layer);
		});
	}
}

int VoxelLodTerrain::get_collision_layer() const {
	return _collision_layer;
}

void VoxelLodTerrain::set_collision_mask(int mask) {
	const unsigned int lod_count = _update_data->settings.lod_count;

	_collision_mask = mask;
	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
		mesh_map.for_each_block([mask](VoxelMeshBlockVLT &block) { //
			block.set_collision_mask(mask);
		});
	}
}

int VoxelLodTerrain::get_collision_mask() const {
	return _collision_mask;
}

void VoxelLodTerrain::set_collision_margin(float margin) {
	const unsigned int lod_count = _update_data->settings.lod_count;

	_collision_margin = margin;
	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
		mesh_map.for_each_block([margin](VoxelMeshBlockVLT &block) { //
			block.set_collision_margin(margin);
		});
	}
}

float VoxelLodTerrain::get_collision_margin() const {
	return _collision_margin;
}

int VoxelLodTerrain::get_data_block_region_extent() const {
	return VoxelServer::get_octree_lod_block_region_extent(_update_data->settings.lod_distance, get_data_block_size());
}

int VoxelLodTerrain::get_mesh_block_region_extent() const {
	return VoxelServer::get_octree_lod_block_region_extent(_update_data->settings.lod_distance, get_mesh_block_size());
}

Vector3i VoxelLodTerrain::voxel_to_data_block_position(Vector3 vpos, int lod_index) const {
	ERR_FAIL_COND_V(lod_index < 0, Vector3i());
	ERR_FAIL_COND_V(lod_index >= get_lod_count(), Vector3i());
	const VoxelDataLodMap::Lod &lod = _data->lods[lod_index];
	const Vector3i bpos = lod.map.voxel_to_block(math::floor_to_int(vpos)) >> lod_index;
	return bpos;
}

Vector3i VoxelLodTerrain::voxel_to_mesh_block_position(Vector3 vpos, int lod_index) const {
	ERR_FAIL_COND_V(lod_index < 0, Vector3i());
	ERR_FAIL_COND_V(lod_index >= get_lod_count(), Vector3i());
	const unsigned int mesh_block_size_po2 = _update_data->settings.mesh_block_size_po2;
	const Vector3i bpos = (math::floor_to_int(vpos) >> mesh_block_size_po2) >> lod_index;
	return bpos;
}

void VoxelLodTerrain::set_process_callback(ProcessCallback mode) {
	_process_callback = mode;
	set_process(_process_callback == PROCESS_CALLBACK_IDLE);
	set_physics_process(_process_callback == PROCESS_CALLBACK_PHYSICS);
}

void VoxelLodTerrain::_notification(int p_what) {
	switch (p_what) {
		// TODO Should use NOTIFICATION_INTERNAL_PROCESS instead?
		case NOTIFICATION_PROCESS:
			if (_process_callback == PROCESS_CALLBACK_IDLE) {
				// Can't do that in enter tree because Godot is "still setting up children".
				// Can't do that in ready either because Godot says node state is locked.
				// This hack is quite miserable.
				VoxelServerUpdater::ensure_existence(get_tree());
				_process(get_process_delta_time());
			}
			break;

		// TODO Should use NOTIFICATION_INTERNAL_PHYSICS_PROCESS instead?
		case NOTIFICATION_PHYSICS_PROCESS:
			if (_process_callback == PROCESS_CALLBACK_PHYSICS) {
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
			World3D *world = *get_world_3d();
			VoxelLodTerrainUpdateData::State &state = _update_data->state;
			for (unsigned int lod_index = 0; lod_index < state.lods.size(); ++lod_index) {
				VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
				mesh_map.for_each_block([world](VoxelMeshBlockVLT &block) { //
					block.set_world(world);
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
			VoxelLodTerrainUpdateData::State &state = _update_data->state;
			for (unsigned int lod_index = 0; lod_index < state.lods.size(); ++lod_index) {
				VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
				mesh_map.for_each_block([](VoxelMeshBlockVLT &block) { //
					block.set_world(nullptr);
				});
			}
#ifdef TOOLS_ENABLED
			_debug_renderer.set_world(nullptr);
#endif
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			const bool visible = is_visible();
			VoxelLodTerrainUpdateData::State &state = _update_data->state;

			for (unsigned int lod_index = 0; lod_index < state.lods.size(); ++lod_index) {
				VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
				mesh_map.for_each_block([visible](VoxelMeshBlockVLT &block) { //
					block.set_parent_visible(visible);
				});
			}

#ifdef TOOLS_ENABLED
			if (is_showing_gizmos()) {
				_debug_renderer.set_world(is_visible_in_tree() ? *get_world_3d() : nullptr);
			}
#endif
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			ZN_PROFILE_SCOPE_NAMED("VoxelLodTerrain::NOTIFICATION_TRANSFORM_CHANGED");

			const Transform3D transform = get_global_transform();
			VoxelServer::get_singleton().set_volume_transform(_volume_id, transform);

			if (!is_inside_tree()) {
				// The transform and other properties can be set by the scene loader,
				// before we enter the tree
				return;
			}

			VoxelLodTerrainUpdateData::State &state = _update_data->state;

			for (unsigned int lod_index = 0; lod_index < state.lods.size(); ++lod_index) {
				VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
				mesh_map.for_each_block([&transform](VoxelMeshBlockVLT &block) { //
					block.set_parent_transform(transform);
				});
			}
		} break;

		default:
			break;
	}
}

Vector3 VoxelLodTerrain::get_local_viewer_pos() const {
	// Pick this by default
	Vector3 pos = _update_data->state.lods[0].last_viewer_data_block_pos << get_data_block_size_pow2();

	// TODO Support for multiple viewers, this is a placeholder implementation
	VoxelServer::get_singleton().for_each_viewer( //
			[&pos](const VoxelServer::Viewer &viewer, uint32_t viewer_id) { //
				pos = viewer.world_position;
			});

	const Transform3D world_to_local = get_global_transform().affine_inverse();
	pos = world_to_local.xform(pos);
	return pos;
}

inline bool check_block_sizes(int data_block_size, int mesh_block_size) {
	return (data_block_size == 16 || data_block_size == 32) && (mesh_block_size == 16 || mesh_block_size == 32) &&
			mesh_block_size >= data_block_size;
}

// void VoxelLodTerrain::send_block_save_requests(Span<BlockToSave> blocks_to_save) {
// 	for (unsigned int i = 0; i < blocks_to_save.size(); ++i) {
// 		BlockToSave &b = blocks_to_save[i];
// 		ZN_PRINT_VERBOSE(String("Requesting save of block {0} lod {1}").format(varray(b.position, b.lod)));
// 		VoxelServer::get_singleton().request_voxel_block_save(_volume_id, b.voxels, b.position, b.lod);
// 	}
// }

void VoxelLodTerrain::_process(float delta) {
	ZN_PROFILE_SCOPE();

	_stats.dropped_block_loads = 0;
	_stats.dropped_block_meshs = 0;

	if (get_lod_count() == 0) {
		// If there isn't a LOD 0, there is nothing to load
		return;
	}

	// Get block loading responses
	// Note: if block loading is too fast, this can cause stutters.
	// It should only happen on first load, though.
	//process_block_loading_responses();

	process_fading_blocks(delta);

	// TODO This could go into time spread tasks too
	process_deferred_collision_updates(VoxelServer::get_singleton().get_main_thread_time_budget_usec());

#ifdef TOOLS_ENABLED
	if (is_showing_gizmos() && is_visible_in_tree()) {
		update_gizmos();
	}
#endif

	if (_update_data->task_is_complete) {
		ZN_PROFILE_SCOPE();

		apply_main_thread_update_tasks();

		// Get viewer location in voxel space
		const Vector3 viewer_pos = get_local_viewer_pos();

		// TODO Optimization: pool tasks instead of allocating?
		VoxelLodTerrainUpdateTask *task = memnew(VoxelLodTerrainUpdateTask(_data, _update_data, _streaming_dependency,
				_meshing_dependency, VoxelServer::get_singleton().get_shared_viewers_data_from_default_world(),
				viewer_pos, _instancer != nullptr, _volume_id, get_global_transform()));

		_update_data->task_is_complete = false;

		if (_threaded_update_enabled) {
			// Schedule task at the end, so it is less likely to have contention with other logic than if it was done at
			// the beginnning of `_process`
			VoxelServer::get_singleton().push_async_task(task);

		} else {
			task->run(ThreadedTaskContext{ 0 });
			memdelete(task);
			apply_main_thread_update_tasks();
		}
	}
}

void VoxelLodTerrain::apply_main_thread_update_tasks() {
	ZN_PROFILE_SCOPE();
	// Dequeue outputs of the threadable part of the update for actions taking place on the main thread

	CRASH_COND(_update_data->task_is_complete == false);

	VoxelLodTerrainUpdateData::State &state = _update_data->state;

	for (unsigned int lod_index = 0; lod_index < _update_data->settings.lod_count; ++lod_index) {
		VoxelLodTerrainUpdateData::Lod &lod = _update_data->state.lods[lod_index];
		VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];

		for (unsigned int i = 0; i < lod.mesh_blocks_to_activate.size(); ++i) {
			const Vector3i bpos = lod.mesh_blocks_to_activate[i];
			VoxelMeshBlockVLT *block = mesh_map.get_block(bpos);
			// Can be null if there is actually no surface at this location
			if (block == nullptr) {
				continue;
			}
			//ERR_CONTINUE(block == nullptr);
			set_mesh_block_active(*block, true);
		}

		for (unsigned int i = 0; i < lod.mesh_blocks_to_deactivate.size(); ++i) {
			const Vector3i bpos = lod.mesh_blocks_to_deactivate[i];
			VoxelMeshBlockVLT *block = mesh_map.get_block(bpos);
			// Can be null if there is actually no surface at this location
			if (block == nullptr) {
				continue;
			}
			//ERR_CONTINUE(block == nullptr);
			set_mesh_block_active(*block, false);
		}

		lod.mesh_blocks_to_activate.clear();
		lod.mesh_blocks_to_deactivate.clear();

		/*
#ifdef DEBUG_ENABLED
		std::unordered_set<Vector3i> debug_removed_blocks;
#endif
		*/

		for (unsigned int i = 0; i < lod.mesh_blocks_to_unload.size(); ++i) {
			const Vector3i bpos = lod.mesh_blocks_to_unload[i];
			mesh_map.remove_block(bpos, BeforeUnloadMeshAction{ _shader_material_pool });

			_fading_blocks_per_lod[lod_index].erase(bpos);

			if (_instancer != nullptr) {
				_instancer->on_mesh_block_exit(bpos, lod_index);
			}
			/*
#ifdef DEBUG_ENABLED
			debug_removed_blocks.insert(bpos);
#endif
			*/
			// Blocks in the update queue will be cancelled in _process,
			// because it's too expensive to linear-search all blocks for each block
		}

		for (unsigned int i = 0; i < lod.mesh_blocks_to_update_transitions.size(); ++i) {
			const VoxelLodTerrainUpdateData::TransitionUpdate tu = lod.mesh_blocks_to_update_transitions[i];
			VoxelMeshBlockVLT *block = mesh_map.get_block(tu.block_position);
			// Can be null if there is actually no surface at this location
			if (block == nullptr) {
				/*
#ifdef DEBUG_ENABLED
				// If the block was removed for a different reason then it is unexpected
				ERR_CONTINUE(debug_removed_blocks.find(tu.block_position) == debug_removed_blocks.end());
#endif
				ZN_PRINT_VERBOSE(String("Skipping TransitionUpdate at {0} lod {1}, block not found")
									  .format(varray(tu.block_position, lod_index)));
				*/
				continue;
			}
			//CRASH_COND(block == nullptr);
			if (block->active) {
				block->set_transition_mask(tu.transition_mask);
			}
		}

		lod.mesh_blocks_to_unload.clear();
		lod.mesh_blocks_to_update_transitions.clear();
	}

	// Remove completed async edits
	unordered_remove_if(state.running_async_edits, [this](VoxelLodTerrainUpdateData::RunningAsyncEdit &e) {
		if (e.tracker->is_complete()) {
			if (e.tracker->has_next_tasks()) {
				ERR_PRINT("Completed async edit had next tasks?");
			}
			post_edit_area(e.box);
			return true;

		} else if (e.tracker->is_aborted()) {
			return true;
		}

		return false;
	});

	_stats.blocked_lods = state.stats.blocked_lods;
	_stats.time_detect_required_blocks = state.stats.time_detect_required_blocks;
	_stats.time_io_requests = state.stats.time_io_requests;
	_stats.time_mesh_requests = state.stats.time_mesh_requests;
	_stats.time_update_task = state.stats.time_total;
}

template <typename T>
bool thread_safe_contains(const std::unordered_set<T> &set, T v, BinaryMutex &mutex) {
	MutexLock lock(mutex);
	typename std::unordered_set<T>::const_iterator it = set.find(v);
	return it != set.end();
}

void VoxelLodTerrain::apply_data_block_response(VoxelServer::BlockDataOutput &ob) {
	ZN_PROFILE_SCOPE();

	if (ob.type == VoxelServer::BlockDataOutput::TYPE_SAVED) {
		// That's a save confirmation event.
		// Note: in the future, if blocks don't get copied before being sent for saving,
		// we will need to use block versionning to know when we can reset the `modified` flag properly

		// TODO Now that's the case. Use version? Or just keep copying?
		return;
	}

	if (ob.lod >= _update_data->settings.lod_count) {
		// That block was requested at a time where LOD was higher... drop it
		++_stats.dropped_block_loads;
		return;
	}

	// Initial load will be true when we requested data without specifying specific positions,
	// so we wouldn't know which ones to expect. This is the case of full load mode.
	VoxelLodTerrainUpdateData::Lod &lod = _update_data->state.lods[ob.lod];
	if (!ob.initial_load) {
		if (!thread_safe_contains(lod.loading_blocks, ob.position, lod.loading_blocks_mutex)) {
			// That block was not requested, or is no longer needed. drop it...
			ZN_PRINT_VERBOSE(format("Ignoring block {} lod {}, it was not in loading blocks", ob.position, ob.lod));
			++_stats.dropped_block_loads;
			return;
		}
	}

	if (ob.dropped) {
		// That block was dropped by the data loader thread, but we were still expecting it...
		// This is most likely caused by the loader not keeping up with the speed at which the player is moving.
		// We should recover with the removal from `loading_blocks` so it will be re-queried again later...

		//				print_line(String("Received a block loading drop while we were still expecting it: lod{0} ({1},
		//{2}, {3})") 								   .format(varray(ob.lod, ob.position.x, ob.position.y,
		//ob.position.z)));

		++_stats.dropped_block_loads;
		return;
	}

	if (ob.voxels != nullptr) {
		VoxelDataLodMap::Lod &data_lod = _data->lods[ob.lod];

		if (ob.voxels->get_size() != Vector3iUtil::create(data_lod.map.get_block_size())) {
			// Voxel block size is incorrect, drop it
			ERR_PRINT("Block size obtained from stream is different from expected size");
			++_stats.dropped_block_loads;
			return;
		}

		// Store buffer
		RWLockWrite wlock(data_lod.map_lock);
		VoxelDataBlock *block = data_lod.map.set_block_buffer(ob.position, ob.voxels, false);
		CRASH_COND(block == nullptr);
		block->set_edited(ob.type == VoxelServer::BlockDataOutput::TYPE_LOADED);
	}

	{
		// We have to do this after adding the block to the map, otherwise there would be a small period of time where
		// the threaded update task could request the block again needlessly
		MutexLock lock(lod.loading_blocks_mutex);
		lod.loading_blocks.erase(ob.position);
	}

	if (_instancer != nullptr && ob.instances != nullptr) {
		_instancer->on_data_block_loaded(ob.position, ob.lod, std::move(ob.instances));
	}
}

void VoxelLodTerrain::apply_mesh_update(const VoxelServer::BlockMeshOutput &ob) {
	// The following is done on the main thread because Godot doesn't really support multithreaded Mesh allocation.
	// This also proved to be very slow compared to the meshing process itself...
	// hopefully Vulkan will allow us to upload graphical resources without stalling rendering as they upload?
	ZN_PROFILE_SCOPE();

	ERR_FAIL_COND(!is_inside_tree());

	CRASH_COND(_update_data == nullptr);
	VoxelLodTerrainUpdateData &update_data = *_update_data;

	if (ob.lod >= update_data.settings.lod_count) {
		// Sorry, LOD configuration changed, drop that mesh
		++_stats.dropped_block_meshs;
		return;
	}

	uint8_t transition_mask;
	bool active;
	{
		VoxelLodTerrainUpdateData::Lod &lod = update_data.state.lods[ob.lod];
		RWLockRead rlock(lod.mesh_map_state.map_lock);
		auto mesh_block_state_it = lod.mesh_map_state.map.find(ob.position);
		if (mesh_block_state_it == lod.mesh_map_state.map.end()) {
			// That block is no longer loaded in the update map, drop the result
			++_stats.dropped_block_meshs;
			return;
		}
		if (ob.type == VoxelServer::BlockMeshOutput::TYPE_DROPPED) {
			// That block is loaded, but its meshing request was dropped.
			// TODO Not sure what to do in this case, the code sending update queries has to be tweaked
			ZN_PRINT_VERBOSE("Received a block mesh drop while we were still expecting it");
			++_stats.dropped_block_meshs;
			return;
		}
		transition_mask = mesh_block_state_it->second.transition_mask;
		// The update task could be running at the same time, so we need to do this atomically.
		// The state can become "up to date" only if no other unsent update was pending.
		VoxelLodTerrainUpdateData::MeshState expected = VoxelLodTerrainUpdateData::MESH_UPDATE_SENT;
		mesh_block_state_it->second.state.compare_exchange_strong(expected, VoxelLodTerrainUpdateData::MESH_UP_TO_DATE);
		active = mesh_block_state_it->second.active;
	}

	// -------- Part where we invoke Godot functions ---------
	// As far as I know, this is not yet threadable efficiently, for the most part.
	// By that, I mean being able to call into RenderingServer and PhysicsServer,
	// without inducing a stall of the main thread.

	VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[ob.lod];
	VoxelMeshBlockVLT *block = mesh_map.get_block(ob.position);

	const VoxelMesher::Output &mesh_data = ob.surfaces;

	Ref<ArrayMesh> mesh =
			build_mesh(to_span_const(mesh_data.surfaces), mesh_data.primitive_type, mesh_data.mesh_flags, _material);

	if (mesh.is_null()) {
		if (block != nullptr) {
			// No surface anymore in this block, destroy it
			// TODO Factor removal in a function, it's done in a few places
			mesh_map.remove_block(ob.position, BeforeUnloadMeshAction{ _shader_material_pool });
			if (_instancer != nullptr) {
				_instancer->on_mesh_block_exit(ob.position, ob.lod);
			}
		}
		return;
	}

	if (block == nullptr) {
		block = memnew(VoxelMeshBlockVLT(ob.position, get_mesh_block_size(), ob.lod));
		block->active = active;
		block->set_visible(active);
		mesh_map.set_block(ob.position, block);
	}

	bool has_collision = _generate_collisions;
	if (has_collision && _collision_lod_count != 0) {
		has_collision = ob.lod < _collision_lod_count;
	}

	// TODO Is this boolean needed anymore now that we create blocks only if a surface is present?
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
		block->set_world(get_world_3d());

		Ref<ShaderMaterial> shader_material = _material;
		if (shader_material.is_valid() && block->get_shader_material().is_null()) {
			ZN_PROFILE_SCOPE_NAMED("Add ShaderMaterial");

			// Pooling shader materials is necessary for now, to avoid stuttering in the editor.
			// Due to a signal used to keep the inspector up to date, even though these
			// material copies will never be seen in the inspector
			// See https://github.com/godotengine/godot/issues/34741
			Ref<ShaderMaterial> sm;
			if (_shader_material_pool.size() > 0) {
				sm = _shader_material_pool.back();
				// The joys of pooling materials
				sm->set_shader_param(VoxelStringNames::get_singleton().u_transition_mask, 0);
				_shader_material_pool.pop_back();
			} else {
				sm = shader_material->duplicate(false);
			}

			// Set individual shader material, because each block can have dynamic parameters,
			// used to smooth seams without re-uploading meshes and allow to implement LOD fading
			block->set_shader_material(sm);
		}

		block->set_transition_mask(transition_mask);
	}

	block->set_mesh(mesh, DirectMeshInstance::GIMode(get_gi_mode()));
	{
		ZN_PROFILE_SCOPE_NAMED("Transition meshes");
		for (unsigned int dir = 0; dir < mesh_data.transition_surfaces.size(); ++dir) {
			Ref<ArrayMesh> transition_mesh = build_mesh(to_span_const(mesh_data.transition_surfaces[dir]),
					mesh_data.primitive_type, mesh_data.mesh_flags, _material);

			block->set_transition_mesh(transition_mesh, dir, DirectMeshInstance::GIMode(get_gi_mode()));
		}
	}

	const uint32_t now = get_ticks_msec();
	if (has_collision) {
		if (_collision_update_delay == 0 ||
				static_cast<int>(now - block->last_collider_update_time) > _collision_update_delay) {
			block->set_collision_mesh(to_span_const(mesh_data.surfaces), get_tree()->is_debugging_collisions_hint(),
					this, _collision_margin);
			block->set_collision_layer(_collision_layer);
			block->set_collision_mask(_collision_mask);
			block->last_collider_update_time = now;
			block->has_deferred_collider_update = false;
			block->deferred_collider_data.clear();
		} else {
			if (!block->has_deferred_collider_update) {
				_deferred_collision_updates_per_lod[ob.lod].push_back(ob.position);
				block->has_deferred_collider_update = true;
			}
			// TODO Optimization: could avoid the small allocation.
			// It's usually a small vectors with a handful of elements.
			// The caller providing `mesh_data` doesnt use `mesh_data` later so we could have moved the vector,
			// but at the moment it's passed with `const` so that isn't possible. Indeed we are only going to read the
			// data, but `const` also means the structure holding it is read-only as well.
			block->deferred_collider_data = mesh_data.surfaces;
		}
	}

	block->set_parent_transform(get_global_transform());
}

void VoxelLodTerrain::process_deferred_collision_updates(uint32_t timeout_msec) {
	ZN_PROFILE_SCOPE();

	const unsigned int lod_count = _update_data->settings.lod_count;

	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
		std::vector<Vector3i> &deferred_collision_updates = _deferred_collision_updates_per_lod[lod_index];

		for (unsigned int i = 0; i < deferred_collision_updates.size(); ++i) {
			const Vector3i block_pos = deferred_collision_updates[i];
			VoxelMeshBlockVLT *block = mesh_map.get_block(block_pos);

			if (block == nullptr || block->has_deferred_collider_update == false) {
				// Block was unloaded or no longer needs a collision update
				unordered_remove(deferred_collision_updates, i);
				--i;
				continue;
			}

			const uint32_t now = get_ticks_msec();

			if (static_cast<int>(now - block->last_collider_update_time) > _collision_update_delay) {
				block->set_collision_mesh(to_span_const(block->deferred_collider_data),
						get_tree()->is_debugging_collisions_hint(), this, _collision_margin);
				block->set_collision_layer(_collision_layer);
				block->set_collision_mask(_collision_mask);
				block->last_collider_update_time = now;
				block->has_deferred_collider_update = false;
				block->deferred_collider_data.clear();

				unordered_remove(deferred_collision_updates, i);
				--i;
			}

			// We always process at least one, then we to check the timeout
			if (get_ticks_msec() >= timeout_msec) {
				return;
			}
		}
	}
}

void VoxelLodTerrain::abort_async_edits() {
	_update_data->wait_for_end_of_task();
	VoxelLodTerrainUpdateData::State &state = _update_data->state;

	for (auto it = state.pending_async_edits.begin(); it != state.pending_async_edits.end(); ++it) {
		VoxelLodTerrainUpdateData::AsyncEdit &e = *it;
		CRASH_COND(e.task == nullptr);
		memdelete(e.task);
	}
	state.pending_async_edits.clear();
	state.running_async_edits.clear();
	// Can't cancel edits which are already running on the thread pool,
	// so the caller of this function must ensure none of them are running, or none will have an effect
}

void VoxelLodTerrain::process_fading_blocks(float delta) {
	ZN_PROFILE_SCOPE();

	const float speed = _lod_fade_duration < 0.001f ? 99999.f : delta / _lod_fade_duration;

	for (unsigned int lod_index = 0; lod_index < _fading_blocks_per_lod.size(); ++lod_index) {
		Map<Vector3i, VoxelMeshBlockVLT *> &fading_blocks = _fading_blocks_per_lod[lod_index];

		Map<Vector3i, VoxelMeshBlockVLT *>::Element *e = fading_blocks.front();

		while (e != nullptr) {
			VoxelMeshBlockVLT *block = e->value();
			// The collection of fading blocks must only contain fading blocks
			ERR_FAIL_COND(block->fading_state == VoxelMeshBlockVLT::FADING_NONE);

			const bool finished = block->update_fading(speed);

			if (finished) {
				Map<Vector3i, VoxelMeshBlockVLT *>::Element *next = e->next();
				fading_blocks.erase(e);
				e = next;

			} else {
				e = e->next();
			}
		}
	}
}

void VoxelLodTerrain::set_instancer(VoxelInstancer *instancer) {
	if (_instancer != nullptr && instancer != nullptr) {
		ERR_FAIL_COND_MSG(_instancer != nullptr, "No more than one VoxelInstancer per terrain");
	}
	_instancer = instancer;
}

// This function is primarily intented for editor use cases at the moment.
// It will be slower than using the instancing generation events,
// because it has to query VisualServer, which then allocates and decodes vertex buffers (assuming they are cached).
Array VoxelLodTerrain::get_mesh_block_surface(Vector3i block_pos, int lod_index) const {
	ZN_PROFILE_SCOPE();

	const int lod_count = _update_data->settings.lod_count;
	ERR_FAIL_COND_V(lod_index < 0 || lod_index >= lod_count, Array());

	const VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];

	Ref<Mesh> mesh;
	{
		const VoxelMeshBlockVLT *block = mesh_map.get_block(block_pos);
		if (block != nullptr) {
			mesh = block->get_mesh();
		}
	}

	if (mesh.is_valid()) {
		return mesh->surface_get_arrays(0);
	}

	return Array();
}

void VoxelLodTerrain::get_meshed_block_positions_at_lod(int lod_index, std::vector<Vector3i> &out_positions) const {
	const int lod_count = _update_data->settings.lod_count;
	ERR_FAIL_COND(lod_index < 0 || lod_index >= lod_count);

	const VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];

	mesh_map.for_each_block([&out_positions](const VoxelMeshBlockVLT &block) {
		if (block.has_mesh()) {
			out_positions.push_back(block.position);
		}
	});
}

void VoxelLodTerrain::save_all_modified_blocks(bool with_copy) {
	ZN_PROFILE_SCOPE();

	// This is often called before quitting the game or forcing a global save.
	// This could be part of the update task if async, but here we want it to be immediate.
	_update_data->wait_for_end_of_task();

	VoxelLodTerrainUpdateTask::flush_pending_lod_edits(
			_update_data->state, *_data, _generator, _update_data->settings.full_load_mode, get_mesh_block_size());

	std::vector<VoxelLodTerrainUpdateData::BlockToSave> blocks_to_save;

	if (_stream.is_valid()) {
		for (unsigned int i = 0; i < _data->lod_count; ++i) {
			VoxelDataLodMap::Lod &data_lod = _data->lods[i];
			RWLockRead rlock(data_lod.map_lock);
			// That may cause a stutter, so should be used when the player won't notice
			data_lod.map.for_each_block(ScheduleSaveAction{ blocks_to_save });
		}

		if (_instancer != nullptr && _stream->supports_instance_blocks()) {
			_instancer->save_all_modified_blocks();
		}
	}

	// And flush immediately
	BufferedTaskScheduler task_scheduler;
	VoxelLodTerrainUpdateTask::send_block_save_requests(
			_volume_id, to_span(blocks_to_save), _streaming_dependency, get_data_block_size(), task_scheduler);
	task_scheduler.flush();
}

const VoxelLodTerrain::Stats &VoxelLodTerrain::get_stats() const {
	return _stats;
}

Dictionary VoxelLodTerrain::_b_get_statistics() const {
	Dictionary d;

	// const unsigned int lod_count = _update_data->settings.lod_count;

	// int deferred_collision_updates = 0;
	// for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
	// 	deferred_collision_updates += _deferred_collision_updates_per_lod[lod_index].size();
	// }

	// Breakdown of information and time spent in _process and the update task.

	// Update task
	d["time_detect_required_blocks"] = _stats.time_detect_required_blocks;
	d["time_io_requests"] = _stats.time_io_requests;
	d["time_mesh_requests"] = _stats.time_mesh_requests;
	d["time_update_task"] = _stats.time_update_task;
	d["blocked_lods"] = _stats.blocked_lods;

	// Process
	d["dropped_block_loads"] = _stats.dropped_block_loads;
	d["dropped_block_meshs"] = _stats.dropped_block_meshs;

	return d;
}

void VoxelLodTerrain::set_run_stream_in_editor(bool enable) {
	if (enable == _update_data->settings.run_stream_in_editor) {
		return;
	}

	_update_data->wait_for_end_of_task();
	_update_data->settings.run_stream_in_editor = enable;

	if (Engine::get_singleton()->is_editor_hint()) {
		if (enable) {
			_on_stream_params_changed();

		} else {
			// This is expected to block the main thread until the streaming thread is done.
			stop_streamer();
		}
	}
}

bool VoxelLodTerrain::is_stream_running_in_editor() const {
	return _update_data->settings.run_stream_in_editor;
}

void VoxelLodTerrain::restart_stream() {
	_on_stream_params_changed();
}

void VoxelLodTerrain::remesh_all_blocks() {
	_update_data->wait_for_end_of_task();
	for (unsigned int lod_index = 0; lod_index < _update_data->settings.lod_count; ++lod_index) {
		VoxelLodTerrainUpdateData::Lod &lod = _update_data->state.lods[lod_index];
		for (auto it = lod.mesh_map_state.map.begin(); it != lod.mesh_map_state.map.end(); ++it) {
			VoxelLodTerrainUpdateTask::schedule_mesh_update(it->second, it->first, lod.blocks_pending_update);
		}
	}
}

void VoxelLodTerrain::set_voxel_bounds(Box3i p_box) {
	_update_data->wait_for_end_of_task();
	Box3i bounds_in_voxels =
			p_box.clipped(Box3i::from_center_extents(Vector3i(), Vector3iUtil::create(constants::MAX_VOLUME_EXTENT)));
	// Round to octree size
	const int octree_size = get_mesh_block_size() << (get_lod_count() - 1);
	bounds_in_voxels = bounds_in_voxels.snapped(octree_size);
	// Can't have a smaller region than one octree
	for (unsigned i = 0; i < Vector3iUtil::AXIS_COUNT; ++i) {
		if (bounds_in_voxels.size[i] < octree_size) {
			bounds_in_voxels.size[i] = octree_size;
		}
	}
	_update_data->settings.bounds_in_voxels = bounds_in_voxels;
	_update_data->state.force_update_octrees_next_update = true;
}

void VoxelLodTerrain::set_collision_update_delay(int delay_msec) {
	_collision_update_delay = math::clamp(delay_msec, 0, 4000);
}

int VoxelLodTerrain::get_collision_update_delay() const {
	return _collision_update_delay;
}

void VoxelLodTerrain::set_lod_fade_duration(float seconds) {
	_lod_fade_duration = math::clamp(seconds, 0.f, 1.f);
}

float VoxelLodTerrain::get_lod_fade_duration() const {
	return _lod_fade_duration;
}

#ifdef TOOLS_ENABLED

TypedArray<String> VoxelLodTerrain::get_configuration_warnings() const {
	TypedArray<String> warnings = VoxelNode::get_configuration_warnings();
	if (!warnings.is_empty()) {
		return warnings;
	}
	Ref<VoxelMesher> mesher = get_mesher();
	if (mesher.is_valid() && !mesher->supports_lod()) {
		warnings.append(TTR("The assigned mesher does not support level of detail (LOD), results may be unexpected."));
	}
	return warnings;
}

#endif // TOOLS_ENABLED

void VoxelLodTerrain::_b_save_modified_blocks() {
	save_all_modified_blocks(true);
}

void VoxelLodTerrain::_b_set_voxel_bounds(AABB aabb) {
	ERR_FAIL_COND(!math::is_valid_size(aabb.size));
	set_voxel_bounds(Box3i(math::round_to_int(aabb.position), math::round_to_int(aabb.size)));
}

AABB VoxelLodTerrain::_b_get_voxel_bounds() const {
	const Box3i b = get_voxel_bounds();
	return AABB(b.pos, b.size);
}

// DEBUG LAND

Array VoxelLodTerrain::debug_raycast_mesh_block(Vector3 world_origin, Vector3 world_direction) const {
	const Transform3D world_to_local = get_global_transform().affine_inverse();
	Vector3 pos = world_to_local.xform(world_origin);
	const Vector3 dir = world_to_local.basis.xform(world_direction);
	const float max_distance = 256;
	const float step = 2.f;
	float distance = 0.f;
	const unsigned int lod_count = _update_data->settings.lod_count;
	const unsigned int mesh_block_size_po2 = _update_data->settings.mesh_block_size_po2;

	Array hits;
	while (distance < max_distance && hits.size() == 0) {
		const Vector3i posi = math::floor_to_int(pos);
		for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
			const VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
			const Vector3i bpos = (posi << mesh_block_size_po2) >> lod_index;
			const VoxelMeshBlockVLT *block = mesh_map.get_block(bpos);
			if (block != nullptr && block->is_visible() && block->has_mesh()) {
				Dictionary d;
				d["position"] = block->position;
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

	const VoxelLodTerrainUpdateData::Lod &lod = _update_data->state.lods[lod_index];

	const VoxelDataLodMap::Lod &data_lod = _data->lods[lod_index];
	Vector3i bpos = math::floor_to_int(fbpos);

	int loading_state = 0;

	bool has_block = false;
	{
		RWLockRead rlock(data_lod.map_lock);
		has_block = data_lod.map.has_block(bpos);
	}
	if (has_block) {
		loading_state = 2;
	} else {
		MutexLock lock(lod.loading_blocks_mutex);
		if (lod.has_loading_block(bpos)) {
			loading_state = 1;
		}
	}

	d["loading_state"] = loading_state;
	return d;
}

Dictionary VoxelLodTerrain::debug_get_mesh_block_info(Vector3 fbpos, int lod_index) const {
	Dictionary d;
	ERR_FAIL_COND_V(lod_index < 0, d);
	const int lod_count = get_lod_count();
	ERR_FAIL_COND_V(lod_index >= lod_count, d);

	const Vector3i bpos = math::floor_to_int(fbpos);

	bool loaded = false;
	bool meshed = false;
	bool visible = false;
	bool active = false;
	int mesh_state = VoxelLodTerrainUpdateData::MESH_NEVER_UPDATED;

	const VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
	const VoxelMeshBlockVLT *block = mesh_map.get_block(bpos);

	if (block != nullptr) {
		int recomputed_transition_mask;
		{
			const VoxelLodTerrainUpdateData::Lod &lod = _update_data->state.lods[lod_index];
			RWLockRead rlock(lod.mesh_map_state.map_lock);
			recomputed_transition_mask = VoxelLodTerrainUpdateTask::get_transition_mask(
					_update_data->state, bpos, block->lod_index, lod_count);
			auto it = lod.mesh_map_state.map.find(bpos);
			if (it != lod.mesh_map_state.map.end()) {
				mesh_state = it->second.state;
			}
		}

		loaded = true;
		meshed = block->has_mesh();
		visible = block->is_visible();
		active = block->active;
		d["transition_mask"] = block->get_transition_mask();
		// This can highlight possible bugs between the current state and what it should be
		d["recomputed_transition_mask"] = recomputed_transition_mask;
	}

	d["loaded"] = loaded;
	d["meshed"] = meshed;
	d["mesh_state"] = mesh_state;
	d["visible"] = visible;
	d["active"] = active;
	return d;
}

Array VoxelLodTerrain::debug_get_octree_positions() const {
	_update_data->wait_for_end_of_task();
	Array positions;
	const Map<Vector3i, VoxelLodTerrainUpdateData::OctreeItem> &octrees = _update_data->state.lod_octrees;
	positions.resize(octrees.size());
	int i = 0;
	for (Map<Vector3i, VoxelLodTerrainUpdateData::OctreeItem>::Element *e = octrees.front(); e; e = e->next()) {
		positions[i++] = e->key();
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
				const VoxelLodTerrainUpdateData::State &state, Array &out_data) {
			ERR_FAIL_COND(lod_index < 0);
			Variant node_state;

			const VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
			auto mesh_block_it = lod.mesh_map_state.map.find(position);
			if (mesh_block_it == lod.mesh_map_state.map.end()) {
				node_state = 0;
			} else {
				if (mesh_block_it->second.state == VoxelLodTerrainUpdateData::MESH_UP_TO_DATE) {
					node_state = 2;
				} else {
					node_state = 1;
				}
			}

			out_data.append(node_state);

			if (node->has_children()) {
				Array children_data;
				for (unsigned int i = 0; i < 8; ++i) {
					Array child_data;
					const LodOctree::Node *child = octree.get_child(node, i);
					const Vector3i child_pos = LodOctree::get_child_position(position, i);
					read_node(octree, child, child_pos, lod_index - 1, state, child_data);
					children_data.append(child_data);
				}
				out_data.append(children_data);

			} else {
				out_data.append(Variant());
			}
		}
	};

	_update_data->wait_for_end_of_task();

	const Map<Vector3i, VoxelLodTerrainUpdateData::OctreeItem> &octrees = _update_data->state.lod_octrees;

	Array forest_data;

	for (const Map<Vector3i, VoxelLodTerrainUpdateData::OctreeItem>::Element *e = octrees.front(); e; e = e->next()) {
		const LodOctree &octree = e->value().octree;
		const LodOctree::Node *root = octree.get_root();
		Array root_data;
		const Vector3i octree_pos = e->key();
		L::read_node(octree, root, octree_pos, get_lod_count() - 1, _update_data->state, root_data);
		forest_data.append(octree_pos);
		forest_data.append(root_data);
	}

	return forest_data;
}

#ifdef TOOLS_ENABLED

void VoxelLodTerrain::update_gizmos() {
	ZN_PROFILE_SCOPE();

	// Hopefully this should not be skipped most of the time, because the task is started at the end of `_process`,
	// and gizmos update before. So the task has about 16ms to complete. If it takes longer, it will skip.
	// This allows us to avoid locking data structures.
	if (_update_data->task_is_complete == false) {
		return;
	}

	const VoxelLodTerrainUpdateData::State &state = _update_data->state;

	DebugRenderer &dr = _debug_renderer;
	dr.begin();

	const Transform3D parent_transform = get_global_transform();
	const unsigned int lod_count = get_lod_count();

	// Octree bounds
	if (_show_octree_bounds_gizmos) {
		const int octree_size = 1 << LodOctree::get_octree_size_po2(get_mesh_block_size_pow2(), get_lod_count());
		const Basis local_octree_basis = Basis().scaled(Vector3(octree_size, octree_size, octree_size));
		for (Map<Vector3i, VoxelLodTerrainUpdateData::OctreeItem>::Element *e = state.lod_octrees.front(); e;
				e = e->next()) {
			const Transform3D local_transform(local_octree_basis, e->key() * octree_size);
			dr.draw_box(parent_transform * local_transform, DebugColors::ID_OCTREE_BOUNDS);
		}
	}

	// Volume bounds
	if (_show_volume_bounds_gizmos) {
		const Box3i bounds_in_voxels = get_voxel_bounds();
		const float bounds_in_voxels_len = Vector3(bounds_in_voxels.size).length();
		if (bounds_in_voxels_len < 10000) {
			const Vector3 margin = Vector3(1, 1, 1) * bounds_in_voxels_len * 0.0025f;
			const Vector3 size = bounds_in_voxels.size;
			const Transform3D local_transform(
					Basis().scaled(size + margin * 2.f), Vector3(bounds_in_voxels.pos) - margin);
			dr.draw_box(parent_transform * local_transform, DebugColors::ID_VOXEL_BOUNDS);
		}
	}

	// Octree nodes
	if (_show_octree_node_gizmos) {
		// That can be expensive to draw
		const int mesh_block_size = get_mesh_block_size();
		const float lod_count_f = lod_count;
		for (Map<Vector3i, VoxelLodTerrainUpdateData::OctreeItem>::Element *e = state.lod_octrees.front(); e;
				e = e->next()) {
			const LodOctree &octree = e->value().octree;

			const Vector3i block_pos_maxlod = e->key();
			const Vector3i block_offset_lod0 = block_pos_maxlod << (lod_count - 1);

			octree.for_each_leaf([&dr, block_offset_lod0, mesh_block_size, parent_transform, lod_count_f](
										 Vector3i node_pos, int lod_index, const LodOctree::NodeData &data) {
				//
				const int size = mesh_block_size << lod_index;
				const Vector3i voxel_pos = mesh_block_size * ((node_pos << lod_index) + block_offset_lod0);
				const Transform3D local_transform(Basis().scaled(Vector3(size, size, size)), voxel_pos);
				const Transform3D t = parent_transform * local_transform;
				// Squaring because lower lod indexes are more interesting to see, so we give them more contrast.
				// Also this might be better with sRGB?
				const float g = math::squared(math::max(1.f - float(lod_index) / lod_count_f, 0.f));
				dr.draw_box_mm(t, Color8(255, uint8_t(g * 254.f), 0, 255));
			});
		}
	}

	// Edited blocks
	if (_show_edited_blocks && _edited_blocks_gizmos_lod_index < lod_count) {
		const VoxelDataLodMap::Lod &data_lod = _data->lods[_edited_blocks_gizmos_lod_index];
		const int data_block_size = get_data_block_size() << _edited_blocks_gizmos_lod_index;
		const Basis basis(Basis().scaled(Vector3(data_block_size, data_block_size, data_block_size)));

		RWLockRead rlock(data_lod.map_lock);
		data_lod.map.for_each_block([&dr, parent_transform, data_block_size, basis](const VoxelDataBlock &block) {
			const Transform3D local_transform(basis, block.position * data_block_size);
			const Transform3D t = parent_transform * local_transform;
			const Color8 c = Color8(block.is_modified() ? 255 : 0, 255, 0, 255);
			dr.draw_box_mm(t, c);
		});
	}

	dr.end();
}

void VoxelLodTerrain::set_show_gizmos(bool enable) {
	_show_gizmos_enabled = enable;
	if (_show_gizmos_enabled) {
		_debug_renderer.set_world(is_visible_in_tree() ? *get_world_3d() : nullptr);
	} else {
		_debug_renderer.clear();
	}
}

void VoxelLodTerrain::set_show_octree_gizmos(bool enable) {
	_show_octree_node_gizmos = enable;
}

#endif

// This copies at multiple LOD levels to debug mips
Array VoxelLodTerrain::_b_debug_print_sdf_top_down(Vector3i center, Vector3i extents) {
	ERR_FAIL_COND_V(!math::is_valid_size(extents), Array());

	Array image_array;

	for (unsigned int lod_index = 0; lod_index < _data->lod_count; ++lod_index) {
		const Box3i world_box = Box3i::from_center_extents(center >> lod_index, extents >> lod_index);

		if (Vector3iUtil::get_volume(world_box.size) == 0) {
			continue;
		}

		VoxelBufferInternal buffer;
		buffer.create(world_box.size);

		const VoxelDataLodMap::Lod &data_lod = _data->lods[lod_index];

		world_box.for_each_cell([&data_lod, &buffer, world_box](const Vector3i &world_pos) {
			std::shared_ptr<VoxelBufferInternal> voxels =
					try_get_voxel_buffer_with_lock(data_lod, data_lod.map.voxel_to_block(world_pos));
			if (voxels == nullptr) {
				return;
			}
			const float v =
					get_voxel_with_lock(*voxels, data_lod.map.to_local(world_pos), VoxelBufferInternal::CHANNEL_SDF).f;
			const Vector3i rpos = world_pos - world_box.pos;
			buffer.set_voxel_f(v, rpos.x, rpos.y, rpos.z, VoxelBufferInternal::CHANNEL_SDF);
		});

		Ref<Image> image = gd::VoxelBuffer::debug_print_sdf_to_image_top_down(buffer);
		image_array.append(image);
	}

	return image_array;
}

int VoxelLodTerrain::_b_debug_get_mesh_block_count() const {
	int sum = 0;
	const unsigned int lod_count = get_lod_count();
	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		const VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
		sum += mesh_map.get_block_count();
	}
	return sum;
}

int VoxelLodTerrain::_b_debug_get_data_block_count() const {
	int sum = 0;
	for (unsigned int lod_index = 0; lod_index < _data->lod_count; ++lod_index) {
		const VoxelDataLodMap::Lod &data_lod = _data->lods[lod_index];
		RWLockRead rlock(data_lod.map_lock);
		sum += data_lod.map.get_block_count();
	}
	return sum;
}

Error VoxelLodTerrain::_b_debug_dump_as_scene(String fpath, bool include_instancer) const {
	Node3D *root = memnew(Node3D);
	root->set_name(get_name());

	const unsigned int lod_count = get_lod_count();

	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		const VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];

		mesh_map.for_each_block([root](const VoxelMeshBlockVLT &block) {
			block.for_each_mesh_instance_with_transform([root, &block](const DirectMeshInstance &dmi, Transform3D t) {
				Ref<Mesh> mesh = dmi.get_mesh();

				if (mesh.is_valid()) {
					MeshInstance3D *mi = memnew(MeshInstance3D);
					mi->set_mesh(mesh);
					mi->set_transform(t);
					// TODO Transition mesh visibility?
					mi->set_visible(block.is_visible());
					root->add_child(mi);
					// The owner must be set after adding to parent
					mi->set_owner(root);
				}
			});
		});
	}

	if (include_instancer && _instancer != nullptr) {
		Node *instances_root = _instancer->debug_dump_as_nodes();
		if (instances_root != nullptr) {
			root->add_child(instances_root);
			set_nodes_owner(instances_root, root);
		}
	}

	Ref<PackedScene> scene;
	scene.instantiate();
	const Error pack_result = scene->pack(root);
	memdelete(root);
	if (pack_result != OK) {
		return pack_result;
	}

	const Error save_result = ResourceSaver::save(fpath, scene, ResourceSaver::FLAG_BUNDLE_RESOURCES);
	return save_result;
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
	ClassDB::bind_method(
			D_METHOD("set_collision_update_delay", "delay_msec"), &VoxelLodTerrain::set_collision_update_delay);

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

	ClassDB::bind_method(D_METHOD("set_full_load_mode_enabled"), &VoxelLodTerrain::set_full_load_mode_enabled);
	ClassDB::bind_method(D_METHOD("is_full_load_mode_enabled"), &VoxelLodTerrain::is_full_load_mode_enabled);

	ClassDB::bind_method(D_METHOD("get_statistics"), &VoxelLodTerrain::_b_get_statistics);
	ClassDB::bind_method(
			D_METHOD("voxel_to_data_block_position", "lod_index"), &VoxelLodTerrain::voxel_to_data_block_position);
	ClassDB::bind_method(
			D_METHOD("voxel_to_mesh_block_position", "lod_index"), &VoxelLodTerrain::voxel_to_mesh_block_position);

	ClassDB::bind_method(D_METHOD("get_voxel_tool"), &VoxelLodTerrain::get_voxel_tool);
	ClassDB::bind_method(D_METHOD("save_modified_blocks"), &VoxelLodTerrain::_b_save_modified_blocks);

	ClassDB::bind_method(D_METHOD("set_run_stream_in_editor"), &VoxelLodTerrain::set_run_stream_in_editor);
	ClassDB::bind_method(D_METHOD("is_stream_running_in_editor"), &VoxelLodTerrain::is_stream_running_in_editor);

	ClassDB::bind_method(D_METHOD("set_voxel_bounds"), &VoxelLodTerrain::_b_set_voxel_bounds);
	ClassDB::bind_method(D_METHOD("get_voxel_bounds"), &VoxelLodTerrain::_b_get_voxel_bounds);

	ClassDB::bind_method(D_METHOD("set_process_callback", "mode"), &VoxelLodTerrain::set_process_callback);
	ClassDB::bind_method(D_METHOD("get_process_callback"), &VoxelLodTerrain::get_process_callback);

	ClassDB::bind_method(
			D_METHOD("set_threaded_update_enabled", "enabled"), &VoxelLodTerrain::set_threaded_update_enabled);
	ClassDB::bind_method(D_METHOD("is_threaded_update_enabled"), &VoxelLodTerrain::is_threaded_update_enabled);

	ClassDB::bind_method(
			D_METHOD("debug_raycast_mesh_block", "origin", "dir"), &VoxelLodTerrain::debug_raycast_mesh_block);
	ClassDB::bind_method(
			D_METHOD("debug_get_data_block_info", "block_pos", "lod"), &VoxelLodTerrain::debug_get_data_block_info);
	ClassDB::bind_method(
			D_METHOD("debug_get_mesh_block_info", "block_pos", "lod"), &VoxelLodTerrain::debug_get_mesh_block_info);
	ClassDB::bind_method(D_METHOD("debug_get_octrees_detailed"), &VoxelLodTerrain::debug_get_octrees_detailed);
	ClassDB::bind_method(
			D_METHOD("debug_print_sdf_top_down", "center", "extents"), &VoxelLodTerrain::_b_debug_print_sdf_top_down);
	ClassDB::bind_method(D_METHOD("debug_get_mesh_block_count"), &VoxelLodTerrain::_b_debug_get_mesh_block_count);
	ClassDB::bind_method(D_METHOD("debug_get_data_block_count"), &VoxelLodTerrain::_b_debug_get_data_block_count);
	ClassDB::bind_method(
			D_METHOD("debug_dump_as_scene", "path", "include_instancer"), &VoxelLodTerrain::_b_debug_dump_as_scene);

	ClassDB::bind_method(D_METHOD("restart_stream"), &VoxelLodTerrain::restart_stream);
	//ClassDB::bind_method(D_METHOD("_on_stream_params_changed"), &VoxelLodTerrain::_on_stream_params_changed);

	BIND_ENUM_CONSTANT(PROCESS_CALLBACK_IDLE);
	BIND_ENUM_CONSTANT(PROCESS_CALLBACK_PHYSICS);
	BIND_ENUM_CONSTANT(PROCESS_CALLBACK_DISABLED);

	ADD_GROUP("Bounds", "");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "view_distance"), "set_view_distance", "get_view_distance");
	ADD_PROPERTY(PropertyInfo(Variant::AABB, "voxel_bounds"), "set_voxel_bounds", "get_voxel_bounds");

	ADD_GROUP("Level of detail", "");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod_count"), "set_lod_count", "get_lod_count");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod_distance"), "set_lod_distance", "get_lod_distance");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod_fade_duration"), "set_lod_fade_duration", "get_lod_fade_duration");

	ADD_GROUP("Material", "");

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, Material::get_class_static()),
			"set_material", "get_material");

	ADD_GROUP("Collisions", "");

	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "generate_collisions"), "set_generate_collisions", "get_generate_collisions");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_layer",
			"get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask",
			"get_collision_mask");
	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "collision_lod_count"), "set_collision_lod_count", "get_collision_lod_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_update_delay"), "set_collision_update_delay",
			"get_collision_update_delay");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_margin"), "set_collision_margin", "get_collision_margin");

	ADD_GROUP("Advanced", "");

	// TODO Probably should be in parent class?
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "run_stream_in_editor"), "set_run_stream_in_editor",
			"is_stream_running_in_editor");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_block_size"), "set_mesh_block_size", "get_mesh_block_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "full_load_mode_enabled"), "set_full_load_mode_enabled",
			"is_full_load_mode_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "threaded_update_enabled"), "set_threaded_update_enabled",
			"is_threaded_update_enabled");
}

} // namespace zylann::voxel
