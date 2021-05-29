#ifndef EDITION_FUNCS_H
#define EDITION_FUNCS_H

#include "../storage/funcs.h"
#include "../util/fixed_array.h"

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

inline void blend_texture_packed_u16(int texture_index, float target_weight,
		uint16_t &encoded_indices, uint16_t &encoded_weights) {
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(target_weight < 0.f || target_weight > 1.f);
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
			weights[i] = clamp(weights_f[i] * 255.f, 0.f, 255.f);
		}

		encoded_indices = encode_indices_to_packed_u16(indices[0], indices[1], indices[2], indices[3]);
		encoded_weights = encode_weights_to_packed_u16(weights[0], weights[1], weights[2], weights[3]);
	}
}

#endif // EDITION_FUNCS_H
