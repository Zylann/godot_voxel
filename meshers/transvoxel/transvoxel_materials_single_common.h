#ifndef VOXEL_TRANSVOXEL_MATERIALS_SINGLE_COMMON_H
#define VOXEL_TRANSVOXEL_MATERIALS_SINGLE_COMMON_H

#include "transvoxel_materials_common.h"

namespace zylann::voxel::transvoxel::materials::single {

struct WeightedIndex {
	uint8_t index;
	uint8_t weight;

	// Higher weight comes first, otherwise lower index comes first
	static inline bool compare_higher_weight(const WeightedIndex &a, const WeightedIndex &b) {
		return a.weight > b.weight || (a.weight == b.weight && a.index < b.index);
	}
};

template <size_t N>
void insert_combine(std::array<WeightedIndex, N> &items, uint32_t &item_count, const uint8_t new_index) {
	for (uint32_t i = 0; i < item_count; ++i) {
		if (items[i].index == new_index) {
			++items[i].weight;
			return;
		}
	}
	items[item_count] = WeightedIndex{ new_index, 1 };
	++item_count;
}

struct VoxelMaterialIndices {
	Span<const uint8_t> indices;
	bool is_uniform = true;
	uint8_t uniform_value = 0;

	Span<const uint8_t> to_span() const {
		if (is_uniform) {
			return Span<const uint8_t>(&uniform_value, 1);
		} else {
			return indices;
		}
	}
};

VoxelMaterialIndices get_material_indices_from_vb(
		const VoxelBuffer &voxels,
		const unsigned int channel,
		StdVector<uint8_t> &conversion_buffer
) {
	ZN_ASSERT_RETURN_V(voxels.get_channel_depth(channel) == VoxelBuffer::DEPTH_8_BIT, VoxelMaterialIndices());

	VoxelMaterialIndices data;

	if (voxels.is_uniform(channel)) {
		data.is_uniform = true;
		data.uniform_value = voxels.get_voxel(Vector3i(), channel);
		return data;
	}

	data.is_uniform = false;

	switch (voxels.get_channel_depth(channel)) {
		case VoxelBuffer::DEPTH_8_BIT: {
			Span<const uint8_t> data_bytes;
			ZN_ASSERT(voxels.get_channel_as_bytes_read_only(channel, data_bytes) == true);
			data.indices = data_bytes;
		} break;

		case VoxelBuffer::DEPTH_16_BIT: {
			static bool s_once = false;
			if (!s_once) {
				s_once = true;
				ZN_PRINT_WARNING(
						"Transvoxel: the Single texturing mode expects 8-bit indices in voxel data, but "
						"it was passed 16-bit. Data will be converted on the fly, with a performance cost."
				);
			}

			Span<const uint16_t> data_u16;
			ZN_ASSERT(voxels.get_channel_data_read_only(channel, data_u16) == true);

			conversion_buffer.resize(data_u16.size());
			for (unsigned int i = 0; i < data_u16.size(); ++i) {
				conversion_buffer[i] = data_u16[i];
			}

			data.indices = to_span(conversion_buffer);
		} break;

		case VoxelBuffer::DEPTH_32_BIT: {
			static bool s_once = false;
			if (!s_once) {
				s_once = true;
				ZN_PRINT_WARNING(
						"Transvoxel: the Single texturing mode expects 8-bit indices in voxel data, but "
						"it was passed 32-bit. This is not supported."
				);
			}
			data.is_uniform = true;
			data.uniform_value = 0;
		} break;

		case VoxelBuffer::DEPTH_64_BIT: {
			static bool s_once = false;
			if (!s_once) {
				s_once = true;
				ZN_PRINT_WARNING(
						"Transvoxel: the Single texturing mode expects 8-bit indices in voxel data, but "
						"it was passed 64-bit. This is not supported."
				);
			}
			data.is_uniform = true;
			data.uniform_value = 0;
		} break;

		default:
			ZN_PRINT_ERROR("Unhandled depth");
			data.is_uniform = true;
			data.uniform_value = 0;
			break;
	}

	return data;
}

} // namespace zylann::voxel::transvoxel::materials::single

#endif // VOXEL_TRANSVOXEL_MATERIALS_SINGLE_COMMON_H
