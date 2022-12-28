#include "voxel_modifier_stack.h"
#include "../edition/funcs.h"
#include "../util/dstack.h"
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

} // namespace zylann::voxel
