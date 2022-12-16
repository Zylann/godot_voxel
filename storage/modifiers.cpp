#include "modifiers.h"
#include "../edition/funcs.h"
#include "../engine/voxel_engine.h"
#include "../util/dstack.h"
#include "../util/godot/funcs.h"
#include "../util/math/conv.h"
#include "../util/math/vector4f.h"
#include "../util/profiling.h"

namespace zylann::voxel {

namespace {
std::vector<float> &get_tls_sdf() {
	thread_local std::vector<float> tls_sdf;
	return tls_sdf;
}

std::vector<Vector3> &get_tls_positions() {
	thread_local std::vector<Vector3> tls_positions;
	return tls_positions;
}

Span<const Vector3> get_positions_temporary(Vector3i buffer_size, Vector3 origin, Vector3 size) {
	get_tls_positions().resize(Vector3iUtil::get_volume(buffer_size));
	Span<Vector3> positions = to_span(get_tls_positions());

	const Vector3 end = origin + size;
	const Vector3 bsf = buffer_size;

	unsigned int i = 0;

	for (int z = 0; z < buffer_size.z; ++z) {
		for (int x = 0; x < buffer_size.x; ++x) {
			for (int y = 0; y < buffer_size.y; ++y) {
				positions[i] = math::lerp(origin, end, Vector3(x / bsf.x, y / bsf.y, z / bsf.z));
				++i;
			}
		}
	}

	return positions;
}

Span<const Vector3> get_positions_temporary(
		Span<const float> x_buffer, Span<const float> y_buffer, Span<const float> z_buffer) {
	ZN_ASSERT(x_buffer.size() == z_buffer.size() && y_buffer.size() == z_buffer.size());

	get_tls_positions().resize(x_buffer.size());
	Span<Vector3> positions = to_span(get_tls_positions());

	for (unsigned int i = 0; i < x_buffer.size(); ++i) {
		positions[i] = Vector3(x_buffer[i], y_buffer[i], z_buffer[i]);
	}

	return positions;
}

// TODO Use VoxelBufferInternal helper function
Span<float> decompress_sdf_to_temporary(VoxelBufferInternal &voxels) {
	ZN_DSTACK();
	const Vector3i bs = voxels.get_size();
	get_tls_sdf().resize(Vector3iUtil::get_volume(bs));
	Span<float> sdf = to_span(get_tls_sdf());

	const VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_SDF;
	voxels.decompress_channel(channel);

	const VoxelBufferInternal::Depth depth = voxels.get_channel_depth(channel);

	switch (depth) {
		case VoxelBufferInternal::DEPTH_8_BIT: {
			Span<int8_t> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			for (unsigned int i = 0; i < sdf.size(); ++i) {
				sdf[i] = s8_to_snorm(raw[i]);
			}
		} break;

		case VoxelBufferInternal::DEPTH_16_BIT: {
			Span<int16_t> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			for (unsigned int i = 0; i < sdf.size(); ++i) {
				sdf[i] = s16_to_snorm(raw[i]);
			}
		} break;

		case VoxelBufferInternal::DEPTH_32_BIT: {
			Span<float> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			memcpy(sdf.data(), raw.data(), sizeof(float) * sdf.size());
		} break;

		case VoxelBufferInternal::DEPTH_64_BIT: {
			Span<double> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			for (unsigned int i = 0; i < sdf.size(); ++i) {
				sdf[i] = raw[i];
			}
		} break;

		default:
			ZN_CRASH();
	}

	const float inv_scale = 1.0f / VoxelBufferInternal::get_sdf_quantization_scale(depth);
	for (unsigned int i = 0; i < sdf.size(); ++i) {
		sdf[i] *= inv_scale;
	}

	return sdf;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelModifierStack::VoxelModifierStack() {}

VoxelModifierStack::VoxelModifierStack(VoxelModifierStack &&other) {
	move_from_noclear(other);
}

VoxelModifierStack &VoxelModifierStack::operator=(VoxelModifierStack &&other) {
	clear();
	move_from_noclear(other);
	_next_id = other._next_id;
	return *this;
}

void VoxelModifierStack::move_from_noclear(VoxelModifierStack &other) {
	{
		RWLockRead rlock(other._stack_lock);
		_modifiers = std::move(other._modifiers);
		_stack = std::move(other._stack);
	}
	_next_id = other._next_id;
}

uint32_t VoxelModifierStack::allocate_id() {
	return ++_next_id;
}

void VoxelModifierStack::remove_modifier(uint32_t id) {
	RWLockWrite lock(_stack_lock);

	auto map_it = _modifiers.find(id);
	ZN_ASSERT_RETURN(map_it != _modifiers.end());

	const VoxelModifier *ptr = map_it->second.get();
	for (auto stack_it = _stack.begin(); stack_it != _stack.end(); ++stack_it) {
		if (*stack_it == ptr) {
			_stack.erase(stack_it);
			break;
		}
	}

	_modifiers.erase(map_it);
}

bool VoxelModifierStack::has_modifier(uint32_t id) const {
	return _modifiers.find(id) != _modifiers.end();
}

VoxelModifier *VoxelModifierStack::get_modifier(uint32_t id) const {
	auto it = _modifiers.find(id);
	if (it != _modifiers.end()) {
		return it->second.get();
	}
	return nullptr;
}

void VoxelModifierStack::apply(VoxelBufferInternal &voxels, AABB aabb) const {
	ZN_PROFILE_SCOPE();
	RWLockRead lock(_stack_lock);

	if (_stack.size() == 0) {
		return;
	}

	VoxelModifierContext ctx;
	bool any_intersection = false;

	for (unsigned int i = 0; i < _stack.size(); ++i) {
		const VoxelModifier *modifier = _stack[i];
		ZN_ASSERT(modifier != nullptr);

		if (modifier->get_aabb().intersects(aabb)) {
			if (any_intersection == false) {
				any_intersection = true;

				ctx.positions = get_positions_temporary(voxels.get_size(), aabb.position, aabb.size);
				ctx.sdf = decompress_sdf_to_temporary(voxels);
			}

			modifier->apply(ctx);
		}
	}

	if (any_intersection) {
		scale_and_store_sdf(voxels, ctx.sdf);
		voxels.compress_uniform_channels();
	}
}

void VoxelModifierStack::apply(float &sdf, Vector3 position) const {
	ZN_PROFILE_SCOPE();
	RWLockRead lock(_stack_lock);

	if (_stack.size() == 0) {
		return;
	}

	VoxelModifierContext ctx;
	ctx.positions = Span<Vector3>(&position, 1);
	ctx.sdf = Span<float>(&sdf, 1);

	const AABB aabb(position, Vector3(1, 1, 1));

	for (unsigned int i = 0; i < _stack.size(); ++i) {
		const VoxelModifier *modifier = _stack[i];
		ZN_ASSERT(modifier != nullptr);

		if (modifier->get_aabb().intersects(aabb)) {
			modifier->apply(ctx);
		}
	}
}

void VoxelModifierStack::apply(Span<const float> x_buffer, Span<const float> y_buffer, Span<const float> z_buffer,
		Span<float> sdf_buffer, Vector3f min_pos, Vector3f max_pos) const {
	ZN_PROFILE_SCOPE();
	RWLockRead lock(_stack_lock);

	if (_stack.size() == 0) {
		return;
	}

	VoxelModifierContext ctx;
	ctx.positions = get_positions_temporary(x_buffer, y_buffer, z_buffer);
	ctx.sdf = sdf_buffer;

	const AABB aabb(to_vec3(min_pos), to_vec3(max_pos - min_pos));

	for (unsigned int i = 0; i < _stack.size(); ++i) {
		const VoxelModifier *modifier = _stack[i];
		ZN_ASSERT(modifier != nullptr);

		if (modifier->get_aabb().intersects(aabb)) {
			modifier->apply(ctx);
		}
	}
}

void VoxelModifierStack::apply_for_detail_gpu_rendering(
		std::vector<VoxelModifier::ShaderData> &out_data, AABB aabb) const {
	ZN_PROFILE_SCOPE();
	RWLockRead lock(_stack_lock);

	if (_stack.size() == 0) {
		return;
	}

	for (unsigned int i = 0; i < _stack.size(); ++i) {
		VoxelModifier *modifier = _stack[i];
		ZN_ASSERT(modifier != nullptr);

		if (modifier->get_aabb().intersects(aabb)) {
			VoxelModifier::ShaderData sd;
			modifier->get_shader_data(sd);
			if (sd.detail_rendering_shader_rid.is_valid()) {
				out_data.push_back(sd);
			}
		}
	}
}

void VoxelModifierStack::clear() {
	RWLockWrite lock(_stack_lock);
	_stack.clear();
	_modifiers.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelModifier::set_transform(Transform3D t) {
	RWLockWrite wlock(_rwlock);
	if (t == _transform) {
		return;
	}
	_transform = t;
	_shader_data_need_update = true;
	update_aabb();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelModifierSdf::set_operation(Operation op) {
	RWLockWrite wlock(_rwlock);
	_operation = op;
	_shader_data_need_update = true;
}

void VoxelModifierSdf::set_smoothness(float p_smoothness) {
	RWLockWrite wlock(_rwlock);
	const float smoothness = math::max(p_smoothness, 0.f);
	if (smoothness == _smoothness) {
		return;
	}
	_smoothness = smoothness;
	_shader_data_need_update = true;
	update_aabb();
}

inline float get_largest_coord(Vector3 v) {
	return math::max(math::max(v.x, v.y), v.z);
}

void VoxelModifierSdf::update_base_shader_data_no_lock() {
	struct BaseModifierParams {
		FixedArray<float, 16> world_to_model;
		int32_t operation;
		float smoothness;
		float sd_scale;
	};

	BaseModifierParams base_params;
	transform3d_to_mat4(get_transform().affine_inverse(), to_span(base_params.world_to_model));
	base_params.operation = get_operation();
	base_params.smoothness = get_smoothness();
	base_params.sd_scale = get_largest_coord(get_transform().get_basis().get_scale());
	PackedByteArray pba0;
	copy_bytes_to(pba0, base_params);

	if (_shader_data == nullptr) {
		_shader_data = make_shared_instance<ComputeShaderParameters>();

		std::shared_ptr<ComputeShaderResource> res0 = make_shared_instance<ComputeShaderResource>();
		res0->create_storage_buffer(pba0);
		_shader_data->params.push_back(ComputeShaderParameter{ 4, res0 });

	} else {
		ZN_ASSERT(_shader_data->params.size() >= 1);
		_shader_data->params[0].resource->update_storage_buffer(pba0);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
