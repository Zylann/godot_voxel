#include "voxel_server.h"
#include "../meshers/transvoxel/voxel_mesher_transvoxel.h"
#include "../util/macros.h"
#include "../util/profiling.h"
#include "../voxel_constants.h"
#include <core/os/memory.h>
#include <scene/main/viewport.h>

namespace {
VoxelServer *g_voxel_server = nullptr;
}

template <typename Dst_T>
inline Dst_T *must_be_cast(IVoxelTask *src) {
#ifdef TOOLS_ENABLED
	Dst_T *dst = dynamic_cast<Dst_T *>(src);
	CRASH_COND_MSG(dst == nullptr, "Invalid cast");
	return dst;
#else
	return static_cast<Dst_T *>(src);
#endif
}

template <typename T>
inline std::shared_ptr<T> gd_make_shared() {
	// std::make_shared() apparently wont allow us to specify custom new and delete
	return std::shared_ptr<T>(memnew(T), memdelete<T>);
}

VoxelServer *VoxelServer::get_singleton() {
	CRASH_COND_MSG(g_voxel_server == nullptr, "Accessing singleton while it's null");
	return g_voxel_server;
}

void VoxelServer::create_singleton() {
	CRASH_COND_MSG(g_voxel_server != nullptr, "Creating singleton twice");
	g_voxel_server = memnew(VoxelServer);
}

void VoxelServer::destroy_singleton() {
	CRASH_COND_MSG(g_voxel_server == nullptr, "Destroying singleton twice");
	memdelete(g_voxel_server);
	g_voxel_server = nullptr;
}

VoxelServer::VoxelServer() {
	// TODO Can't set more than 1 thread yet, streams aren't well made for it... unless we can separate file ones
	// This pool can work on larger periods, it doesn't require low latency
	_streaming_thread_pool.set_thread_count(1);
	_streaming_thread_pool.set_priority_update_period(300);
	_streaming_thread_pool.set_batch_count(16);

	// TODO Try more threads, it should be possible
	// This pool works on visuals so it must have low latency
	_meshing_thread_pool.set_thread_count(2);
	_meshing_thread_pool.set_priority_update_period(64);
	_meshing_thread_pool.set_batch_count(1);

	for (size_t i = 0; i < _meshing_thread_pool.get_thread_count(); ++i) {
		Ref<VoxelMesherBlocky> mesher;
		mesher.instance();
		mesher->set_occlusion_enabled(true);
		mesher->set_occlusion_darkness(0.8f);
		_blocky_meshers[i] = mesher;
	}

	for (size_t i = 0; i < _meshing_thread_pool.get_thread_count(); ++i) {
		Ref<VoxelMesherTransvoxel> mesher;
		mesher.instance();
		_smooth_meshers[i] = mesher;
	}

	if (Engine::get_singleton()->is_editor_hint()) {
		// Default viewer
		const uint32_t default_viewer_id = add_viewer();
		Viewer &default_viewer = _world.viewers.get(default_viewer_id);
		default_viewer.is_default = true;
	}

	// Init world
	_world.shared_priority_dependency = gd_make_shared<PriorityDependencyShared>();

	PRINT_VERBOSE(String("Size of BlockDataRequest: {0}").format(varray((int)sizeof(BlockDataRequest))));
	PRINT_VERBOSE(String("Size of BlockMeshRequest: {0}").format(varray((int)sizeof(BlockMeshRequest))));
}

VoxelServer::~VoxelServer() {
	// The GDScriptLanguage singleton can get destroyed before ours, so any script referenced by tasks
	// cannot be freed. To work this around, tasks are cleared when the scene tree autoload is destroyed.
	// So normally there should not be any task left to clear here,
	// but doing it anyways for correctness, it's how it should have been...
	// See https://github.com/Zylann/godot_voxel/issues/189
	wait_and_clear_all_tasks(true);
}

void VoxelServer::wait_and_clear_all_tasks(bool warn) {
	_streaming_thread_pool.wait_for_all_tasks();
	_meshing_thread_pool.wait_for_all_tasks();

	_streaming_thread_pool.dequeue_completed_tasks([warn](IVoxelTask *task) {
		if (warn) {
			WARN_PRINT("Streaming tasks remain on module cleanup, "
					   "this could become a problem if they reference scripts");
		}
		memdelete(task);
	});

	_meshing_thread_pool.dequeue_completed_tasks([](IVoxelTask *task) {
		memdelete(task);
	});
}

int VoxelServer::get_priority(const PriorityDependency &dep, uint8_t lod, float *out_closest_distance_sq) {
	const std::vector<Vector3> &viewer_positions = dep.shared->viewers;
	const Vector3 block_position = dep.world_position;

	float closest_distance_sq = 99999.f;
	if (viewer_positions.size() == 0) {
		// Assume origin
		closest_distance_sq = block_position.length_squared();
	} else {
		for (size_t i = 0; i < viewer_positions.size(); ++i) {
			float d = viewer_positions[i].distance_squared_to(block_position);
			if (d < closest_distance_sq) {
				closest_distance_sq = d;
			}
		}
	}

	int priority = static_cast<int>(closest_distance_sq);

	if (out_closest_distance_sq != nullptr) {
		*out_closest_distance_sq = closest_distance_sq;
	}

	// Higher lod indexes come first to allow the octree to subdivide.
	// Then comes distance, which is modified by how much in view the block is
	priority += (VoxelConstants::MAX_LOD - lod) * 10000;

	return priority;
}

uint32_t VoxelServer::add_volume(ReceptionBuffers *buffers, VolumeType type) {
	CRASH_COND(buffers == nullptr);
	Volume volume;
	volume.type = type;
	volume.reception_buffers = buffers;
	volume.meshing_dependency = gd_make_shared<MeshingDependency>();
	return _world.volumes.create(volume);
}

void VoxelServer::set_volume_transform(uint32_t volume_id, Transform t) {
	Volume &volume = _world.volumes.get(volume_id);
	volume.transform = t;
}

void VoxelServer::set_volume_block_size(uint32_t volume_id, uint32_t block_size) {
	Volume &volume = _world.volumes.get(volume_id);
	volume.block_size = block_size;
}

void VoxelServer::set_volume_stream(uint32_t volume_id, Ref<VoxelStream> stream) {
	Volume &volume = _world.volumes.get(volume_id);
	volume.stream = stream;

	// Commit a new stream to process requests with
	if (volume.stream_dependency != nullptr) {
		volume.stream_dependency->valid = false;
	}

	if (stream.is_valid()) {
		volume.stream_dependency = gd_make_shared<StreamingDependency>();
		for (size_t i = 0; i < _streaming_thread_pool.get_thread_count(); ++i) {
			volume.stream_dependency->streams[i] = stream->duplicate();
		}
	} else {
		volume.stream_dependency = nullptr;
	}
}

void VoxelServer::set_volume_voxel_library(uint32_t volume_id, Ref<VoxelLibrary> library) {
	Volume &volume = _world.volumes.get(volume_id);
	volume.voxel_library = library;
	volume.meshing_dependency = gd_make_shared<MeshingDependency>();
	volume.meshing_dependency->library = volume.voxel_library;
}

void VoxelServer::set_volume_octree_split_scale(uint32_t volume_id, float split_scale) {
	Volume &volume = _world.volumes.get(volume_id);
	volume.octree_split_scale = split_scale;
}

void VoxelServer::invalidate_volume_mesh_requests(uint32_t volume_id) {
	Volume &volume = _world.volumes.get(volume_id);
	volume.meshing_dependency->valid = false;
	volume.meshing_dependency = gd_make_shared<MeshingDependency>();
	volume.meshing_dependency->library = volume.voxel_library;
}

static inline Vector3i get_block_center(Vector3i pos, int bs, int lod) {
	return (pos << lod) * bs + Vector3i(bs / 2);
}

void VoxelServer::init_priority_dependency(
		VoxelServer::PriorityDependency &dep, Vector3i block_position, uint8_t lod, const Volume &volume) {

	const Vector3i voxel_pos = get_block_center(block_position, volume.block_size, lod);
	const float block_radius = (volume.block_size << lod) / 2;
	dep.shared = _world.shared_priority_dependency;
	dep.world_position = volume.transform.xform(voxel_pos.to_vec3());
	const float transformed_block_radius =
			volume.transform.basis.xform(Vector3(block_radius, block_radius, block_radius)).length();

	switch (volume.type) {
		case VOLUME_SPARSE_GRID:
			// Distance beyond which no field of view can overlap the block
			dep.drop_distance_squared =
					squared(_world.shared_priority_dependency->highest_view_distance + transformed_block_radius);
			break;

		case VOLUME_SPARSE_OCTREE:
			// Distance beyond which it is safe to drop a block without risking to block LOD subdivision.
			// This does not depend on viewer's view distance, but on LOD precision instead.
			dep.drop_distance_squared = squared(2.f * transformed_block_radius *
												get_octree_lod_block_region_extent(volume.octree_split_scale));
			break;

		default:
			CRASH_NOW_MSG("Unexpected type");
			break;
	}
}

void VoxelServer::request_block_mesh(uint32_t volume_id, BlockMeshInput &input) {
	const Volume &volume = _world.volumes.get(volume_id);
	ERR_FAIL_COND(volume.stream.is_null());
	CRASH_COND(volume.stream_dependency == nullptr);
	ERR_FAIL_COND(volume.meshing_dependency == nullptr);

	BlockMeshRequest *r = memnew(BlockMeshRequest);
	r->volume_id = volume_id;
	r->blocks = input.blocks;
	r->position = input.position;
	r->lod = input.lod;

	r->smooth_enabled = volume.stream->get_used_channels_mask() & (1 << VoxelBuffer::CHANNEL_SDF);
	r->blocky_enabled = volume.voxel_library.is_valid() &&
						volume.stream->get_used_channels_mask() & (1 << VoxelBuffer::CHANNEL_TYPE);

	r->meshing_dependency = volume.meshing_dependency;

	init_priority_dependency(r->priority_dependency, input.position, input.lod, volume);

	// We'll allocate this quite often. If it becomes a problem, it should be easy to pool.
	_meshing_thread_pool.enqueue(r);
}

void VoxelServer::request_block_load(uint32_t volume_id, Vector3i block_pos, int lod) {
	const Volume &volume = _world.volumes.get(volume_id);
	ERR_FAIL_COND(volume.stream.is_null());
	CRASH_COND(volume.stream_dependency == nullptr);

	BlockDataRequest r;
	r.volume_id = volume_id;
	r.position = block_pos;
	r.lod = lod;
	r.type = BlockDataRequest::TYPE_LOAD;
	r.block_size = volume.block_size;

	r.stream_dependency = volume.stream_dependency;

	init_priority_dependency(r.priority_dependency, block_pos, lod, volume);

	BlockDataRequest *rp = memnew(BlockDataRequest(r));
	_streaming_thread_pool.enqueue(rp);
}

void VoxelServer::request_block_save(uint32_t volume_id, Ref<VoxelBuffer> voxels, Vector3i block_pos, int lod) {
	const Volume &volume = _world.volumes.get(volume_id);
	ERR_FAIL_COND(volume.stream.is_null());
	CRASH_COND(volume.stream_dependency == nullptr);

	BlockDataRequest r;
	r.voxels = voxels;
	r.volume_id = volume_id;
	r.position = block_pos;
	r.lod = lod;
	r.type = BlockDataRequest::TYPE_SAVE;
	r.block_size = volume.block_size;

	r.stream_dependency = volume.stream_dependency;

	// No priority data, saving doesnt need sorting

	BlockDataRequest *rp = memnew(BlockDataRequest(r));
	_streaming_thread_pool.enqueue(rp);
}

void VoxelServer::remove_volume(uint32_t volume_id) {
	{
		Volume &volume = _world.volumes.get(volume_id);
		if (volume.stream_dependency != nullptr) {
			volume.stream_dependency->valid = false;
		}
		if (volume.meshing_dependency != nullptr) {
			volume.meshing_dependency->valid = false;
		}
	}

	_world.volumes.destroy(volume_id);
	// TODO How to cancel meshing tasks?

	if (_world.volumes.count() == 0) {
		// To workaround https://github.com/Zylann/godot_voxel/issues/189
		// When the last remaining volume got destroyed (as in game exit)
		VoxelServer::get_singleton()->wait_and_clear_all_tasks(false);
	}
}

uint32_t VoxelServer::add_viewer() {
	if (Engine::get_singleton()->is_editor_hint()) {
		// Remove default viewer if any
		_world.viewers.for_each_with_id([this](Viewer &viewer, uint32_t id) {
			if (viewer.is_default) {
				// Safe because StructDB does not shift items
				_world.viewers.destroy(id);
			}
		});
	}

	return _world.viewers.create(Viewer());
}

void VoxelServer::remove_viewer(uint32_t viewer_id) {
	_world.viewers.destroy(viewer_id);

	if (Engine::get_singleton()->is_editor_hint()) {
		// Re-add default viewer
		if (_world.viewers.count() == 0) {
			const uint32_t default_viewer_id = add_viewer();
			Viewer &default_viewer = _world.viewers.get(default_viewer_id);
			default_viewer.is_default = true;
		}
	}
}

void VoxelServer::set_viewer_position(uint32_t viewer_id, Vector3 position) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.world_position = position;
}

void VoxelServer::set_viewer_distance(uint32_t viewer_id, unsigned int distance) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.view_distance = distance;
}

unsigned int VoxelServer::get_viewer_distance(uint32_t viewer_id) const {
	const Viewer &viewer = _world.viewers.get(viewer_id);
	return viewer.view_distance;
}

void VoxelServer::set_viewer_requires_visuals(uint32_t viewer_id, bool enabled) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.require_visuals = enabled;
}

bool VoxelServer::is_viewer_requiring_visuals(uint32_t viewer_id) const {
	const Viewer &viewer = _world.viewers.get(viewer_id);
	return viewer.require_visuals;
}

void VoxelServer::set_viewer_requires_collisions(uint32_t viewer_id, bool enabled) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.require_collisions = enabled;
}

bool VoxelServer::is_viewer_requiring_collisions(uint32_t viewer_id) const {
	const Viewer &viewer = _world.viewers.get(viewer_id);
	return viewer.require_collisions;
}

bool VoxelServer::viewer_exists(uint32_t viewer_id) const {
	return _world.viewers.is_valid(viewer_id);
}

void VoxelServer::process() {
	// Note, this shouldn't be here. It should normally done just after SwapBuffers.
	// Godot does not have any C++ profiler usage anywhere, so when using Tracy Profiler I have to put it somewhere...
	VOXEL_PROFILE_MARK_FRAME();
	VOXEL_PROFILE_SCOPE();

	// Receive data updates
	_streaming_thread_pool.dequeue_completed_tasks([this](IVoxelTask *task) {
		BlockDataRequest *r = must_be_cast<BlockDataRequest>(task);
		Volume *volume = _world.volumes.try_get(r->volume_id);

		if (volume != nullptr) {
			// TODO Comparing pointer may not be guaranteed
			// The request response must match the dependency it would have been requested with.
			// If it doesn't match, we are no longer interested in the result.
			if (r->stream_dependency == volume->stream_dependency) {
				BlockDataOutput o;
				o.voxels = r->voxels;
				o.position = r->position;
				o.lod = r->lod;
				o.dropped = !r->has_run;

				switch (r->type) {
					case BlockDataRequest::TYPE_SAVE:
						o.type = BlockDataOutput::TYPE_SAVE;
						break;

					case BlockDataRequest::TYPE_LOAD:
						o.type = BlockDataOutput::TYPE_LOAD;
						break;

					default:
						CRASH_NOW_MSG("Unexpected data request response type");
				}

				volume->reception_buffers->data_output.push_back(o);
			}

		} else {
			// This can happen if the user removes the volume while requests are still about to return
			PRINT_VERBOSE("Data request response came back but volume wasn't found");
		}

		memdelete(r);
	});

	// Receive mesh updates
	_meshing_thread_pool.dequeue_completed_tasks([this](IVoxelTask *task) {
		BlockMeshRequest *r = must_be_cast<BlockMeshRequest>(task);
		Volume *volume = _world.volumes.try_get(r->volume_id);

		if (volume != nullptr) {
			// TODO Comparing pointer may not be guaranteed
			// The request response must match the dependency it would have been requested with.
			// If it doesn't match, we are no longer interested in the result.
			if (volume->meshing_dependency == r->meshing_dependency) {

				BlockMeshOutput o;
				// TODO Check for invalidation due to property changes

				if (r->has_run) {
					o.type = BlockMeshOutput::TYPE_MESHED;
				} else {
					o.type = BlockMeshOutput::TYPE_DROPPED;
				}

				o.position = r->position;
				o.lod = r->lod;
				o.blocky_surfaces = r->blocky_surfaces_output;
				o.smooth_surfaces = r->smooth_surfaces_output;

				volume->reception_buffers->mesh_output.push_back(o);
			}

		} else {
			// This can happen if the user removes the volume while requests are still about to return
			PRINT_VERBOSE("Mesh request response came back but volume wasn't found");
		}

		memdelete(r);
	});

	// Update viewer dependencies
	{
		const size_t viewer_count = _world.viewers.count();
		if (_world.shared_priority_dependency->viewers.size() != viewer_count) {
			// TODO We can avoid the invalidation by using an atomic size or memory barrier?
			_world.shared_priority_dependency = gd_make_shared<PriorityDependencyShared>();
			_world.shared_priority_dependency->viewers.resize(viewer_count);
		}
		size_t i = 0;
		unsigned int max_distance = 0;
		_world.viewers.for_each([&i, &max_distance, this](Viewer &viewer) {
			_world.shared_priority_dependency->viewers[i] = viewer.world_position;
			if (viewer.view_distance > max_distance) {
				max_distance = viewer.view_distance;
			}
			++i;
		});
		// Cancel distance is increased because of two reasons:
		// - Some volumes use a cubic area which has higher distances on their corners
		// - Hysteresis is needed to reduce ping-pong
		_world.shared_priority_dependency->highest_view_distance = max_distance * 2;
	}
}

void VoxelServer::get_min_max_block_padding(
		bool blocky_enabled, bool smooth_enabled, unsigned int &out_min_padding, unsigned int &out_max_padding) const {

	// const Volume &volume = _world.volumes.get(volume_id);

	// bool smooth_enabled = volume.stream->get_used_channels_mask() & (1 << VoxelBuffer::CHANNEL_SDF);
	// bool blocky_enabled = volume.voxel_library.is_valid() &&
	// 					  volume.stream->get_used_channels_mask() & (1 << VoxelBuffer::CHANNEL_TYPE);

	out_min_padding = 0;
	out_max_padding = 0;

	if (blocky_enabled) {
		out_min_padding = max(out_min_padding, _blocky_meshers[0]->get_minimum_padding());
		out_max_padding = max(out_max_padding, _blocky_meshers[0]->get_maximum_padding());
	}

	if (smooth_enabled) {
		out_min_padding = max(out_min_padding, _smooth_meshers[0]->get_minimum_padding());
		out_max_padding = max(out_max_padding, _smooth_meshers[0]->get_maximum_padding());
	}
}

static unsigned int debug_get_active_thread_count(const VoxelThreadPool &pool) {
	unsigned int active_count = 0;
	for (unsigned int i = 0; i < pool.get_thread_count(); ++i) {
		VoxelThreadPool::State s = pool.get_thread_debug_state(i);
		if (s == VoxelThreadPool::STATE_RUNNING) {
			++active_count;
		}
	}
	return active_count;
}

static Dictionary debug_get_pool_stats(const VoxelThreadPool &pool) {
	Dictionary d;
	d["tasks"] = pool.get_debug_remaining_tasks();
	d["active_threads"] = debug_get_active_thread_count(pool);
	d["thread_count"] = pool.get_thread_count();
	return d;
}

Dictionary VoxelServer::_b_get_stats() {
	Dictionary d;
	d["streaming"] = debug_get_pool_stats(_streaming_thread_pool);
	d["meshing"] = debug_get_pool_stats(_meshing_thread_pool);
	return d;
}

void VoxelServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_stats"), &VoxelServer::_b_get_stats);
}

//----------------------------------------------------------------------------------------------------------------------

void VoxelServer::BlockDataRequest::run(VoxelTaskContext ctx) {
	VOXEL_PROFILE_SCOPE();

	CRASH_COND(stream_dependency == nullptr);
	Ref<VoxelStream> stream = stream_dependency->streams[ctx.thread_index];
	CRASH_COND(stream.is_null());

	const Vector3i origin_in_voxels = (position << lod) * block_size;

	switch (type) {
		case TYPE_LOAD:
			voxels.instance();
			voxels->create(block_size, block_size, block_size);
			stream->emerge_block(voxels, origin_in_voxels, lod);
			break;

		case TYPE_SAVE: {
			Ref<VoxelBuffer> voxels_copy;
			{
				RWLockRead lock(voxels->get_lock());
				voxels_copy = voxels->duplicate(true);
			}
			voxels.unref();
			stream->immerge_block(voxels_copy, origin_in_voxels, lod);
		} break;

		default:
			CRASH_NOW_MSG("Invalid type");
	}

	has_run = true;
}

int VoxelServer::BlockDataRequest::get_priority() {
	if (type == TYPE_SAVE) {
		return 0;
	}
	float closest_viewer_distance_sq;
	const int p = VoxelServer::get_priority(priority_dependency, lod, &closest_viewer_distance_sq);
	too_far = closest_viewer_distance_sq > priority_dependency.drop_distance_squared;
	return p;
}

bool VoxelServer::BlockDataRequest::is_cancelled() {
	return type == TYPE_LOAD && (!stream_dependency->valid || too_far);
}

//----------------------------------------------------------------------------------------------------------------------

static void copy_block_and_neighbors(const FixedArray<Ref<VoxelBuffer>, Cube::MOORE_AREA_3D_COUNT> &moore_blocks,
		VoxelBuffer &dst, int min_padding, int max_padding) {

	VOXEL_PROFILE_SCOPE();

	FixedArray<unsigned int, 2> channels;
	channels[0] = VoxelBuffer::CHANNEL_TYPE;
	channels[1] = VoxelBuffer::CHANNEL_SDF;

	Ref<VoxelBuffer> central_buffer = moore_blocks[Cube::MOORE_AREA_3D_CENTRAL_INDEX];
	CRASH_COND_MSG(central_buffer.is_null(), "Central buffer must be valid");
	const int block_size = central_buffer->get_size().x;
	const unsigned int padded_block_size = block_size + min_padding + max_padding;

	dst.create(padded_block_size, padded_block_size, padded_block_size);

	for (unsigned int ci = 0; ci < channels.size(); ++ci) {
		dst.set_channel_depth(ci, central_buffer->get_channel_depth(ci));
	}

	const Vector3i min_pos = -Vector3i(min_padding);
	const Vector3i max_pos = Vector3i(block_size + max_padding);

	for (unsigned int i = 0; i < Cube::MOORE_AREA_3D_COUNT; ++i) {
		const Vector3i offset = block_size * Cube::g_ordered_moore_area_3d[i];
		Ref<VoxelBuffer> src = moore_blocks[i];

		if (src.is_null()) {
			continue;
		}

		const Vector3i src_min = min_pos - offset;
		const Vector3i src_max = max_pos - offset;
		const Vector3i dst_min = offset - min_pos;

		{
			RWLockRead read(src->get_lock());
			for (unsigned int ci = 0; ci < channels.size(); ++ci) {
				dst.copy_from(**src, src_min, src_max, dst_min, ci);
			}
		}
	}
}

void VoxelServer::BlockMeshRequest::run(VoxelTaskContext ctx) {
	VOXEL_PROFILE_SCOPE();
	CRASH_COND(meshing_dependency == nullptr);

	unsigned int min_padding;
	unsigned int max_padding;
	VoxelServer::get_singleton()->get_min_max_block_padding(blocky_enabled, smooth_enabled, min_padding, max_padding);

	// TODO Cache?
	Ref<VoxelBuffer> voxels;
	voxels.instance();
	copy_block_and_neighbors(blocks, **voxels, min_padding, max_padding);

	VoxelMesher::Input input = { **voxels, lod };

	if (blocky_enabled) {
		Ref<VoxelLibrary> library = meshing_dependency->library;
		if (library.is_valid()) {
			VOXEL_PROFILE_SCOPE_NAMED("Blocky meshing");
			Ref<VoxelMesherBlocky> blocky_mesher = VoxelServer::get_singleton()->_blocky_meshers[ctx.thread_index];
			CRASH_COND(blocky_mesher.is_null());
			// This mesher only uses baked data from the library, which is protected by a lock
			blocky_mesher->set_library(library);
			blocky_mesher->build(blocky_surfaces_output, input);
			blocky_mesher->set_library(Ref<VoxelLibrary>());
		}
	}

	if (smooth_enabled) {
		VOXEL_PROFILE_SCOPE_NAMED("Smooth meshing");
		Ref<VoxelMesher> smooth_mesher = VoxelServer::get_singleton()->_smooth_meshers[ctx.thread_index];
		CRASH_COND(smooth_mesher.is_null());
		smooth_mesher->build(smooth_surfaces_output, input);
	}

	has_run = true;
}

int VoxelServer::BlockMeshRequest::get_priority() {
	float closest_viewer_distance_sq;
	const int p = VoxelServer::get_priority(priority_dependency, lod, &closest_viewer_distance_sq);
	too_far = closest_viewer_distance_sq > priority_dependency.drop_distance_squared;
	return p;
}

bool VoxelServer::BlockMeshRequest::is_cancelled() {
	return !meshing_dependency->valid || too_far;
}

//----------------------------------------------------------------------------------------------------------------------

namespace {
bool g_updater_created = false;
}

VoxelServerUpdater::VoxelServerUpdater() {
	PRINT_VERBOSE("Creating VoxelServerUpdater");
	set_process(true);
	g_updater_created = true;
}

VoxelServerUpdater::~VoxelServerUpdater() {
	g_updater_created = false;
}

void VoxelServerUpdater::ensure_existence(SceneTree *st) {
	if (st == nullptr) {
		return;
	}
	if (g_updater_created) {
		return;
	}
	Viewport *root = st->get_root();
	for (int i = 0; i < root->get_child_count(); ++i) {
		VoxelServerUpdater *u = Object::cast_to<VoxelServerUpdater>(root->get_child(i));
		if (u != nullptr) {
			return;
		}
	}
	VoxelServerUpdater *u = memnew(VoxelServerUpdater);
	u->set_name("VoxelServerUpdater_dont_touch_this");
	root->add_child(u);
}

void VoxelServerUpdater::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS:
			// To workaround the absence of API to have a custom server processing in the main loop
			VoxelServer::get_singleton()->process();
			break;

		case NOTIFICATION_PREDELETE:
			PRINT_VERBOSE("Deleting VoxelServerUpdater");
			break;

		default:
			break;
	}
}
