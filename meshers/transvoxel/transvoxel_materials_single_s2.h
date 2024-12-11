#ifndef VOXEL_TRANSVOXEL_MATERIALS_SINGLE_S2_H
#define VOXEL_TRANSVOXEL_MATERIALS_SINGLE_S2_H

#include "../../util/math/funcs.h"
#include "transvoxel.h"
#include "transvoxel_materials_single_common.h"
#include <array>

namespace zylann::voxel::transvoxel::materials::single::s2 {

// One 8-bit material per voxel. Up to 4 blending in shader.

template <unsigned int NVoxels>
struct CellMaterials {
	// Selected indices of the 4 most-represented materials that will blend within the cell
	std::array<uint8_t, 2> selected_indices;
	uint16_t packed_indices = 0;
	// Index of one of the 2 selected materials at each voxel of the cell
	std::array<uint8_t, NVoxels> component_indices;
};

inline uint16_t pack_bytes(std::array<uint8_t, 2> a) {
	return //
			(static_cast<uint16_t>(a[0]) << 0) | //
			(static_cast<uint16_t>(a[1]) << 8);
}

// Inserts new item into a fixed collection of sorted items. If it doesn't fit, it is not inserted. If it fits, the last
// item will get evicted to make room.
void insert_sort(std::array<WeightedIndex, 2> &sorted_items, const WeightedIndex new_item) {
	if (new_item.weight > sorted_items[0].weight) {
		sorted_items[1] = sorted_items[0];
		sorted_items[0] = new_item;
	} else if (new_item.weight > sorted_items[1].weight) {
		sorted_items[1] = new_item;
	}
}

inline uint8_t index_of_or_zero(std::array<uint8_t, 2> haystack, uint8_t needle) {
	return haystack[1] == needle ? 1 : 0;
}

template <unsigned int NVoxels>
inline void assign_component_indices(
		const std::array<uint8_t, 2> available_material_indices,
		const std::array<uint8_t, NVoxels> cell_voxel_material_indices,
		std::array<uint8_t, NVoxels> &component_indices
) {
	for (unsigned int i = 0; i < component_indices.size(); ++i) {
		const uint8_t mi = cell_voxel_material_indices[i];
		// Pick corresponding material. If the material wasn't selected for blending, 0 will fallback on the
		// most-represented material.
		component_indices[i] = index_of_or_zero(available_material_indices, mi);
	}
}

template <unsigned int NVoxels>
void get_cell_materials(
		const Span<const uint8_t> voxel_material_indices,
		const FixedArray<uint32_t, NVoxels> &voxel_indices,
		CellMaterials<NVoxels> &cell
) {
	if (voxel_material_indices.size() == 1) {
		// All indices in the chunk are the same
		const uint8_t material_index = voxel_material_indices[0];
		cell.selected_indices[0] = material_index;
		// Fill in 2 different indices with only one having full weight.
		for (uint8_t i = 1; i < cell.selected_indices.size(); ++i) {
			cell.selected_indices[i] = (material_index + i) & 255;
		}

		for (unsigned int i = 0; i < cell.component_indices.size(); ++i) {
			cell.component_indices[i] = 0;
		}

	} else {
		// We can find up to NVoxels distinct materials in the cell, so let's create a small array that can hold that
		std::array<WeightedIndex, NVoxels> distinct_materials;

		// TODO This is pointless initialization, but without this the compiler complains...
		// Compilers don't see that each case we handle guarantees that the N first items are initialized by
		// `insert_combine`, so they emit a "potentially uninitialized usage" warning, which is treated as error and
		// prevents from compiling. This is wasted cycles. I wonder if there is a better alternative that doesn't
		// involve suppressing that warning.
		for (WeightedIndex &wi : distinct_materials) {
			wi = { 0, 0 };
		}

		// Lookup voxels
		std::array<uint8_t, NVoxels> cell_voxel_material_indices;
		for (unsigned int cvi = 0; cvi < voxel_indices.size(); ++cvi) {
			const uint32_t vi = voxel_indices[cvi];
			const uint8_t mi = voxel_material_indices[vi];
			cell_voxel_material_indices[cvi] = mi;
		}

		// Count materials
		uint32_t distinct_material_count = 0;
		for (const uint8_t mi : cell_voxel_material_indices) {
			insert_combine(distinct_materials, distinct_material_count, mi);
		}

		// Select 4 most-represented materials
		switch (distinct_material_count) {
				// case 0:
				// Not supposed to happen?

			case 1: {
				// Same material in whole cell.
				// Probably the most common case.
				const uint8_t i0 = distinct_materials[0].index;
				// First component is the selected material
				cell.selected_indices[0] = i0;
				// Set different material as placeholder in the other component
				cell.selected_indices[1] = (i0 + 1) & 0xff;
				// The whole cell uses component 0
				for (unsigned int i = 0; i < cell.component_indices.size(); ++i) {
					cell.component_indices[i] = 0;
				}
			} break;

			case 2: {
				// Probably the second most common case.
				// Sort distinct components by most-represented first.
				math::sort2_array(distinct_materials, WeightedIndex::compare_higher_weight);
				for (unsigned int i = 0; i < cell.selected_indices.size(); ++i) {
					cell.selected_indices[i] = distinct_materials[i].index;
				}
				assign_component_indices(cell.selected_indices, cell_voxel_material_indices, cell.component_indices);
			} break;

			case 3: {
				// More than 2 materials, the lowest one will not be kept
				math::sort3_array(distinct_materials, WeightedIndex::compare_higher_weight);
				for (unsigned int i = 0; i < cell.selected_indices.size(); ++i) {
					cell.selected_indices[i] = distinct_materials[i].index;
				}
				assign_component_indices(cell.selected_indices, cell_voxel_material_indices, cell.component_indices);
			} break;

			case 4: {
				math::sort4_array(distinct_materials, WeightedIndex::compare_higher_weight);
				for (unsigned int i = 0; i < cell.selected_indices.size(); ++i) {
					cell.selected_indices[i] = distinct_materials[i].index;
				}
				assign_component_indices(cell.selected_indices, cell_voxel_material_indices, cell.component_indices);
			} break;

			default: {
				// More than 4 materials.
				// General sort. Should be rare.
				std::array<WeightedIndex, 2> selected_distinct_materials;
				for (WeightedIndex &it : selected_distinct_materials) {
					it = { 0, 0 };
				}
				for (const WeightedIndex wi : distinct_materials) {
					insert_sort(selected_distinct_materials, wi);
				}
				for (unsigned int i = 0; i < cell.selected_indices.size(); ++i) {
					cell.selected_indices[i] = selected_distinct_materials[i].index;
				}
				assign_component_indices(cell.selected_indices, cell_voxel_material_indices, cell.component_indices);
			} break;
		}
	}

	cell.packed_indices = pack_bytes(cell.selected_indices);
}

inline void add_2i8_1w8_texture_data(StdVector<float> &dst, const uint16_t packed_indices, const uint8_t weight) {
	union Reinterpreter {
		uint32_t i;
		float f;
	};
	Reinterpreter u;
	u.i = static_cast<uint32_t>(packed_indices) | (static_cast<uint32_t>(weight) << 16);
	dst.push_back(u.f);
}

template <unsigned int NVoxels>
struct Processor {
	const Span<const uint8_t> voxel_material_indices;
	CellMaterials<NVoxels> cell;
	StdVector<float> &output_mesh_material_data;

	Processor(
			Span<const uint8_t> p_voxel_material_indices,
			StdVector<float> &p_output_mesh_material_data
	) :
			voxel_material_indices(p_voxel_material_indices), //
			output_mesh_material_data(p_output_mesh_material_data) //
	{}

	inline uint32_t on_cell(const FixedArray<uint32_t, NVoxels> &corner_voxel_indices, const uint8_t case_code) {
		get_cell_materials<NVoxels>(voxel_material_indices, corner_voxel_indices, cell);
		return cell.packed_indices;
	}

	inline uint32_t on_transition_cell(const FixedArray<uint32_t, 9> &corner_voxel_indices, const uint8_t case_code) {
		// const uint16_t alt_case_code = textures_skip_air_voxels ? reorder_transition_case_code(case_code) : 0;

		// Get values from 9 significant corners
		CellMaterials<9> cell_materials_partial;
		get_cell_materials<9>(voxel_material_indices, corner_voxel_indices, cell_materials_partial);

		// Fill in slots that are just repeating others

		cell.selected_indices = cell_materials_partial.selected_indices;
		cell.packed_indices = cell_materials_partial.packed_indices;

		fill_redundant_transition_cell_values(cell_materials_partial.component_indices, cell.component_indices);

		return cell.packed_indices;
	}

	inline void on_vertex(const unsigned int v0, const unsigned int v1, const float alpha) {
		std::array<float, 2> weights_f{ 0.f, 0.f };
		const uint8_t c0 = cell.component_indices[v0];
		const uint8_t c1 = cell.component_indices[v1];
		weights_f[c0] += 1.f - alpha;
		weights_f[c1] += alpha;
		const uint8_t weight = static_cast<uint8_t>(math::clamp(weights_f[1] * 255.f, 0.f, 255.f));
		add_2i8_1w8_texture_data(output_mesh_material_data, cell.packed_indices, weight);
	}
};

} // namespace zylann::voxel::transvoxel::materials::single::s2

#endif // VOXEL_TRANSVOXEL_MATERIALS_SINGLE_S2_H
