#ifndef VOXEL_MATERIAL_FUNCS_4I4W_H
#define VOXEL_MATERIAL_FUNCS_4I4W_H

#include "../util/containers/fixed_array.h"
#include "../util/math/funcs.h"
#include <cstdint>

// Functions to encode, decode and blend voxel materials using 4 indices and 4 weights.

namespace zylann::voxel {

class VoxelBuffer;

inline FixedArray<uint8_t, 4> decode_weights_from_packed_u16(uint16_t packed_weights) {
	FixedArray<uint8_t, 4> weights;
	// SIMDable?
	// weights[0] = ((packed_weights >> 0) & 0x0f) << 4;
	// weights[1] = ((packed_weights >> 4) & 0x0f) << 4;
	// weights[2] = ((packed_weights >> 8) & 0x0f) << 4;
	// weights[3] = ((packed_weights >> 12) & 0x0f) << 4;

	// Reduced but not SIMDable
	weights[0] = (packed_weights & 0x0f) << 4;
	weights[1] = packed_weights & 0xf0;
	weights[2] = (packed_weights >> 4) & 0xf0;
	weights[3] = (packed_weights >> 8) & 0xf0;
	// The code above is such that the maximum uint8_t value for a weight is 240, not 255.
	// We could add extra computations in order to match the range exactly,
	// but as a compromise I'm not doing them because it would kinda break bijectivity and is slower.
	// If this is a problem, then it could be an argument to switch to 8bit representation using 3 channels.
	// weights[0] |= weights[0] >> 4;
	// weights[1] |= weights[1] >> 4;
	// weights[2] |= weights[2] >> 4;
	// weights[3] |= weights[3] >> 4;
	return weights;
}

inline FixedArray<uint8_t, 4> decode_indices_from_packed_u16(uint16_t packed_indices) {
	FixedArray<uint8_t, 4> indices;
	// SIMDable?
	indices[0] = (packed_indices >> 0) & 0x0f;
	indices[1] = (packed_indices >> 4) & 0x0f;
	indices[2] = (packed_indices >> 8) & 0x0f;
	indices[3] = (packed_indices >> 12) & 0x0f;
	return indices;
}

inline constexpr uint16_t encode_indices_to_packed_u16(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
	return (a & 0xf) | ((b & 0xf) << 4) | ((c & 0xf) << 8) | ((d & 0xf) << 12);
}

// Encodes from 0..255 to 0..15 values packed in 16 bits. Lower 4 bits of input values will not be preserved.
inline constexpr uint16_t encode_weights_to_packed_u16_lossy(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
	return (a >> 4) | ((b >> 4) << 4) | ((c >> 4) << 8) | ((d >> 4) << 12);
}

// Checks if there are no duplicate indices in any voxel
inline void debug_check_texture_indices(FixedArray<uint8_t, 4> indices) {
	FixedArray<bool, 16> checked;
	fill(checked, false);
	for (unsigned int i = 0; i < indices.size(); ++i) {
		unsigned int ti = indices[i];
		ZN_ASSERT(!checked[ti]);
		checked[ti] = true;
	}
}

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

void debug_check_texture_indices_packed_u16(const VoxelBuffer &voxels);

} // namespace zylann::voxel

#endif // VOXEL_MATERIAL_FUNCS_4I4W_H
