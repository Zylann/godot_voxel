#ifndef VOXEL_TRANSVOXEL_MATERIALS_MIXEL4_H
#define VOXEL_TRANSVOXEL_MATERIALS_MIXEL4_H

#include "../../storage/materials_4i4w.h"
#include "../../util/containers/fixed_array.h"
#include "../../util/containers/std_vector.h"
#include "transvoxel.h"

namespace zylann::voxel::transvoxel {

inline uint32_t pack_bytes(const FixedArray<uint8_t, 4> &a) {
	return (a[0] | (a[1] << 8) | (a[2] << 16) | (a[3] << 24));
}

void add_texture_data(
		StdVector<Vector2f> &uv,
		unsigned int packed_indices,
		FixedArray<uint8_t, MAX_TEXTURE_BLENDS> weights
) {
	struct IntUV {
		uint32_t x;
		uint32_t y;
	};
	static_assert(sizeof(IntUV) == sizeof(Vector2f), "Expected same binary size");
	uv.push_back(Vector2f());
	IntUV &iuv = *(reinterpret_cast<IntUV *>(&uv.back()));
	// print_line(String("{0}, {1}, {2}, {3}").format(varray(weights[0], weights[1], weights[2], weights[3])));
	iuv.x = packed_indices;
	iuv.y = pack_bytes(weights);
}

template <unsigned int NVoxels>
struct CellTextureDatas {
	uint32_t packed_indices = 0;
	FixedArray<uint8_t, MAX_TEXTURE_BLENDS> indices;
	FixedArray<FixedArray<uint8_t, MAX_TEXTURE_BLENDS>, NVoxels> weights;
};

template <unsigned int NVoxels, typename WeightSampler_T>
CellTextureDatas<NVoxels> select_textures_4_per_voxel(
		const FixedArray<unsigned int, NVoxels> &voxel_indices,
		const Span<const uint16_t> indices_data,
		const WeightSampler_T &weights_sampler,
		const unsigned int case_code
) {
	// TODO Optimization: this function takes almost half of the time when polygonizing non-empty cells.
	// I wonder how it can be optimized further?

	struct IndexAndWeight {
		unsigned int index;
		unsigned int weight;
	};

	FixedArray<FixedArray<uint8_t, MAX_TEXTURES>, NVoxels> cell_texture_weights_temp;
	FixedArray<IndexAndWeight, MAX_TEXTURES> indexed_weight_sums;

	// Find 4 most-used indices in voxels
	for (unsigned int i = 0; i < indexed_weight_sums.size(); ++i) {
		indexed_weight_sums[i] = IndexAndWeight{ i, 0 };
	}
	for (unsigned int ci = 0; ci < voxel_indices.size(); ++ci) {
		// ZN_PROFILE_SCOPE();

		FixedArray<uint8_t, MAX_TEXTURES> &weights_temp = cell_texture_weights_temp[ci];
		fill(weights_temp, uint8_t(0));

		// Air voxels should not contribute
		if ((case_code & (1 << ci)) != 0) {
			continue;
		}

		const unsigned int data_index = voxel_indices[ci];

		const FixedArray<uint8_t, 4> indices = decode_indices_from_packed_u16(indices_data[data_index]);
		const FixedArray<uint8_t, 4> weights = weights_sampler.get_weights(data_index);

		for (unsigned int j = 0; j < indices.size(); ++j) {
			const unsigned int ti = indices[j];
			indexed_weight_sums[ti].weight += weights[j];
			weights_temp[ti] = weights[j];
		}
	}
	struct IndexAndWeightComparator {
		inline bool operator()(const IndexAndWeight &a, const IndexAndWeight &b) const {
			return a.weight > b.weight;
		}
	};
	SortArray<IndexAndWeight, IndexAndWeightComparator> sorter;
	sorter.sort(indexed_weight_sums.data(), indexed_weight_sums.size());

	CellTextureDatas<NVoxels> cell_textures;

	// Assign indices
	for (unsigned int i = 0; i < cell_textures.indices.size(); ++i) {
		cell_textures.indices[i] = indexed_weight_sums[i].index;
	}

	// Sort indices to avoid cases that are ambiguous for blending, like 1,2,3,4 and 2,1,3,4
	// TODO maybe we could require this sorting to be done up front?
	// Or maybe could be done after meshing so we do it less times?
	math::sort(cell_textures.indices[0], cell_textures.indices[1], cell_textures.indices[2], cell_textures.indices[3]);

	cell_textures.packed_indices = pack_bytes(cell_textures.indices);

	// Remap weights to follow the indices we selected
	for (unsigned int ci = 0; ci < cell_texture_weights_temp.size(); ++ci) {
		// ZN_PROFILE_SCOPE();

		FixedArray<uint8_t, 4> &dst_weights = cell_textures.weights[ci];

		// Skip air voxels
		if ((case_code & (1 << ci)) != 0) {
			fill(dst_weights, uint8_t(0));
			continue;
		}

		const FixedArray<uint8_t, MAX_TEXTURES> &src_weights = cell_texture_weights_temp[ci];

		for (unsigned int i = 0; i < cell_textures.indices.size(); ++i) {
			const unsigned int ti = cell_textures.indices[i];
			dst_weights[i] = src_weights[ti];
		}
	}

	return cell_textures;
}

struct TextureIndicesData {
	// Texture indices for each voxel
	Span<const uint16_t> buffer;
	// Used if the buffer is empty
	FixedArray<uint8_t, 4> default_indices;
	uint32_t packed_default_indices;
};

template <unsigned int NVoxels, typename WeightSampler_T>
inline void get_cell_texture_data(
		CellTextureDatas<NVoxels> &cell_textures,
		const TextureIndicesData &texture_indices_data,
		const FixedArray<unsigned int, NVoxels> &voxel_indices,
		const WeightSampler_T &weights_data,
		// Used for rejecting air voxels. Can be set to 0 so all corners are always used.
		const unsigned int case_code
) {
	if (texture_indices_data.buffer.size() == 0) {
		// Indices are known for the whole block, just read weights directly
		cell_textures.indices = texture_indices_data.default_indices;
		cell_textures.packed_indices = texture_indices_data.packed_default_indices;
		for (unsigned int ci = 0; ci < voxel_indices.size(); ++ci) {
			if ((case_code & (1 << ci)) != 0) {
				// Force air voxels to not contribute
				// TODO This is not great, because every Transvoxel vertex interpolates between a matter and air corner.
				// This approach means we would always interpolate towards 0 as a result.
				// Maybe we'll have to use a different approach and remove this option in the future.
				fill(cell_textures.weights[ci], uint8_t(0));
			} else {
				const unsigned int wi = voxel_indices[ci];
				cell_textures.weights[ci] = weights_data.get_weights(wi);
			}
		}

	} else {
		// There can be more than 4 indices or they are not known, so we have to select them
		cell_textures =
				select_textures_4_per_voxel(voxel_indices, texture_indices_data.buffer, weights_data, case_code);
	}
}

struct WeightSamplerPackedU16 {
	Span<const uint16_t> u16_data;
	inline FixedArray<uint8_t, 4> get_weights(unsigned int i) const {
		return decode_weights_from_packed_u16(u16_data[i]);
	}
};

inline uint16_t reorder_transition_case_code(const uint16_t case_code) {
	// Reorders the case code from a transition cell so bits corresponds to an XYZ iteration order through cell corners.
	//
	// The order of cell corners chosen in transition cells are dependent on how the Transvoxel tables are laid out (see
	// figures 4.16 and 4.17 of the paper), which unfortunately is different from the order in which we sample voxels.
	// That prevents from re-using it in texture selection. The reason for that choice seems to stem from convenience
	// when creating the lookup tables.
	//                       |436785210|
	const uint16_t alt_case_code = 0 //
			| ((case_code & 0b000000111)) // 210
			| ((case_code & 0b110000000) >> 4) // 43
			| ((case_code & 0b000001000) << 2) // 5
			| ((case_code & 0b001000000)) // 6
			| ((case_code & 0b000100000) << 2) // 7
			| ((case_code & 0b000010000) << 4) // 8
			;
	// uint16_t alt_case_code = sign_f(cell_samples[0]);
	// alt_case_code |= (sign_f(cell_samples[1]) << 1);
	// alt_case_code |= (sign_f(cell_samples[2]) << 2);
	// alt_case_code |= (sign_f(cell_samples[3]) << 3);
	// alt_case_code |= (sign_f(cell_samples[4]) << 4);
	// alt_case_code |= (sign_f(cell_samples[5]) << 5);
	// alt_case_code |= (sign_f(cell_samples[6]) << 6);
	// alt_case_code |= (sign_f(cell_samples[7]) << 7);
	// alt_case_code |= (sign_f(cell_samples[8]) << 8);

	return alt_case_code;
}

template <unsigned int NVoxels>
struct MaterialProcessorMixel4 {
	const TextureIndicesData voxel_material_indices;
	const WeightSamplerPackedU16 voxel_material_weights;
	const bool textures_skip_air_voxels;
	CellTextureDatas<NVoxels> cell_textures;
	StdVector<Vector2f> &output_mesh_material_data;

	MaterialProcessorMixel4(
			const TextureIndicesData p_voxel_material_indices,
			const WeightSamplerPackedU16 p_voxel_material_weights,
			StdVector<Vector2f> &p_output_mesh_material_data,
			const bool p_textures_skip_air_voxels
	) :
			voxel_material_indices(p_voxel_material_indices),
			voxel_material_weights(p_voxel_material_weights),
			textures_skip_air_voxels(p_textures_skip_air_voxels),
			output_mesh_material_data(p_output_mesh_material_data) {}

	inline uint32_t on_cell(const FixedArray<uint32_t, NVoxels> &corner_voxel_indices, const uint8_t case_code) {
		get_cell_texture_data(
				cell_textures,
				voxel_material_indices,
				corner_voxel_indices,
				voxel_material_weights,
				textures_skip_air_voxels ? case_code : 0
		);

		return cell_textures.packed_indices;
	}

	inline uint32_t on_transition_cell(const FixedArray<uint32_t, 9> &corner_voxel_indices, const uint8_t case_code) {
		const uint16_t alt_case_code = textures_skip_air_voxels ? reorder_transition_case_code(case_code) : 0;

		// Get values from 9 significant corners
		CellTextureDatas<9> cell_textures_partial;
		get_cell_texture_data(
				cell_textures_partial,
				voxel_material_indices,
				corner_voxel_indices,
				voxel_material_weights,
				alt_case_code
		);

		// Fill in slots that are just repeating others

		cell_textures.indices = cell_textures_partial.indices;
		cell_textures.packed_indices = cell_textures_partial.packed_indices;

		for (unsigned int i = 0; i < cell_textures_partial.weights.size(); ++i) {
			cell_textures.weights[i] = cell_textures_partial.weights[i];
		}

		cell_textures.weights[0x9] = cell_textures_partial.weights[0];
		cell_textures.weights[0xA] = cell_textures_partial.weights[2];
		cell_textures.weights[0xB] = cell_textures_partial.weights[6];
		cell_textures.weights[0xC] = cell_textures_partial.weights[8];

		return cell_textures.packed_indices;
	}

	inline void on_vertex(const unsigned int v0, const unsigned int v1, const float alpha) {
		const FixedArray<uint8_t, MAX_TEXTURE_BLENDS> weights0 = cell_textures.weights[v0];
		const FixedArray<uint8_t, MAX_TEXTURE_BLENDS> weights1 = cell_textures.weights[v1];
		FixedArray<uint8_t, MAX_TEXTURE_BLENDS> weights;
		for (unsigned int i = 0; i < MAX_TEXTURE_BLENDS; ++i) {
			weights[i] = static_cast<uint8_t>(math::clamp(Math::lerp(weights0[i], weights1[i], alpha), 0.f, 255.f));
		}
		add_texture_data(output_mesh_material_data, cell_textures.packed_indices, weights);
	}
};

TextureIndicesData get_texture_indices_data(
		const VoxelBuffer &voxels,
		unsigned int channel,
		DefaultTextureIndicesData &out_default_texture_indices_data
) {
	ZN_ASSERT_RETURN_V(voxels.get_channel_depth(channel) == VoxelBuffer::DEPTH_16_BIT, TextureIndicesData());

	TextureIndicesData data;

	if (voxels.is_uniform(channel)) {
		const uint16_t encoded_indices = voxels.get_voxel(Vector3i(), channel);
		data.default_indices = decode_indices_from_packed_u16(encoded_indices);
		data.packed_default_indices = pack_bytes(data.default_indices);

		out_default_texture_indices_data.indices = data.default_indices;
		out_default_texture_indices_data.packed_indices = data.packed_default_indices;
		out_default_texture_indices_data.use = true;

	} else {
		Span<const uint8_t> data_bytes;
		ZN_ASSERT(voxels.get_channel_as_bytes_read_only(channel, data_bytes) == true);
		data.buffer = data_bytes.reinterpret_cast_to<const uint16_t>();

		out_default_texture_indices_data.use = false;
	}

	return data;
}

} // namespace zylann::voxel::transvoxel

#endif // VOXEL_TRANSVOXEL_MATERIALS_MIXEL4_H
