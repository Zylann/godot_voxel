#include "modifiers.h"
#include "../edition/funcs.h"
#include "../util/dstack.h"
#include "../util/math/conv.h"
#include "../util/profiling.h"

namespace zylann::voxel {

namespace {
thread_local std::vector<float> tls_sdf;
thread_local std::vector<Vector3> tls_positions;

Span<const Vector3> get_positions_temporary(Vector3i buffer_size, Vector3 origin, Vector3 size) {
	tls_positions.resize(Vector3iUtil::get_volume(buffer_size));
	Span<Vector3> positions = to_span(tls_positions);

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

Span<float> decompress_sdf_to_temporary(VoxelBufferInternal &voxels) {
	ZN_DSTACK();
	const Vector3i bs = voxels.get_size();
	tls_sdf.resize(Vector3iUtil::get_volume(bs));
	Span<float> sdf = to_span(tls_sdf);

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

void store_sdf(VoxelBufferInternal &voxels, Span<float> sdf) {
	const VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_SDF;
	const VoxelBufferInternal::Depth depth = voxels.get_channel_depth(channel);

	const float scale = VoxelBufferInternal::get_sdf_quantization_scale(depth);
	for (unsigned int i = 0; i < sdf.size(); ++i) {
		sdf[i] *= scale;
	}

	switch (depth) {
		case VoxelBufferInternal::DEPTH_8_BIT: {
			Span<int8_t> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			for (unsigned int i = 0; i < sdf.size(); ++i) {
				raw[i] = snorm_to_s8(sdf[i]);
			}
		} break;

		case VoxelBufferInternal::DEPTH_16_BIT: {
			Span<int16_t> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			for (unsigned int i = 0; i < sdf.size(); ++i) {
				raw[i] = snorm_to_s16(sdf[i]);
			}
		} break;

		case VoxelBufferInternal::DEPTH_32_BIT: {
			Span<float> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			memcpy(raw.data(), sdf.data(), sizeof(float) * sdf.size());
		} break;

		case VoxelBufferInternal::DEPTH_64_BIT: {
			Span<double> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			for (unsigned int i = 0; i < sdf.size(); ++i) {
				raw[i] = sdf[i];
			}
		} break;

		default:
			ZN_CRASH();
	}
}

} //namespace

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
		store_sdf(voxels, ctx.sdf);
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

void VoxelModifierStack::clear() {
	RWLockWrite lock(_stack_lock);
	_stack.clear();
	_modifiers.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelModifierSphere::set_radius(real_t radius) {
	RWLockWrite wlock(_rwlock);
	if (radius == _radius) {
		return;
	}
	_radius = radius;
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelModifierBuffer::set_buffer(std::shared_ptr<VoxelBufferInternal> buffer, Vector3f min_pos, Vector3f max_pos) {
	//ZN_ASSERT_RETURN(buffer != nullptr);
	RWLockWrite wlock(_rwlock);
	_buffer = buffer;
	_min_pos = min_pos;
	_max_pos = max_pos;
	update_aabb();
}

void VoxelModifierBuffer::set_isolevel(float isolevel) {
	RWLockWrite wlock(_rwlock);
	if (isolevel == isolevel) {
		return;
	}
	_isolevel = isolevel;
}

float get_largest_coord(Vector3 v) {
	return math::max(math::max(v.x, v.y), v.z);
}

void VoxelModifierBuffer::apply(VoxelModifierContext ctx) const {
	ZN_PROFILE_SCOPE();

	RWLockRead rlock(_rwlock);
	if (_buffer == nullptr) {
		return;
	}

	// TODO VoxelMeshSDF isn't preventing scripts from writing into this buffer from a different thread.
	// I can't think of a reason to manually modify the buffer of a VoxelMeshSDF at the moment.
	RWLockRead buffer_rlock(_buffer->get_lock());

	const Transform3D model_to_world = get_transform();
	const Transform3D buffer_to_model =
			Transform3D(Basis().scaled(to_vec3(_max_pos - _min_pos) / to_vec3(_buffer->get_size())), to_vec3(_min_pos));
	const Transform3D buffer_to_world = model_to_world * buffer_to_model;

	Span<const float> buffer_sdf;
	ZN_ASSERT_RETURN(_buffer->get_channel_data(VoxelBufferInternal::CHANNEL_SDF, buffer_sdf));
	const float smoothness = get_smoothness();

	ops::SdfBufferShape shape;
	shape.buffer = buffer_sdf;
	shape.buffer_size = _buffer->get_size();
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

void VoxelModifierBuffer::update_aabb() {
	const Transform3D &model_to_world = get_transform();
	_aabb = model_to_world.xform(AABB(to_vec3(_min_pos), to_vec3(_max_pos - _min_pos)));
}

} // namespace zylann::voxel
