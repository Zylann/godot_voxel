#include "voxel_modifier_sphere.h"
#include "../engine/voxel_engine.h"
#include "../util/godot/funcs.h"
#include "../util/math/sdf.h"
#include "../util/memory.h"
#include "../util/profiling.h"

namespace zylann::voxel {

void VoxelModifierSphere::set_radius(real_t radius) {
	RWLockWrite wlock(_rwlock);
	if (radius == _radius) {
		return;
	}
	_radius = radius;
	_shader_data_need_update = true;
	update_aabb();
}

real_t VoxelModifierSphere::get_radius() const {
	return _radius;
}

void VoxelModifierSphere::update_aabb() {
	const float extent = _radius * 1.25f + get_smoothness();
	const float extent2 = 2.0 * extent;
	_aabb = AABB(get_transform().origin - Vector3(extent, extent, extent), Vector3(extent2, extent2, extent2));
}

void VoxelModifierSphere::apply(VoxelModifierContext ctx) const {
	ZN_PROFILE_SCOPE();
	RWLockRead rlock(_rwlock);
	const float smoothness = get_smoothness();
	const Vector3 center = get_transform().origin;
	const float sdf_scale = 1.0f;

	// TODO Support transform scale

	switch (get_operation()) {
		case OP_ADD:
			for (unsigned int i = 0; i < ctx.sdf.size(); ++i) {
				const float sd = sdf_scale * math::sdf_sphere(ctx.positions[i], center, _radius);
				ctx.sdf[i] = math::sdf_smooth_union(ctx.sdf[i], sd, smoothness);
			}
			break;

		case OP_SUBTRACT:
			for (unsigned int i = 0; i < ctx.sdf.size(); ++i) {
				const float sd = sdf_scale * math::sdf_sphere(ctx.positions[i], center, _radius);
				ctx.sdf[i] = math::sdf_smooth_subtract(ctx.sdf[i], sd, smoothness);
			}
			break;

		default:
			ZN_CRASH();
	}
}

void VoxelModifierSphere::get_shader_data(ShaderData &out_shader_data) {
	struct SphereParams {
		float radius;
	};

	RWLockWrite wlock(_rwlock);

	if (_shader_data_need_update || _shader_data == nullptr) {
		update_base_shader_data_no_lock();

		SphereParams sphere_params;
		sphere_params.radius = _radius;
		PackedByteArray pba;
		copy_bytes_to(pba, sphere_params);

		if (_shader_data->params.size() < 2) {
			std::shared_ptr<ComputeShaderResource> res = make_shared_instance<ComputeShaderResource>();
			res->create_storage_buffer(pba);
			_shader_data->params.push_back(ComputeShaderParameter{ 5, res });

		} else if (_shader_data_need_update) {
			ZN_ASSERT(_shader_data->params.size() == 2);
			_shader_data->params[1].resource->update_storage_buffer(pba);
		}

		_shader_data_need_update = false;
	}

	out_shader_data.detail_rendering_shader_rid =
			VoxelEngine::get_singleton().get_detail_modifier_sphere_shader().get_rid();
	out_shader_data.params = _shader_data;
}

} // namespace zylann::voxel
