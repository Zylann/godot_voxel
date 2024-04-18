#include "voxel_modifier_stack.h"
#include "../edition/funcs.h"
#include "../util/dstack.h"
#include "../util/profiling.h"

namespace zylann::voxel {

namespace {

StdVector<float> &get_tls_sdf() {
	thread_local StdVector<float> tls_sdf;
	return tls_sdf;
}

StdVector<Vector3> &get_tls_positions() {
	thread_local StdVector<Vector3> tls_positions;
	return tls_positions;
}

void get_positions_buffer(Vector3i buffer_size, Vector3 origin, Vector3 size, StdVector<Vector3> &positions) {
	positions.resize(Vector3iUtil::get_volume(buffer_size));

	const Vector3 end = origin + size;
	const Vector3 inv_bsf = Vector3(1.0, 1.0, 1.0) / buffer_size;

	unsigned int i = 0;

	Vector3 pos;

	for (int z = 0; z < buffer_size.z; ++z) {
		pos.z = Math::lerp(origin.z, end.z, z * inv_bsf.z);

		for (int x = 0; x < buffer_size.x; ++x) {
			pos.x = Math::lerp(origin.x, end.x, x * inv_bsf.x);

			for (int y = 0; y < buffer_size.y; ++y) {
				pos.y = Math::lerp(origin.y, end.y, y * inv_bsf.y);

				positions[i] = pos;
				++i;
			}
		}
	}
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

// TODO Use VoxelBuffer helper function
void decompress_sdf_to_buffer(VoxelBuffer &voxels, StdVector<float> &sdf) {
	ZN_DSTACK();

	sdf.resize(Vector3iUtil::get_volume(voxels.get_size()));

	const VoxelBuffer::ChannelId channel = VoxelBuffer::CHANNEL_SDF;
	voxels.decompress_channel(channel);

	const VoxelBuffer::Depth depth = voxels.get_channel_depth(channel);

	switch (depth) {
		case VoxelBuffer::DEPTH_8_BIT: {
			Span<int8_t> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			for (unsigned int i = 0; i < sdf.size(); ++i) {
				sdf[i] = s8_to_snorm(raw[i]);
			}
		} break;

		case VoxelBuffer::DEPTH_16_BIT: {
			Span<int16_t> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			for (unsigned int i = 0; i < sdf.size(); ++i) {
				sdf[i] = s16_to_snorm(raw[i]);
			}
		} break;

		case VoxelBuffer::DEPTH_32_BIT: {
			Span<float> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			memcpy(sdf.data(), raw.data(), sizeof(float) * sdf.size());
		} break;

		case VoxelBuffer::DEPTH_64_BIT: {
			Span<double> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			for (unsigned int i = 0; i < sdf.size(); ++i) {
				sdf[i] = raw[i];
			}
		} break;

		default:
			ZN_CRASH();
	}

	const float inv_scale = 1.0f / VoxelBuffer::get_sdf_quantization_scale(depth);
	for (unsigned int i = 0; i < sdf.size(); ++i) {
		sdf[i] *= inv_scale;
	}
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

void VoxelModifierStack::apply(VoxelBuffer &voxels, AABB aabb) const {
	ZN_PROFILE_SCOPE();
	RWLockRead lock(_stack_lock);

	if (_stack.size() == 0) {
		return;
	}

	VoxelModifierContext ctx;
	bool any_intersection = false;

	// This version can be slower because we are trying to workaround a side-effect of fixed-point compression.
	// Processing through the whole block is easier, but it can introduce artifacts because scaling and applying
	// modifiers can cause tiny changes all over the area when encoded back to snorm/i16/i8, not just inside the shape.
	// Even if modifiers are made to do nothing, the presence of one in a mesh block and not in another block can
	// produce a seam in between them (one will have evaluated SDF with it, and the other without). So we try to
	// modify only the area that intersects the block, and we re-encode only what was modified.
	// Another option later could be to use uncompressed blocks (32-bit float) when doing on-the-fly sampling?

	thread_local StdVector<float> tls_block_sdf_initial;
	thread_local StdVector<float> tls_block_sdf;

	StdVector<float> &area_sdf = get_tls_sdf();
	StdVector<Vector3> &area_positions = get_tls_positions();

	const Vector3 v_to_w = aabb.size / Vector3(voxels.get_size());
	const Vector3 w_to_v = Vector3(voxels.get_size()) / aabb.size;
	const Vector3i origin_voxels = Vector3i(math::floor(aabb.position * w_to_v));

	for (unsigned int i = 0; i < _stack.size(); ++i) {
		const VoxelModifier *modifier = _stack[i];
		ZN_ASSERT(modifier != nullptr);

		const AABB modifier_aabb = modifier->get_aabb();
		if (modifier_aabb.intersects(aabb)) {
			ZN_PROFILE_SCOPE_NAMED("Intersecting modifier");

			if (any_intersection == false) {
				ZN_PROFILE_SCOPE_NAMED("Read block");
				any_intersection = true;

				decompress_sdf_to_buffer(voxels, tls_block_sdf_initial);

				tls_block_sdf.resize(tls_block_sdf_initial.size());
				memcpy(tls_block_sdf.data(), tls_block_sdf_initial.data(), tls_block_sdf.size() * sizeof(float));
			}

			// Get modifier bounds in voxels
			Box3i modifier_box(math::floor(modifier_aabb.position * w_to_v), math::ceil(modifier_aabb.size * w_to_v));
			modifier_box.clip(Box3i(origin_voxels, voxels.get_size()));
			const Vector3i local_origin_in_voxels = modifier_box.position - origin_voxels;

			const int64_t volume = Vector3iUtil::get_volume(modifier_box.size);
			area_sdf.resize(volume);
			copy_3d_region_zxy(to_span(area_sdf), modifier_box.size, Vector3i(), to_span_const(tls_block_sdf),
					voxels.get_size(), local_origin_in_voxels, local_origin_in_voxels + modifier_box.size);

			get_positions_buffer(
					modifier_box.size, v_to_w * modifier_box.position, v_to_w * modifier_box.size, area_positions);

			ctx.positions = to_span(area_positions);
			ctx.sdf = to_span(area_sdf);
			modifier->apply(ctx);

			// Write modifications back to the full-block decompressed buffer
			// TODO Maybe use an unchecked version for a bit more speed?
			copy_3d_region_zxy(to_span(tls_block_sdf), voxels.get_size(), local_origin_in_voxels,
					Span<const float>(ctx.sdf), modifier_box.size, Vector3i(), modifier_box.size);
		}
	}

	if (any_intersection) {
		// scale_and_store_sdf(voxels, to_span(tls_block_sdf));
		scale_and_store_sdf_if_modified(voxels, to_span(tls_block_sdf), to_span(tls_block_sdf_initial));
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

void VoxelModifierStack::apply_for_gpu_rendering(
		StdVector<VoxelModifier::ShaderData> &out_data, AABB aabb, VoxelModifier::ShaderData::Type type) const {
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
			if (sd.shader_rids[type].is_valid()) {
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
