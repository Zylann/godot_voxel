#ifndef VOXEL_EDITION_FUNCS_H
#define VOXEL_EDITION_FUNCS_H

#include "../storage/funcs.h"
#include "../storage/voxel_data_grid.h"
#include "../util/containers/fixed_array.h"
#include "../util/math/conv.h"
#include "../util/math/sdf.h"
#include "../util/math/transform_3d.h"
#include "../util/profiling.h"

namespace zylann::voxel {

inline void _normalize_weights_preserving(FixedArray<float, 4> &weights, unsigned int preserved_index,
		unsigned int other0, unsigned int other1, unsigned int other2) {
	const float part_sum = weights[other0] + weights[other1] + weights[other2];
	// It is assumed the preserved channel is already clamped to [0, 1]
	const float expected_part_sum = 1.f - weights[preserved_index];

	if (part_sum < 0.0001f) {
		weights[other0] = expected_part_sum / 3.f;
		weights[other1] = expected_part_sum / 3.f;
		weights[other2] = expected_part_sum / 3.f;

	} else {
		const float scale = expected_part_sum / part_sum;
		weights[other0] *= scale;
		weights[other1] *= scale;
		weights[other2] *= scale;
	}
}

inline void normalize_weights_preserving(FixedArray<float, 4> &weights, unsigned int preserved_index) {
	switch (preserved_index) {
		case 0:
			_normalize_weights_preserving(weights, 0, 1, 2, 3);
			break;
		case 1:
			_normalize_weights_preserving(weights, 1, 0, 2, 3);
			break;
		case 2:
			_normalize_weights_preserving(weights, 2, 1, 0, 3);
			break;
		default:
			_normalize_weights_preserving(weights, 3, 1, 2, 0);
			break;
	}
}

/*inline void normalize_weights(FixedArray<float, 4> &weights) {
	float sum = 0;
	for (unsigned int i = 0; i < weights.size(); ++i) {
		sum += weights[i];
	}
	const float sum_inv = 255.f / sum;
	for (unsigned int i = 0; i < weights.size(); ++i) {
		weights[i] = clamp(weights[i] * sum_inv, 0.f, 255.f);
	}
}*/

inline void blend_texture_packed_u16(
		int texture_index, float target_weight, uint16_t &encoded_indices, uint16_t &encoded_weights) {
#ifdef DEBUG_ENABLED
	ZN_ASSERT_RETURN(target_weight >= 0.f && target_weight <= 1.f);
#endif

	FixedArray<uint8_t, 4> indices = decode_indices_from_packed_u16(encoded_indices);
	FixedArray<uint8_t, 4> weights = decode_weights_from_packed_u16(encoded_weights);

	// Search if our texture index is already present
	unsigned int component_index = 4;
	for (unsigned int i = 0; i < indices.size(); ++i) {
		if (indices[i] == texture_index) {
			component_index = i;
			break;
		}
	}

	bool index_was_changed = false;
	if (component_index >= indices.size()) {
		// Our texture index is not present, we'll replace the lowest weight
		uint8_t lowest_weight = 255;
		// Default to 0 in the hypothetic case where all weights are maxed
		component_index = 0;
		for (unsigned int i = 0; i < weights.size(); ++i) {
			if (weights[i] < lowest_weight) {
				lowest_weight = weights[i];
				component_index = i;
			}
		}
		indices[component_index] = texture_index;
		index_was_changed = true;
	}

	// TODO Optimization in case target_weight is 1?

	FixedArray<float, 4> weights_f;
	for (unsigned int i = 0; i < weights.size(); ++i) {
		weights_f[i] = weights[i] / 255.f;
	}

	if (weights_f[component_index] < target_weight || index_was_changed) {
		weights_f[component_index] = target_weight;

		normalize_weights_preserving(weights_f, component_index);

		for (unsigned int i = 0; i < weights_f.size(); ++i) {
			weights[i] = math::clamp(weights_f[i] * 255.f, 0.f, 255.f);
		}

		encoded_indices = encode_indices_to_packed_u16(indices[0], indices[1], indices[2], indices[3]);
		encoded_weights = encode_weights_to_packed_u16_lossy(weights[0], weights[1], weights[2], weights[3]);
	}
}

// Interpolates values from a 3D grid at a given position, using trilinear interpolation.
// If the position is outside the grid, values are clamped.
inline float interpolate_trilinear(Span<const float> grid, const Vector3i res, const Vector3f pos) {
	const Vector3f pfi = math::floor(pos - Vector3f(0.5f));
	// TODO Clamp pf too somehow?
	const Vector3f pf = pos - pfi;
	const Vector3i max_pos = math::max(res - Vector3i(2, 2, 2), Vector3i());
	const Vector3i pi = math::clamp(Vector3i(pfi.x, pfi.y, pfi.z), Vector3i(), max_pos);

	const unsigned int n010 = 1;
	const unsigned int n100 = res.y;
	const unsigned int n001 = res.y * res.x;

	const unsigned int i000 = pi.x * n100 + pi.y * n010 + pi.z * n001;
	const unsigned int i010 = i000 + n010;
	const unsigned int i100 = i000 + n100;
	const unsigned int i001 = i000 + n001;
	const unsigned int i110 = i010 + n100;
	const unsigned int i011 = i010 + n001;
	const unsigned int i101 = i100 + n001;
	const unsigned int i111 = i110 + n001;

	return math::interpolate_trilinear(
			grid[i000], grid[i100], grid[i101], grid[i001], grid[i010], grid[i110], grid[i111], grid[i011], pf);
}

template <typename Volume_F>
float get_sdf_interpolated(const Volume_F &f, Vector3 pos) {
	const Vector3i c = math::floor_to_int(pos);

	const float s000 = f(Vector3i(c.x, c.y, c.z));
	const float s100 = f(Vector3i(c.x + 1, c.y, c.z));
	const float s010 = f(Vector3i(c.x, c.y + 1, c.z));
	const float s110 = f(Vector3i(c.x + 1, c.y + 1, c.z));
	const float s001 = f(Vector3i(c.x, c.y, c.z + 1));
	const float s101 = f(Vector3i(c.x + 1, c.y, c.z + 1));
	const float s011 = f(Vector3i(c.x, c.y + 1, c.z + 1));
	const float s111 = f(Vector3i(c.x + 1, c.y + 1, c.z + 1));

	return math::interpolate_trilinear(s000, s100, s101, s001, s010, s110, s111, s011, to_vec3f(math::fract(pos)));
}

// Standalone helper function to copy voxels from any 3D chunked container
void copy_from_chunked_storage(VoxelBufferInternal &dst_buffer, Vector3i min_pos, unsigned int block_size_po2,
		uint32_t channels_mask, const VoxelBufferInternal *(*get_block_func)(void *, Vector3i),
		void *get_block_func_ctx);

// Standalone helper function to paste voxels to any 3D chunked container
void paste_to_chunked_storage(const VoxelBufferInternal &src_buffer, Vector3i min_pos, unsigned int block_size_po2,
		unsigned int channels_mask, bool use_mask, uint8_t mask_channel, uint64_t mask_value,
		VoxelBufferInternal *(*get_block_func)(void *, Vector3i), void *get_block_func_ctx);

AABB get_path_aabb(Span<const Vector3> positions, Span<const float> radii);

} // namespace zylann::voxel

// Library of templates for executing per-voxel operations.
// There is a bunch of compile-time abstraction boilerplate, which is to minimize the code to write when adding new
// operations, and have them work with different chunked containers, different edition modes, different formats... while
// also trying to avoid runtime per-voxel branching and checks for all these cases.
namespace zylann::voxel::ops {

// Operations

template <typename Op, typename Shape>
struct SdfOperation16bit {
	Op op;
	Shape shape;
	inline int16_t operator()(Vector3i pos, int16_t sdf) const {
		return snorm_to_s16(op(s16_to_snorm(sdf), shape(Vector3(pos))));
	}
};

struct SdfUnion {
	real_t strength;
	inline real_t operator()(real_t a, real_t b) const {
		return Math::lerp(a, math::sdf_union(a, b), strength);
	}
};

struct SdfSubtract {
	real_t strength;
	inline real_t operator()(real_t a, real_t b) const {
		return Math::lerp(a, math::sdf_subtract(a, b), strength);
	}
};

struct SdfSet {
	real_t strength;
	inline real_t operator()(real_t a, real_t b) const {
		return Math::lerp(a, b, strength);
	}
};

template <typename Value_T, typename Shape_T>
struct BlockySetOperation {
	Shape_T shape;
	Value_T value;
	inline Value_T operator()(Vector3i pos, Value_T src) const {
		return shape.is_inside(Vector3(pos)) ? value : src;
	}
};

inline Box3i get_sdf_sphere_box(Vector3 center, real_t radius) {
	return Box3i::from_min_max( //
			math::floor_to_int(center - Vector3(radius, radius, radius)),
			math::ceil_to_int(center + Vector3(radius, radius, radius)))
			// That padding is for SDF to have some margin
			// TODO Don't add padding from here, it must be done at higher level, where we know the type of operation
			.padded(2);
}

// Shapes

struct SdfSphere {
	Vector3 center;
	real_t radius;
	real_t sdf_scale;

	// TODO Rename get_signed_distance?
	inline real_t operator()(Vector3 pos) const {
		return sdf_scale * math::sdf_sphere(pos, center, radius);
	}

	inline bool is_inside(Vector3 pos) const {
		// Faster than the true SDF, we avoid a square root
		return center.distance_squared_to(pos) < radius * radius;
	}

	// TODO Seems unused?
	inline const char *name() const {
		return "SdfSphere";
	}

	inline Box3i get_box() {
		return get_sdf_sphere_box(center, radius);
	}
};

struct SdfHemisphere {
	Vector3 center;
	Vector3 flat_direction;
	real_t plane_d;
	real_t radius;
	real_t sdf_scale;
	real_t smoothness;

	inline real_t operator()(Vector3 pos) const {
		return sdf_scale *
				math::sdf_smooth_subtract( //
						math::sdf_sphere(pos, center, radius), //
						math::sdf_plane(pos, flat_direction, plane_d), smoothness);
	}

	inline bool is_inside(Vector3 pos) const {
		return (*this)(pos) < 0;
	}

	inline const char *name() const {
		return "SdfHemisphere";
	}

	inline Box3i get_box() {
		return get_sdf_sphere_box(center, radius);
	}
};

struct SdfBufferShape {
	Span<const float> buffer;
	Vector3i buffer_size;
	Transform3D world_to_buffer;
	float isolevel;
	float sdf_scale;

	inline real_t operator()(const Vector3 &wpos) const {
		// Transform terrain-space position to buffer-space
		const Vector3f lpos = to_vec3f(world_to_buffer.xform(wpos));
		if (lpos.x < 0 || lpos.y < 0 || lpos.z < 0 || lpos.x >= buffer_size.x || lpos.y >= buffer_size.y ||
				lpos.z >= buffer_size.z) {
			// Outside the buffer
			return 100;
		}
		// TODO Trilinear looks bad when the shape is scaled up.
		// Use Hermite in 3D https://www.researchgate.net/publication/360206102_Hermite_interpolation_of_heightmaps
		return interpolate_trilinear(buffer, buffer_size, lpos) * sdf_scale - isolevel;
	}

	inline const char *name() const {
		return "SdfBufferShape";
	}
};

Box3i get_round_cone_int_bounds(Vector3 p0, Vector3 p1, float r0, float r1);

struct SdfRoundCone {
	math::SdfRoundConePrecalc cone;
	real_t sdf_scale;

	inline real_t operator()(Vector3 pos) const {
		return sdf_scale * cone(pos);
	}

	inline bool is_inside(Vector3 pos) const {
		return cone(pos) < 0.f;
	}

	Box3i get_box() const {
		return get_round_cone_int_bounds(cone.a, cone.b, cone.r1, cone.r2);
	}
};

struct TextureParams {
	float opacity = 1.f;
	float sharpness = 2.f;
	unsigned int index = 0;
};

// Optimized for spheres
struct TextureBlendSphereOp {
	Vector3 center;
	float radius;
	float radius_squared;
	TextureParams tp;

	TextureBlendSphereOp(Vector3 p_center, float p_radius, TextureParams p_tp) {
		center = p_center;
		radius = p_radius;
		radius_squared = p_radius * p_radius;
		tp = p_tp;
	}

	inline void operator()(Vector3i pos, uint16_t &indices, uint16_t &weights) const {
		const float distance_squared = Vector3(pos).distance_squared_to(center);
		// Avoiding square root on the hot path
		if (distance_squared < radius_squared) {
			const float distance_from_radius = radius - Math::sqrt(distance_squared);
			const float target_weight =
					tp.opacity * math::clamp(tp.sharpness * (distance_from_radius / radius), 0.f, 1.f);
			blend_texture_packed_u16(tp.index, target_weight, indices, weights);
		}
	}
};

template <typename Shape_T>
struct TextureBlendOp {
	Shape_T shape;
	TextureParams texture_params;

	inline void operator()(Vector3i pos, uint16_t &indices, uint16_t &weights) const {
		const float sd = shape(pos);
		if (sd <= 0) {
			// TODO We don't know the full size of the shape so sharpness may be adjusted
			const float target_weight = texture_params.opacity * math::clamp(-sd * texture_params.sharpness, 0.f, 1.f);
			blend_texture_packed_u16(texture_params.index, target_weight, indices, weights);
		}
	}
};

enum Mode { //
	MODE_ADD,
	MODE_REMOVE,
	MODE_SET,
	MODE_TEXTURE_PAINT
};

// This one is implemented manually for a fast-path in texture paint
// TODO Find a nicer way to do this without copypasta
struct DoSphere {
	SdfSphere shape;
	Mode mode;
	VoxelDataGrid blocks;
	Box3i box;
	TextureParams texture_params;
	VoxelBufferInternal::ChannelId channel;
	uint32_t blocky_value;
	float strength;

	void operator()() {
		ZN_PROFILE_SCOPE();

		if (channel == VoxelBufferInternal::CHANNEL_SDF) {
			switch (mode) {
				case MODE_ADD: {
					// TODO Support other depths, format should be accessible from the volume
					SdfOperation16bit<SdfUnion, SdfSphere> op;
					op.shape = shape;
					op.op.strength = strength;
					blocks.write_box(box, VoxelBufferInternal::CHANNEL_SDF, op);
				} break;

				case MODE_REMOVE: {
					SdfOperation16bit<SdfSubtract, SdfSphere> op;
					op.shape = shape;
					op.op.strength = strength;
					blocks.write_box(box, VoxelBufferInternal::CHANNEL_SDF, op);
				} break;

				case MODE_SET: {
					SdfOperation16bit<SdfSet, SdfSphere> op;
					op.shape = shape;
					op.op.strength = strength;
					blocks.write_box(box, VoxelBufferInternal::CHANNEL_SDF, op);
				} break;

				case MODE_TEXTURE_PAINT: {
					blocks.write_box_2(box, VoxelBufferInternal::CHANNEL_INDICES, VoxelBufferInternal::CHANNEL_WEIGHTS,
							TextureBlendSphereOp(shape.center, shape.radius, texture_params));
				} break;

				default:
					ERR_PRINT("Unknown mode");
					break;
			}
		} else {
			BlockySetOperation<uint32_t, SdfSphere> op;
			op.shape = shape;
			op.value = blocky_value;
			blocks.write_box(box, channel, op);
		}
	}
};

template <typename TBlockAccess, typename FBlockAction>
void process_chunked_storage(Box3i voxel_box,
		// VoxelBufferInternal *get_block(Vector3i bpos)
		// unsigned int get_block_size_po2()
		TBlockAccess block_access,
		// void(VoxelBufferInternal &vb, Box3i local_box, Vector3i origin)
		FBlockAction op_func) {
	//
	const Vector3i max_pos = voxel_box.pos + voxel_box.size;

	const unsigned int block_size_po2 = block_access.get_block_size_po2();

	const Vector3i min_block_pos = voxel_box.pos >> block_size_po2;
	const Vector3i max_block_pos = ((max_pos - Vector3i(1, 1, 1)) >> block_size_po2) + Vector3i(1, 1, 1);

	Vector3i bpos;
	for (bpos.z = min_block_pos.z; bpos.z < max_block_pos.z; ++bpos.z) {
		for (bpos.x = min_block_pos.x; bpos.x < max_block_pos.x; ++bpos.x) {
			for (bpos.y = min_block_pos.y; bpos.y < max_block_pos.y; ++bpos.y) {
				VoxelBufferInternal *block = block_access.get_block(bpos);
				if (block == nullptr) {
					continue;
				}
				const Vector3i block_origin = bpos << block_size_po2;
				const Box3i local_box = Box3i(voxel_box.pos - block_origin, voxel_box.size)
												.clipped(Box3i(Vector3i(), Vector3iUtil::create(1 << block_size_po2)));

				op_func(*block, local_box, block_origin);
			}
		}
	}
}

template <typename TBlockAccess, typename FOp>
inline void write_box_in_chunked_storage_1_channel(
		// D process(D src, Vector3i position)
		const FOp &op, TBlockAccess &block_access, Box3i box, VoxelBufferInternal::ChannelId channel_id) {
	//
	process_chunked_storage(
			box, block_access, [&op, channel_id](VoxelBufferInternal &vb, const Box3i local_box, Vector3i origin) {
				vb.write_box(local_box, channel_id, op, origin);
			});
}

template <typename TBlockAccess, typename FOp>
inline void write_box_in_chunked_storage_2_channels(
		// D process(D src, Vector3i position)
		const FOp &op, TBlockAccess &block_access, Box3i box, VoxelBufferInternal::ChannelId channel0_id,
		VoxelBufferInternal::ChannelId channel1_id) {
	//
	process_chunked_storage(box, block_access,
			[&op, channel0_id, channel1_id](VoxelBufferInternal &vb, const Box3i local_box, Vector3i origin) {
				vb.write_box_2_template<FOp, uint16_t, uint16_t>(local_box, channel0_id, channel1_id, op, origin);
			});
}

struct VoxelDataGridAccess {
	VoxelDataGrid *grid = nullptr;
	inline VoxelBufferInternal *get_block(const Vector3i &bpos) {
		return grid->get_block_no_lock(bpos);
	}
	inline unsigned int get_block_size_po2() const {
		return grid->get_block_size_po2();
	}
};

// Executes an operation in a box of a chunked voxel storage.
template <typename TShape, typename TBlockAccess>
struct DoShapeChunked {
	TShape shape;
	Mode mode;
	// VoxelBufferInternal *get_block(Vector3i bpos)
	// unsigned int get_block_size_po2()
	TBlockAccess block_access;
	Box3i box;
	VoxelBufferInternal::ChannelId channel;
	TextureParams texture_params;
	uint32_t blocky_value;
	float strength;

	void operator()() {
		ZN_PROFILE_SCOPE();

		if (channel == VoxelBufferInternal::CHANNEL_SDF) {
			switch (mode) {
				case MODE_ADD: {
					// TODO Support other depths, format should be accessible from the volume. Or separate encoding?
					SdfOperation16bit<SdfUnion, TShape> op;
					op.shape = shape;
					op.op.strength = strength;
					write_box_in_chunked_storage_1_channel(op, block_access, box, VoxelBufferInternal::CHANNEL_SDF);
				} break;

				case MODE_REMOVE: {
					SdfOperation16bit<SdfSubtract, TShape> op;
					op.shape = shape;
					op.op.strength = strength;
					write_box_in_chunked_storage_1_channel(op, block_access, box, VoxelBufferInternal::CHANNEL_SDF);
				} break;

				case MODE_SET: {
					SdfOperation16bit<SdfSet, TShape> op;
					op.shape = shape;
					op.op.strength = strength;
					write_box_in_chunked_storage_1_channel(op, block_access, box, VoxelBufferInternal::CHANNEL_SDF);
				} break;

				case MODE_TEXTURE_PAINT: {
					TextureBlendOp<TShape> op;
					op.shape = shape;
					op.texture_params = texture_params;
					write_box_in_chunked_storage_2_channels(op, block_access, box, VoxelBufferInternal::CHANNEL_INDICES,
							VoxelBufferInternal::CHANNEL_WEIGHTS);
				} break;

				default:
					ERR_PRINT("Unknown mode");
					break;
			}

		} else {
			BlockySetOperation<uint32_t, TShape> op;
			op.shape = shape;
			op.value = blocky_value;
			write_box_in_chunked_storage_1_channel(op, block_access, box, channel);
		}
	}
};

#ifdef DEBUG_ENABLED
void box_blur_slow_ref(
		const VoxelBufferInternal &src, VoxelBufferInternal &dst, int radius, Vector3f sphere_pos, float sphere_radius);
#endif

void box_blur(
		const VoxelBufferInternal &src, VoxelBufferInternal &dst, int radius, Vector3f sphere_pos, float sphere_radius);

}; // namespace zylann::voxel::ops

#endif // VOXEL_EDITION_FUNCS_H
