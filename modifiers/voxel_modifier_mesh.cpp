#include "voxel_modifier_mesh.h"
#include "../edition/funcs.h"
#include "../engine/voxel_engine.h"
#include "../util/godot/funcs.h"
#include "../util/math/conv.h"
#include "../util/profiling.h"

namespace zylann::voxel {

void VoxelModifierMesh::set_mesh_sdf(Ref<VoxelMeshSDF> mesh_sdf) {
	// ZN_ASSERT_RETURN(buffer != nullptr);
	RWLockWrite wlock(_rwlock);
	_mesh_sdf = mesh_sdf;
	_shader_data_need_update = true;
	update_aabb();
}

void VoxelModifierMesh::set_isolevel(float isolevel) {
	RWLockWrite wlock(_rwlock);
	if (isolevel == isolevel) {
		return;
	}
	_isolevel = isolevel;
}

inline float get_largest_coord(Vector3 v) {
	return math::max(math::max(v.x, v.y), v.z);
}

void VoxelModifierMesh::apply(VoxelModifierContext ctx) const {
	ZN_PROFILE_SCOPE();

	RWLockRead rlock(_rwlock);
	if (_mesh_sdf.is_null()) {
		return;
	}
	Ref<gd::VoxelBuffer> voxel_buffer_gd = _mesh_sdf->get_voxel_buffer();
	if (voxel_buffer_gd.is_null()) {
		return;
	}
	const VoxelBufferInternal &buffer = voxel_buffer_gd->get_buffer();

	// TODO VoxelMeshSDF isn't preventing scripts from writing into this buffer from a different thread.
	// I can't think of a reason to manually modify the buffer of a VoxelMeshSDF at the moment.
	RWLockRead buffer_rlock(buffer.get_lock());

	const Vector3f min_pos = _mesh_sdf->get_aabb_min_pos();
	const Vector3f max_pos = _mesh_sdf->get_aabb_max_pos();

	const Transform3D model_to_world = get_transform();
	const Transform3D buffer_to_model =
			Transform3D(Basis().scaled(to_vec3(max_pos - min_pos) / to_vec3(buffer.get_size())), to_vec3(min_pos));
	const Transform3D buffer_to_world = model_to_world * buffer_to_model;

	Span<const float> buffer_sdf;
	ZN_ASSERT_RETURN(buffer.get_channel_data(VoxelBufferInternal::CHANNEL_SDF, buffer_sdf));
	const float smoothness = get_smoothness();

	ops::SdfBufferShape shape;
	shape.buffer = buffer_sdf;
	shape.buffer_size = buffer.get_size();
	shape.isolevel = _isolevel;
	shape.sdf_scale = get_largest_coord(model_to_world.get_basis().get_scale());
	shape.world_to_buffer = buffer_to_world.affine_inverse();

	switch (get_operation()) {
		case OP_ADD:
			for (unsigned int i = 0; i < ctx.sdf.size(); ++i) {
				const float sd = shape(ctx.positions[i]);
				ctx.sdf[i] = math::sdf_smooth_union(ctx.sdf[i], sd, smoothness);
			}
			break;

		case OP_SUBTRACT:
			for (unsigned int i = 0; i < ctx.sdf.size(); ++i) {
				const float sd = shape(ctx.positions[i]);
				ctx.sdf[i] = math::sdf_smooth_subtract(ctx.sdf[i], sd, smoothness);
			}
			break;

		default:
			ZN_CRASH();
	}
}

void VoxelModifierMesh::update_aabb() {
	// ZN_ASSERT_RETURN(_mesh_sdf.is_valid());
	if (_mesh_sdf.is_null()) {
		return;
	}
	const Transform3D &model_to_world = get_transform();
	_aabb = model_to_world.xform(_mesh_sdf->get_aabb());
}

void VoxelModifierMesh::get_shader_data(ShaderData &out_shader_data) {
	struct MeshParams {
		Vector3f model_to_buffer_translation;
		Vector3f model_to_buffer_scale;
		float isolevel;
	};

	if (_mesh_sdf.is_null()) {
		return;
	}

	RWLockWrite wlock(_rwlock);

	if (_shader_data_need_update || _shader_data == nullptr) {
		update_base_shader_data_no_lock();

		const Vector3f min_pos = _mesh_sdf->get_aabb_min_pos();
		const Vector3f max_pos = _mesh_sdf->get_aabb_max_pos();

		MeshParams mesh_params;
		// The shader uses a sampler3D so coordinates are normalized
		mesh_params.model_to_buffer_scale = Vector3f(1.f) / (max_pos - min_pos);
		mesh_params.model_to_buffer_translation = min_pos;
		mesh_params.isolevel = _isolevel;
		PackedByteArray pba;
		copy_bytes_to(pba, mesh_params);

		if (_shader_data->params.size() < 3) {
			std::shared_ptr<ComputeShaderResource> params_res = make_shared_instance<ComputeShaderResource>();
			params_res->create_storage_buffer(pba);
			_shader_data->params.push_back(ComputeShaderParameter{ 5, params_res });

			std::shared_ptr<ComputeShaderResource> buffer_res = _mesh_sdf->get_gpu_resource();
			_shader_data->params.push_back(ComputeShaderParameter{ 6, buffer_res });

		} else if (_shader_data_need_update) {
			_shader_data->params[1].resource->update_storage_buffer(pba);
			_shader_data->params[2].resource = _mesh_sdf->get_gpu_resource();
		}

		_shader_data_need_update = false;
	}

	out_shader_data.detail_rendering_shader_rid =
			VoxelEngine::get_singleton().get_detail_modifier_sphere_shader().get_rid();
	out_shader_data.params = _shader_data;
}

void VoxelModifierMesh::request_shader_data_update() {
	_shader_data_need_update = true;
}

} // namespace zylann::voxel
