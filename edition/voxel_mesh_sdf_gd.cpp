#include "voxel_mesh_sdf_gd.h"
#include "../engine/voxel_engine.h"
#include "../engine/voxel_engine_updater.h"
#include "../storage/voxel_buffer_gd.h"
#include "../util/dstack.h"
#include "../util/godot/core/array.h"
#include "../util/godot/funcs.h"
#include "../util/math/color.h"
#include "../util/math/conv.h"
#include "../util/profiling.h"
#include "../util/string_funcs.h"
#include "mesh_sdf.h"
// Necessary when compiling with GodotCpp because it is used in a registered method argument, and the type must be
// defined
#include "../util/godot/classes/scene_tree.h"

namespace zylann::voxel {

static bool prepare_triangles(
		Mesh &mesh, std::vector<mesh_sdf::Triangle> &triangles, Vector3f &out_min_pos, Vector3f &out_max_pos) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND_V(mesh.get_surface_count() == 0, false);
	if (mesh.get_surface_count() > 1) {
		WARN_PRINT("The given mesh has more than one surface. Only the first will be used.");
	}
	Array surface;
	{
		ZN_PROFILE_SCOPE_NAMED("Get surface from Godot")
		surface = mesh.surface_get_arrays(0);
	}
	PackedVector3Array positions = surface[Mesh::ARRAY_VERTEX];
	PackedInt32Array indices = surface[Mesh::ARRAY_INDEX];
	ERR_FAIL_COND_V(
			!mesh_sdf::prepare_triangles(to_span(positions), to_span(indices), triangles, out_min_pos, out_max_pos),
			false);
	return true;
}

bool VoxelMeshSDF::is_baked() const {
	return _voxel_buffer.is_valid();
}

bool VoxelMeshSDF::is_baking() const {
	return _is_baking;
}

int VoxelMeshSDF::get_cell_count() const {
	return _cell_count;
}

void VoxelMeshSDF::set_cell_count(int cc) {
	_cell_count = math::clamp(cc, MIN_CELL_COUNT, MAX_CELL_COUNT);
}

float VoxelMeshSDF::get_margin_ratio() const {
	return _margin_ratio;
}

void VoxelMeshSDF::set_margin_ratio(float mr) {
	_margin_ratio = math::clamp(mr, MIN_MARGIN_RATIO, MAX_MARGIN_RATIO);
}

VoxelMeshSDF::BakeMode VoxelMeshSDF::get_bake_mode() const {
	return _bake_mode;
}

void VoxelMeshSDF::set_bake_mode(BakeMode mode) {
	ERR_FAIL_INDEX(mode, BAKE_MODE_COUNT);
	_bake_mode = mode;
}

int VoxelMeshSDF::get_partition_subdiv() const {
	return _partition_subdiv;
}

void VoxelMeshSDF::set_partition_subdiv(int subdiv) {
	ERR_FAIL_COND(subdiv < MIN_PARTITION_SUBDIV || subdiv > MAX_PARTITION_SUBDIV);
	_partition_subdiv = subdiv;
}

void VoxelMeshSDF::set_boundary_sign_fix_enabled(bool enable) {
	_boundary_sign_fix = enable;
}

bool VoxelMeshSDF::is_boundary_sign_fix_enabled() const {
	return _boundary_sign_fix;
}

void VoxelMeshSDF::set_mesh(Ref<Mesh> mesh) {
	_mesh = mesh;
}

Ref<Mesh> VoxelMeshSDF::get_mesh() const {
	return _mesh;
}

void VoxelMeshSDF::bake() {
	ZN_DSTACK();
	ZN_PROFILE_SCOPE();

	Ref<Mesh> mesh = _mesh;
	ERR_FAIL_COND(mesh.is_null());

	std::vector<mesh_sdf::Triangle> triangles;
	Vector3f min_pos;
	Vector3f max_pos;
	ERR_FAIL_COND(!prepare_triangles(**mesh, triangles, min_pos, max_pos));

	const Vector3f mesh_size = max_pos - min_pos;

	const Vector3f box_min_pos = min_pos - mesh_size * _margin_ratio;
	const Vector3f box_max_pos = max_pos + mesh_size * _margin_ratio;
	const Vector3f box_size = box_max_pos - box_min_pos;

	const Vector3i res = mesh_sdf::auto_compute_grid_resolution(box_size, _cell_count);
	const VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_SDF;
	Ref<gd::VoxelBuffer> vbgd;
	vbgd.instantiate();
	VoxelBufferInternal &buffer = vbgd->get_buffer();
	buffer.set_channel_depth(channel, VoxelBufferInternal::DEPTH_32_BIT);
	buffer.create(res);
	buffer.decompress_channel(channel);
	Span<float> sdf_grid;
	ERR_FAIL_COND(!buffer.get_channel_data(channel, sdf_grid));

	switch (_bake_mode) {
		case BAKE_MODE_ACCURATE_NAIVE:
			mesh_sdf::generate_mesh_sdf_naive(sdf_grid, res, to_span(triangles), box_min_pos, box_max_pos);
			break;
		case BAKE_MODE_ACCURATE_PARTITIONED:
			mesh_sdf::generate_mesh_sdf_partitioned(
					sdf_grid, res, to_span(triangles), box_min_pos, box_max_pos, _partition_subdiv);
			break;
		case BAKE_MODE_APPROX_INTERP:
			mesh_sdf::generate_mesh_sdf_approx_interp(sdf_grid, res, to_span(triangles), box_min_pos, box_max_pos);
			break;
		case BAKE_MODE_APPROX_FLOODFILL: {
			mesh_sdf::ChunkGrid chunk_grid;
			mesh_sdf::partition_triangles(_partition_subdiv, to_span(triangles), box_min_pos, box_max_pos, chunk_grid);
			// mesh_sdf::compute_near_chunks(chunk_grid);
			mesh_sdf::generate_mesh_sdf_approx_floodfill(
					sdf_grid, res, to_span(triangles), chunk_grid, box_min_pos, box_max_pos, _boundary_sign_fix);
		} break;
		default:
			ZN_CRASH();
	}

	if (_boundary_sign_fix && _bake_mode != BAKE_MODE_APPROX_FLOODFILL) {
		mesh_sdf::fix_sdf_sign_from_boundary(sdf_grid, res, min_pos, max_pos);
	}

	_voxel_buffer = vbgd;
	_min_pos = box_min_pos;
	_max_pos = box_max_pos;
}

#ifdef ZN_GODOT_EXTENSION
void VoxelMeshSDF::bake_async(Object *scene_tree_o) {
	SceneTree *scene_tree = Object::cast_to<SceneTree>(scene_tree_o);
#else
void VoxelMeshSDF::bake_async(SceneTree *scene_tree) {
#endif
	ZN_ASSERT_RETURN(scene_tree != nullptr);
	VoxelEngineUpdater::ensure_existence(scene_tree);

	// ZN_ASSERT_RETURN_MSG(!_is_baking, "Already baking");

	struct L {
		static void notify_on_complete(VoxelMeshSDF &obj, mesh_sdf::GenMeshSDFSubBoxTask::SharedData &shared_data) {
			Ref<gd::VoxelBuffer> vbgd;
			vbgd.instantiate();
			shared_data.buffer.move_to(vbgd->get_buffer());
			obj.call_deferred(
					"_on_bake_async_completed", vbgd, to_godot(shared_data.min_pos), to_godot(shared_data.max_pos));
		}
	};

	class GenMeshSDFSubBoxTaskGD : public mesh_sdf::GenMeshSDFSubBoxTask {
	public:
		Ref<VoxelMeshSDF> obj_to_notify;

		void on_complete() override {
			ZN_ASSERT(obj_to_notify.is_valid());
			L::notify_on_complete(**obj_to_notify, *shared_data);
		}
	};

	class GenMeshSDFFirstPassTask : public IThreadedTask {
	public:
		float margin_ratio;
		int cell_count;
		BakeMode bake_mode;
		uint8_t partition_subdiv;
		bool boundary_sign_fix;
		Array surface;
		Ref<VoxelMeshSDF> obj_to_notify;

		const char *get_debug_name() const override {
			return "GenMeshSDFFirstPass";
		}

		void run(ThreadedTaskContext ctx) override {
			ZN_DSTACK();
			ZN_PROFILE_SCOPE();
			ZN_ASSERT(obj_to_notify.is_valid());

			std::shared_ptr<mesh_sdf::GenMeshSDFSubBoxTask::SharedData> shared_data =
					make_shared_instance<mesh_sdf::GenMeshSDFSubBoxTask::SharedData>();
			Vector3f min_pos;
			Vector3f max_pos;

			PackedVector3Array positions = surface[Mesh::ARRAY_VERTEX];
			PackedInt32Array indices = surface[Mesh::ARRAY_INDEX];
			if (!mesh_sdf::prepare_triangles(
						to_span(positions), to_span(indices), shared_data->triangles, min_pos, max_pos)) {
				ZN_PRINT_ERROR("Failed preparing triangles in threaded task");
				report_error();
				return;
			}

			const Vector3f mesh_size = max_pos - min_pos;

			const Vector3f box_min_pos = min_pos - mesh_size * margin_ratio;
			const Vector3f box_max_pos = max_pos + mesh_size * margin_ratio;
			const Vector3f box_size = box_max_pos - box_min_pos;

			const Vector3i res = mesh_sdf::auto_compute_grid_resolution(box_size, cell_count);
			const VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_SDF;
			shared_data->buffer.set_channel_depth(channel, VoxelBufferInternal::DEPTH_32_BIT);
			shared_data->buffer.create(res);
			shared_data->buffer.decompress_channel(channel);

			shared_data->min_pos = box_min_pos;
			shared_data->max_pos = box_max_pos;

			switch (bake_mode) {
				case BAKE_MODE_ACCURATE_NAIVE:
				case BAKE_MODE_ACCURATE_PARTITIONED: {
					// These two approaches are better parallelized

					const bool partitioned = bake_mode == BAKE_MODE_ACCURATE_PARTITIONED;
					if (partitioned) {
						mesh_sdf::partition_triangles(partition_subdiv, to_span(shared_data->triangles),
								shared_data->min_pos, shared_data->max_pos, shared_data->chunk_grid);
						mesh_sdf::compute_near_chunks(shared_data->chunk_grid);
					}
					shared_data->use_chunk_grid = partitioned;

					shared_data->boundary_sign_fix = boundary_sign_fix;

					// Spawn a parallel task for every Z slice of the grid.
					// Indexing is ZXY so each thread accesses a contiguous part of memory.
					shared_data->pending_jobs = res.z;

					for (int z = 0; z < res.z; ++z) {
						GenMeshSDFSubBoxTaskGD *task = ZN_NEW(GenMeshSDFSubBoxTaskGD);
						task->shared_data = shared_data;
						task->box = Box3i(Vector3i(0, 0, z), Vector3i(res.x, res.y, 1));
						task->obj_to_notify = obj_to_notify;

						VoxelEngine::get_singleton().push_async_task(task);
					}
				} break;

				case BAKE_MODE_APPROX_INTERP: {
					VoxelBufferInternal &buffer = shared_data->buffer;
					Span<float> sdf_grid;
					ZN_ASSERT(buffer.get_channel_data(channel, sdf_grid));

					mesh_sdf::generate_mesh_sdf_approx_interp(
							sdf_grid, res, to_span(shared_data->triangles), box_min_pos, box_max_pos);

					if (boundary_sign_fix) {
						mesh_sdf::fix_sdf_sign_from_boundary(sdf_grid, res, box_min_pos, box_max_pos);
					}

					L::notify_on_complete(**obj_to_notify, *shared_data);
				} break;

				case BAKE_MODE_APPROX_FLOODFILL: {
					VoxelBufferInternal &buffer = shared_data->buffer;
					Span<float> sdf_grid;
					ZN_ASSERT(buffer.get_channel_data(channel, sdf_grid));

					mesh_sdf::partition_triangles(partition_subdiv, to_span(shared_data->triangles),
							shared_data->min_pos, shared_data->max_pos, shared_data->chunk_grid);

					// mesh_sdf::compute_near_chunks(shared_data->chunk_grid);

					mesh_sdf::generate_mesh_sdf_approx_floodfill(sdf_grid, res, to_span(shared_data->triangles),
							shared_data->chunk_grid, box_min_pos, box_max_pos, boundary_sign_fix);

					L::notify_on_complete(**obj_to_notify, *shared_data);
				} break;

				default:
					ZN_PRINT_ERROR(format("Invalid bake mode {}", bake_mode));
					report_error();
					break;
			}
		}

	private:
		void report_error() {
			obj_to_notify->call_deferred("_on_bake_async_completed", Ref<gd::VoxelBuffer>(), Vector3(), Vector3());
		}
	};

	Ref<Mesh> mesh = _mesh;
	ERR_FAIL_COND(mesh.is_null());

	ERR_FAIL_COND(mesh->get_surface_count() == 0);
	Array surface;
	{
		ZN_PROFILE_SCOPE_NAMED("Get surface from Godot")
		surface = mesh->surface_get_arrays(0);
	}

	_is_baking = true;

	GenMeshSDFFirstPassTask *task = ZN_NEW(GenMeshSDFFirstPassTask);
	task->cell_count = _cell_count;
	task->margin_ratio = _margin_ratio;
	task->bake_mode = _bake_mode;
	task->partition_subdiv = _partition_subdiv;
	task->surface = surface;
	task->obj_to_notify.reference_ptr(this);
	task->boundary_sign_fix = _boundary_sign_fix;
	VoxelEngine::get_singleton().push_async_task(task);
}

void VoxelMeshSDF::_on_bake_async_completed(Ref<gd::VoxelBuffer> buffer, Vector3 min_pos, Vector3 max_pos) {
	_is_baking = false;

	// This can mean an error occurred during one of the tasks
	ZN_ASSERT_RETURN(buffer.is_valid());

	_voxel_buffer = buffer;
	_min_pos = to_vec3f(min_pos);
	_max_pos = to_vec3f(max_pos);
	emit_signal("baked");
}

Ref<gd::VoxelBuffer> VoxelMeshSDF::get_voxel_buffer() const {
	return _voxel_buffer;
}

AABB VoxelMeshSDF::get_aabb() const {
	return AABB(to_godot(_min_pos), to_godot(_max_pos - _min_pos));
}

std::shared_ptr<ComputeShaderResource> VoxelMeshSDF::get_gpu_resource() {
	MutexLock mlock(_gpu_resource_mutex);

	if (_gpu_resource == nullptr && _voxel_buffer.is_valid()) {
		const VoxelBufferInternal &buffer = _voxel_buffer->get_buffer();

		Span<const float> sdf_grid;
		ZN_ASSERT_RETURN_V(buffer.get_channel_data(VoxelBufferInternal::CHANNEL_SDF, sdf_grid), _gpu_resource);

		std::shared_ptr<ComputeShaderResource> resource = make_shared_instance<ComputeShaderResource>();
		resource->create_texture_3d_zxy(sdf_grid, buffer.get_size());
		_gpu_resource = resource;
	}

	return _gpu_resource;
}

Array VoxelMeshSDF::debug_check_sdf(Ref<Mesh> mesh) {
	Array result;

	ZN_ASSERT_RETURN_V(is_baked(), result);
	ZN_ASSERT(_voxel_buffer.is_valid());
	const VoxelBufferInternal &buffer = _voxel_buffer->get_buffer();
	Span<const float> sdf_grid;
	ZN_ASSERT_RETURN_V(buffer.get_channel_data(VoxelBufferInternal::CHANNEL_SDF, sdf_grid), result);

	ZN_ASSERT_RETURN_V(mesh.is_valid(), result);
	std::vector<mesh_sdf::Triangle> triangles;
	Vector3f min_pos;
	Vector3f max_pos;
	ZN_ASSERT_RETURN_V(prepare_triangles(**mesh, triangles, min_pos, max_pos), result);

	const mesh_sdf::CheckResult cr =
			mesh_sdf::check_sdf(sdf_grid, buffer.get_size(), to_span(triangles), _min_pos, _max_pos);

	if (cr.ok) {
		return result;
	}

	const mesh_sdf::Triangle &ct0 = triangles[cr.cell0.closest_triangle_index];
	const mesh_sdf::Triangle &ct1 = triangles[cr.cell1.closest_triangle_index];

	result.resize(8);
	result[0] = to_godot(cr.cell0.mesh_pos);
	result[1] = to_godot(ct0.v1);
	result[2] = to_godot(ct0.v2);
	result[3] = to_godot(ct0.v3);
	result[4] = to_godot(cr.cell1.mesh_pos);
	result[5] = to_godot(ct1.v1);
	result[6] = to_godot(ct1.v2);
	result[7] = to_godot(ct1.v3);
	return result;
}

Dictionary VoxelMeshSDF::_b_get_data() const {
	if (_voxel_buffer.is_null()) {
		return Dictionary();
	}
	const VoxelBufferInternal &vb = _voxel_buffer->get_buffer();

	Dictionary d;
	d["v"] = 0;

	d["res"] = vb.get_size();

	PackedFloat32Array sdf_f32;
	sdf_f32.resize(Vector3iUtil::get_volume(vb.get_size()));
	Span<const float> channel;
	ERR_FAIL_COND_V(!vb.get_channel_data(VoxelBufferInternal::CHANNEL_SDF, channel), Dictionary());
	memcpy(sdf_f32.ptrw(), channel.data(), channel.size() * sizeof(float));
	d["sdf_f32"] = sdf_f32;

	d["min_pos"] = to_godot(_min_pos);
	d["max_pos"] = to_godot(_max_pos);

	return d;
}

void VoxelMeshSDF::_b_set_data(Dictionary d) {
	ZN_DSTACK();
	if (_is_baking) {
		WARN_PRINT("Setting data while baking, that data will be overwritten when baking ends.");
	}

	ERR_FAIL_COND(d.is_empty());

	Vector3i res;
	ERR_FAIL_COND(!try_get(d, "res", res));
	ERR_FAIL_COND(Vector3iUtil::is_empty_size(res));

	_voxel_buffer.instantiate();
	VoxelBufferInternal &vb = _voxel_buffer->get_buffer();
	vb.create(res);
	vb.set_channel_depth(VoxelBufferInternal::CHANNEL_SDF, VoxelBufferInternal::DEPTH_32_BIT);
	vb.decompress_channel(VoxelBufferInternal::CHANNEL_SDF);
	Span<float> channel;
	ERR_FAIL_COND(!vb.get_channel_data(VoxelBufferInternal::CHANNEL_SDF, channel));

	PackedFloat32Array sdf_f32;
	ERR_FAIL_COND(!try_get(d, "sdf_f32", sdf_f32));
	memcpy(channel.data(), sdf_f32.ptr(), channel.size() * sizeof(float));

	_min_pos = to_vec3f(Vector3(d["min_pos"]));
	_max_pos = to_vec3f(Vector3(d["max_pos"]));
}

void VoxelMeshSDF::_bind_methods() {
	ClassDB::bind_method(D_METHOD("bake"), &VoxelMeshSDF::bake);
	ClassDB::bind_method(D_METHOD("bake_async", "scene_tree"), &VoxelMeshSDF::bake_async);

	ClassDB::bind_method(D_METHOD("is_baked"), &VoxelMeshSDF::is_baked);
	ClassDB::bind_method(D_METHOD("is_baking"), &VoxelMeshSDF::is_baking);

	ClassDB::bind_method(D_METHOD("get_mesh"), &VoxelMeshSDF::get_mesh);
	ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &VoxelMeshSDF::set_mesh);

	ClassDB::bind_method(D_METHOD("get_cell_count"), &VoxelMeshSDF::get_cell_count);
	ClassDB::bind_method(D_METHOD("set_cell_count", "cell_count"), &VoxelMeshSDF::set_cell_count);

	ClassDB::bind_method(D_METHOD("get_margin_ratio"), &VoxelMeshSDF::get_margin_ratio);
	ClassDB::bind_method(D_METHOD("set_margin_ratio", "ratio"), &VoxelMeshSDF::set_margin_ratio);

	ClassDB::bind_method(D_METHOD("get_bake_mode"), &VoxelMeshSDF::get_bake_mode);
	ClassDB::bind_method(D_METHOD("set_bake_mode", "mode"), &VoxelMeshSDF::set_bake_mode);

	ClassDB::bind_method(D_METHOD("get_partition_subdiv"), &VoxelMeshSDF::get_partition_subdiv);
	ClassDB::bind_method(D_METHOD("set_partition_subdiv", "subdiv"), &VoxelMeshSDF::set_partition_subdiv);

	ClassDB::bind_method(D_METHOD("is_boundary_sign_fix_enabled"), &VoxelMeshSDF::is_boundary_sign_fix_enabled);
	ClassDB::bind_method(
			D_METHOD("set_boundary_sign_fix_enabled", "enable"), &VoxelMeshSDF::set_boundary_sign_fix_enabled);

	ClassDB::bind_method(D_METHOD("get_voxel_buffer"), &VoxelMeshSDF::get_voxel_buffer);
	ClassDB::bind_method(D_METHOD("get_aabb"), &VoxelMeshSDF::get_aabb);
	ClassDB::bind_method(D_METHOD("debug_check_sdf", "mesh"), &VoxelMeshSDF::debug_check_sdf);

	// Internal
	ClassDB::bind_method(D_METHOD("_on_bake_async_completed", "buffer", "min_pos", "max_pos"),
			&VoxelMeshSDF::_on_bake_async_completed);
	ClassDB::bind_method(D_METHOD("_get_data"), &VoxelMeshSDF::_b_get_data);
	ClassDB::bind_method(D_METHOD("_set_data", "d"), &VoxelMeshSDF::_b_set_data);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, Mesh::get_class_static()),
			"set_mesh", "get_mesh");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "cell_count", PROPERTY_HINT_RANGE,
						 String("{0},{1},1").format(varray(MIN_CELL_COUNT, MAX_CELL_COUNT))),
			"set_cell_count", "get_cell_count");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "margin_ratio", PROPERTY_HINT_RANGE,
						 String("{0},{1},0.01").format(varray(MIN_MARGIN_RATIO, MAX_MARGIN_RATIO))),
			"set_margin_ratio", "get_margin_ratio");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "bake_mode", PROPERTY_HINT_ENUM,
						 "AccurateNaive,AccuratePartitioned,ApproxInterp,FloodFill"),
			"set_bake_mode", "get_bake_mode");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "partition_subdiv", PROPERTY_HINT_RANGE,
						 String("{0},{1},1").format(varray(MIN_PARTITION_SUBDIV, MAX_PARTITION_SUBDIV))),
			"set_partition_subdiv", "get_partition_subdiv");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "boundary_sign_fix_enabled"), "set_boundary_sign_fix_enabled",
			"is_boundary_sign_fix_enabled");

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE),
			"_set_data", "_get_data");

	ADD_SIGNAL(MethodInfo("baked"));

	// These modes are mostly for experimentation, I'm not sure if they will remain
	BIND_ENUM_CONSTANT(BAKE_MODE_ACCURATE_NAIVE);
	BIND_ENUM_CONSTANT(BAKE_MODE_ACCURATE_PARTITIONED);
	BIND_ENUM_CONSTANT(BAKE_MODE_APPROX_INTERP);
	BIND_ENUM_CONSTANT(BAKE_MODE_APPROX_FLOODFILL);
	BIND_ENUM_CONSTANT(BAKE_MODE_COUNT);
}

} // namespace zylann::voxel
