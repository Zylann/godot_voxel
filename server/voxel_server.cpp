#include "voxel_server.h"
#include "../meshers/transvoxel/voxel_mesher_transvoxel.h"
#include "../util/macros.h"
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
	_meshing_thread_pool.set_thread_count(1);
	_meshing_thread_pool.set_priority_update_period(32);
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

	// Init world
	// TODO How to make this use memnew and memdelete?
	_world.viewers_for_priority = gd_make_shared<VoxelViewersArray>();
}

VoxelServer::~VoxelServer() {
	_streaming_thread_pool.wait_for_all_tasks();
	_meshing_thread_pool.wait_for_all_tasks();

	_streaming_thread_pool.dequeue_completed_tasks([](IVoxelTask *task) {
		memdelete(task);
	});

	_meshing_thread_pool.dequeue_completed_tasks([](IVoxelTask *task) {
		memdelete(task);
	});
}

int VoxelServer::get_priority(const BlockRequestPriorityDependency &dep) {
	const std::vector<Vector3> &viewer_positions = dep.viewers->positions;
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

	return static_cast<int>(closest_distance_sq);
}

uint32_t VoxelServer::add_volume(ReceptionBuffers *buffers) {
	CRASH_COND(buffers == nullptr);
	Volume volume;
	volume.reception_buffers = buffers;
	volume.meshing_dependency = gd_make_shared<MeshingDependency>();
	volume.meshing_dependency->valid = true;
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
}

void VoxelServer::invalidate_volume_mesh_requests(uint32_t volume_id) {
	Volume &volume = _world.volumes.get(volume_id);
	volume.meshing_dependency->valid = false;
	volume.meshing_dependency = gd_make_shared<MeshingDependency>();
	volume.meshing_dependency->library = volume.voxel_library;
	volume.meshing_dependency->valid = true;
}

template <typename BlockRequest_T>
inline void remove_items_from_matching_volume(std::vector<BlockRequest_T> &requests, uint32_t volume_id) {
	unordered_remove_if(requests, [](const BlockRequest_T &r) {
		return r.volume_id == volume_id;
	})
}

void VoxelServer::request_block_mesh(uint32_t volume_id, Ref<VoxelBuffer> voxels, Vector3i block_pos, int lod) {
	const Volume &volume = _world.volumes.get(volume_id);
	ERR_FAIL_COND(volume.stream.is_null());
	CRASH_COND(volume.stream_dependency == nullptr);
	ERR_FAIL_COND(volume.meshing_dependency == nullptr);

	// TODO Handle spamming!
	// It was previously done by remembering the request with a hashmap by position.
	// But later we may want to solve it by not pre-emptively copying voxels, only do it on meshing using RWLock

	BlockMeshRequest r;
	r.voxels = voxels;
	r.volume_id = volume_id;
	r.position = block_pos;
	r.lod = lod;

	r.smooth_enabled = volume.stream->get_used_channels_mask() & (1 << VoxelBuffer::CHANNEL_SDF);
	r.blocky_enabled = volume.voxel_library.is_valid() &&
					   volume.stream->get_used_channels_mask() & (1 << VoxelBuffer::CHANNEL_TYPE);

	r.meshing_dependency = volume.meshing_dependency;

	const Vector3i voxel_pos = (block_pos << lod) * volume.block_size;
	r.priority_dependency.world_position = volume.transform.xform(voxel_pos.to_vec3());
	r.priority_dependency.viewers = _world.viewers_for_priority;

	// We'll allocate this quite often. If it becomes a problem, it should be easy to pool.
	BlockMeshRequest *rp = memnew(BlockMeshRequest(r));
	_meshing_thread_pool.enqueue(rp);
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

	const Vector3i voxel_pos = (block_pos << lod) * volume.block_size;
	r.priority_dependency.world_position = volume.transform.xform(voxel_pos.to_vec3());
	r.priority_dependency.viewers = _world.viewers_for_priority;

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
}

uint32_t VoxelServer::add_viewer() {
	return _world.viewers.create(Viewer());
}

void VoxelServer::set_viewer_position(uint32_t viewer_id, Vector3 position) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	viewer.world_position = position;
}

void VoxelServer::set_viewer_region_extents(uint32_t viewer_id, Vector3 extents) {
	Viewer &viewer = _world.viewers.get(viewer_id);
	// TODO
}

void VoxelServer::remove_viewer(uint32_t viewer_id) {
	_world.viewers.destroy(viewer_id);
}

void VoxelServer::process() {
	// Receive data updates
	_streaming_thread_pool.dequeue_completed_tasks([this](IVoxelTask *task) {
		BlockDataRequest *r = must_be_cast<BlockDataRequest>(task);
		Volume *volume = _world.volumes.try_get(r->volume_id);

		if (volume != nullptr) {
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
		if (_world.viewers_for_priority->positions.size() != viewer_count) {
			// TODO We can avoid the invalidation by using an atomic size or memory barrier?
			_world.viewers_for_priority = gd_make_shared<VoxelViewersArray>();
			_world.viewers_for_priority->positions.resize(viewer_count);
		}
		size_t i = 0;
		_world.viewers.for_each([&i, this](const Viewer &viewer) {
			_world.viewers_for_priority->positions[i] = viewer.world_position;
			++i;
		});
	}
}

void VoxelServer::get_min_max_block_padding(
		uint32_t volume_id, unsigned int &out_min_padding, unsigned int &out_max_padding) const {

	const Volume &volume = _world.volumes.get(volume_id);

	bool smooth_enabled = volume.stream->get_used_channels_mask() & (1 << VoxelBuffer::CHANNEL_SDF);
	bool blocky_enabled = volume.voxel_library.is_valid() &&
						  volume.stream->get_used_channels_mask() & (1 << VoxelBuffer::CHANNEL_TYPE);

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

//----------------------------------------------------------------------------------------------------------------------

void VoxelServer::BlockDataRequest::run(VoxelTaskContext ctx) {
	CRASH_COND(stream_dependency == nullptr);
	Ref<VoxelStream> stream = stream_dependency->streams[ctx.thread_index];
	CRASH_COND(stream.is_null());

	switch (type) {
		case TYPE_LOAD:
			voxels.instance();
			voxels->create(block_size, block_size, block_size);
			stream->emerge_block(voxels, position * block_size, lod);
			break;

		case TYPE_SAVE:
			stream->immerge_block(voxels, position * block_size, lod);
			voxels.unref();
			break;

		default:
			CRASH_NOW("Invalid type");
	}

	has_run = true;
}

int VoxelServer::BlockDataRequest::get_priority() {
	return type == TYPE_LOAD ? VoxelServer::get_priority(priority_dependency) : 0;
}

bool VoxelServer::BlockDataRequest::is_cancelled() {
	return type == TYPE_LOAD && !stream_dependency->valid;
}

//----------------------------------------------------------------------------------------------------------------------

void VoxelServer::BlockMeshRequest::run(VoxelTaskContext ctx) {
	CRASH_COND(voxels.is_null());
	CRASH_COND(meshing_dependency == nullptr);

	VoxelMesher::Input input = { **voxels, lod };

	if (blocky_enabled) {
		Ref<VoxelLibrary> library = meshing_dependency->library;
		if (library.is_valid()) {
			Ref<VoxelMesherBlocky> blocky_mesher = VoxelServer::get_singleton()->_blocky_meshers[ctx.thread_index];
			CRASH_COND(blocky_mesher.is_null());
			// This mesher only uses baked data from the library, which is protected by a lock
			blocky_mesher->set_library(library);
			blocky_mesher->build(blocky_surfaces_output, input);
			blocky_mesher->set_library(Ref<VoxelLibrary>());
		}
	}

	if (smooth_enabled) {
		Ref<VoxelMesher> smooth_mesher = VoxelServer::get_singleton()->_smooth_meshers[ctx.thread_index];
		CRASH_COND(smooth_mesher.is_null());
		smooth_mesher->build(smooth_surfaces_output, input);
	}

	has_run = true;
}

int VoxelServer::BlockMeshRequest::get_priority() {
	return VoxelServer::get_priority(priority_dependency);
}

bool VoxelServer::BlockMeshRequest::is_cancelled() {
	return !meshing_dependency->valid;
}

//----------------------------------------------------------------------------------------------------------------------

void VoxelServerUpdater::ensure_existence(SceneTree *st) {
	if (st == nullptr) {
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
	u->set_name("VoxelServerUpdater");
	root->add_child(u);
}
