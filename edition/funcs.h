#ifndef VOXEL_EDITION_FUNCS_H
#define VOXEL_EDITION_FUNCS_H

#include "../storage/funcs.h"
#include "../util/fixed_array.h"
#include "../util/math/conv.h"
#include "../util/math/sdf.h"

#include <core/math/transform_3d.h>

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
		encoded_weights = encode_weights_to_packed_u16(weights[0], weights[1], weights[2], weights[3]);
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

} // namespace zylann::voxel

namespace zylann::voxel::ops {

template <typename Op, typename Shape>
struct SdfOperation16bit {
	Op op;
	Shape shape;
	inline int16_t operator()(Vector3i pos, int16_t sdf) const {
		return snorm_to_s16(op(s16_to_snorm(sdf), shape(Vector3(pos))));
	}
};

struct SdfUnion {
	inline real_t operator()(real_t a, real_t b) const {
		return zylann::math::sdf_union(a, b);
	}
};

struct SdfSubtract {
	inline real_t operator()(real_t a, real_t b) const {
		return zylann::math::sdf_subtract(a, b);
	}
};

struct SdfSet {
	inline real_t operator()(real_t a, real_t b) const {
		return b;
	}
};

struct SdfSphere {
	Vector3 center;
	real_t radius;
	real_t scale;

	inline real_t operator()(Vector3 pos) const {
		return scale * zylann::math::sdf_sphere(pos, center, radius);
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
		return interpolate_trilinear(buffer, buffer_size, lpos) * sdf_scale - isolevel;
	}
};

struct TextureParams {
	float opacity = 1.f;
	float sharpness = 2.f;
	unsigned int index = 0;
};

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
		if (distance_squared < radius_squared) {
			const float distance_from_radius = radius - Math::sqrt(distance_squared);
			const float target_weight =
					tp.opacity * math::clamp(tp.sharpness * (distance_from_radius / radius), 0.f, 1.f);
			blend_texture_packed_u16(tp.index, target_weight, indices, weights);
		}
	}
};

}; // namespace zylann::voxel::ops

#endif // VOXEL_EDITION_FUNCS_H
