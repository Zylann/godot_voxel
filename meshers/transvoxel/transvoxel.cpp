#include "transvoxel.h"
#include "../../constants/cube_tables.h"
#include "../../util/godot/core/sort_array.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "transvoxel_tables.cpp"

//#define VOXEL_TRANSVOXEL_REUSE_VERTEX_ON_COINCIDENT_CASES

namespace zylann::voxel::transvoxel {

static const float TRANSITION_CELL_SCALE = 0.25;

// SDF values considered negative have a sign bit of 1 in this algorithm
inline uint8_t sign_f(float v) {
	return v < 0.f;
}

Vector3f get_border_offset(const Vector3f pos_scaled, const int lod_index, const Vector3i block_size_non_scaled) {
	// When transition meshes are inserted between blocks of different LOD, we need to make space for them.
	// Secondary vertex positions can be calculated by linearly transforming positions inside boundary cells
	// so that the full-size cell is scaled to a smaller size that allows space for between one and three
	// transition cells, as necessary, depending on the location with respect to the edges and corners of the
	// entire block. This can be accomplished by computing offsets (Δx, Δy, Δz) for the coordinates (x, y, z)
	// in any boundary cell.

	Vector3f delta;

	const float p2k = 1 << lod_index; // 2 ^ lod
	const float p2mk = 1.f / p2k; // 2 ^ (-lod)

	const float wk = TRANSITION_CELL_SCALE * p2k; // 2 ^ (lod - 2), if scale is 0.25

	for (unsigned int i = 0; i < Vector3iUtil::AXIS_COUNT; ++i) {
		const float p = pos_scaled[i];
		const float s = block_size_non_scaled[i];

		if (p < p2k) {
			// The vertex is inside the minimum cell.
			delta[i] = (1.0f - p2mk * p) * wk;

		} else if (p > (p2k * (s - 1))) {
			// The vertex is inside the maximum cell.
			delta[i] = ((p2k * s) - 1.0f - p) * wk;
		}
	}

	return delta;
}

inline Vector3f project_border_offset(Vector3f delta, Vector3f normal) {
	// Secondary position can be obtained with the following formula:
	//
	// | x |   | 1 - nx²   ,  -nx * ny  ,  -nx * nz |   | Δx |
	// | y | + | -nx * ny  ,  1 - ny²   ,  -ny * nz | * | Δy |
	// | z |   | -nx * nz  ,  -ny * nz  ,  1 - nz²  |   | Δz |
	//
	// clang-format off
	return Vector3f(
		(1 - normal.x * normal.x) * delta.x -      normal.y * normal.x  * delta.y -      normal.z * normal.x  * delta.z,
		    -normal.x * normal.y  * delta.x + (1 - normal.y * normal.y) * delta.y -      normal.z * normal.y  * delta.z,
		    -normal.x * normal.z  * delta.x -      normal.y * normal.z  * delta.y + (1 - normal.z * normal.z) * delta.z
	);
	// clang-format on
}

inline Vector3f get_secondary_position(
		const Vector3f primary, const Vector3f normal, const int lod_index, const Vector3i block_size_non_scaled) {
	Vector3f delta = get_border_offset(primary, lod_index, block_size_non_scaled);
	delta = project_border_offset(delta, normal);

	// At very low LOD levels, error can be high and make secondary positions shoot very far.
	// Saw this happen around LOD 8. This doesn't get rid of them, but should make it not noticeable.
	const float p2k = 1 << lod_index;
	delta = math::clamp(delta, Vector3f(-p2k), Vector3f(p2k));

	return primary + delta;
}

inline uint8_t get_border_mask(const Vector3i &pos, const Vector3i &block_size) {
	uint8_t mask = 0;

	//  1: -X
	//  2: +X
	//  4: -Y
	//  8: +Y
	// 16: -Z
	// 32: +Z

	for (unsigned int i = 0; i < Vector3iUtil::AXIS_COUNT; i++) {
		// Close to negative face.
		if (pos[i] == 0) {
			mask |= (1 << (i * 2));
		}
		// Close to positive face.
		if (pos[i] == block_size[i]) {
			mask |= (1 << (i * 2 + 1));
		}
	}

	return mask;
}

inline Vector3f normalized_not_null(Vector3f n) {
	const float lengthsq = math::length_squared(n);
	if (lengthsq == 0) {
		return Vector3f(0, 1, 0);
	} else {
		const float length = Math::sqrt(lengthsq);
		return Vector3f(n.x / length, n.y / length, n.z / length);
	}
}

inline Vector3i dir_to_prev_vec(uint8_t dir) {
	// return g_corner_dirs[mask] - Vector3f(1,1,1);
	return Vector3i(-(dir & 1), -((dir >> 1) & 1), -((dir >> 2) & 1));
}

inline float sdf_as_float(int8_t v) {
	return -s8_to_snorm_noclamp(v);
}

inline float sdf_as_float(int16_t v) {
	return -s16_to_snorm_noclamp(v);
}

inline float sdf_as_float(float v) {
	return -v;
}

inline float sdf_as_float(double v) {
	return -v;
}

template <typename Sdf_T>
inline Vector3f get_corner_gradient(unsigned int data_index, Span<const Sdf_T> sdf_data, const Vector3i block_size) {
	const unsigned int n010 = 1; // Y+1
	const unsigned int n100 = block_size.y; // X+1
	const unsigned int n001 = block_size.y * block_size.x; // Z+1

	const float nx = sdf_as_float(sdf_data[data_index - n100]);
	const float px = sdf_as_float(sdf_data[data_index + n100]);
	const float ny = sdf_as_float(sdf_data[data_index - n010]);
	const float py = sdf_as_float(sdf_data[data_index + n010]);
	const float nz = sdf_as_float(sdf_data[data_index - n001]);
	const float pz = sdf_as_float(sdf_data[data_index + n001]);

	// get_gradient_normal(nx, px, ny, py, nz, pz, cell_samples[i]);
	return Vector3f(nx - px, ny - py, nz - pz);
}

inline uint32_t pack_bytes(const FixedArray<uint8_t, 4> &a) {
	return (a[0] | (a[1] << 8) | (a[2] << 16) | (a[3] << 24));
}

void add_texture_data(
		std::vector<Vector2f> &uv, unsigned int packed_indices, FixedArray<uint8_t, MAX_TEXTURE_BLENDS> weights) {
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
CellTextureDatas<NVoxels> select_textures_4_per_voxel(const FixedArray<unsigned int, NVoxels> &voxel_indices,
		Span<const uint16_t> indices_data, const WeightSampler_T &weights_sampler) {
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

		const unsigned int data_index = voxel_indices[ci];
		const FixedArray<uint8_t, 4> indices = decode_indices_from_packed_u16(indices_data[data_index]);
		const FixedArray<uint8_t, 4> weights = weights_sampler.get_weights(data_index);

		FixedArray<uint8_t, MAX_TEXTURES> &weights_temp = cell_texture_weights_temp[ci];
		fill(weights_temp, uint8_t(0));

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
		const FixedArray<uint8_t, MAX_TEXTURES> &src_weights = cell_texture_weights_temp[ci];
		FixedArray<uint8_t, 4> &dst_weights = cell_textures.weights[ci];

		for (unsigned int i = 0; i < cell_textures.indices.size(); ++i) {
			const unsigned int ti = cell_textures.indices[i];
			dst_weights[i] = src_weights[ti];
		}
	}

	return cell_textures;
}

struct TextureIndicesData {
	Span<const uint16_t> buffer;
	FixedArray<uint8_t, 4> default_indices;
	uint32_t packed_default_indices;
};

template <unsigned int NVoxels, typename WeightSampler_T>
inline void get_cell_texture_data(CellTextureDatas<NVoxels> &cell_textures,
		const TextureIndicesData &texture_indices_data, const FixedArray<unsigned int, NVoxels> &voxel_indices,
		const WeightSampler_T &weights_data) {
	if (texture_indices_data.buffer.size() == 0) {
		// Indices are known for the whole block, just read weights directly
		cell_textures.indices = texture_indices_data.default_indices;
		cell_textures.packed_indices = texture_indices_data.packed_default_indices;
		for (unsigned int ci = 0; ci < voxel_indices.size(); ++ci) {
			const unsigned int wi = voxel_indices[ci];
			cell_textures.weights[ci] = weights_data.get_weights(wi);
		}

	} else {
		// There can be more than 4 indices or they are not known, so we have to select them
		cell_textures = select_textures_4_per_voxel(voxel_indices, texture_indices_data.buffer, weights_data);
	}
}

template <typename Sdf_T>
inline Sdf_T get_isolevel() = delete;

template <>
inline int8_t get_isolevel<int8_t>() {
	return 0;
}

template <>
inline int16_t get_isolevel<int16_t>() {
	return 0;
}

template <>
inline float get_isolevel<float>() {
	return 0.f;
}

Vector3f binary_search_interpolate(const IDeepSDFSampler &sampler, float s0, float s1, Vector3i p0, Vector3i p1,
		uint32_t initial_lod_index, uint32_t min_lod_index) {
	for (uint32_t lod_index = initial_lod_index; lod_index > min_lod_index; --lod_index) {
		const Vector3i pm = (p0 + p1) >> 1;
		// TODO Optimization: this is quite slow for a small difference in the result.
		// Could be improved somewhat, but for now I don't think it's worth it
		const float sm = -sampler.get_single(pm, lod_index - 1);
		if (sign_f(s0) != sign_f(sm)) {
			p1 = pm;
			s1 = sm;
		} else {
			p0 = pm;
			s0 = sm;
		}
	}
	const float t = s1 / (s1 - s0);
	const float t0 = t;
	const float t1 = 1.f - t;
	return to_vec3f(p0) * t0 + to_vec3f(p1) * t1;
}

// This function is template so we avoid branches and checks when sampling voxels
template <typename Sdf_T, typename WeightSampler_T>
void build_regular_mesh(Span<const Sdf_T> sdf_data, TextureIndicesData texture_indices_data,
		const WeightSampler_T &weights_sampler, const Vector3i block_size_with_padding, uint32_t lod_index,
		TexturingMode texturing_mode, Cache &cache, MeshArrays &output, const IDeepSDFSampler *deep_sdf_sampler,
		std::vector<CellInfo> *cell_info) {
	ZN_PROFILE_SCOPE();

	// This function has some comments as quotes from the Transvoxel paper.

	const Vector3i block_size = block_size_with_padding - Vector3iUtil::create(MIN_PADDING + MAX_PADDING);
	const Vector3i block_size_scaled = block_size << lod_index;

	// Prepare vertex reuse cache
	cache.reset_reuse_cells(block_size_with_padding);

	// We iterate 2x2x2 voxel groups, which the paper calls "cells".
	// We also reach one voxel further to compute normals, so we adjust the iterated area
	const Vector3i min_pos = Vector3iUtil::create(MIN_PADDING);
	const Vector3i max_pos = block_size_with_padding - Vector3iUtil::create(MAX_PADDING);

	// How much to advance in the data array to get neighbor voxels
	const unsigned int n010 = 1; // Y+1
	const unsigned int n100 = block_size_with_padding.y; // X+1
	const unsigned int n001 = block_size_with_padding.y * block_size_with_padding.x; // Z+1
	const unsigned int n110 = n010 + n100;
	const unsigned int n101 = n100 + n001;
	const unsigned int n011 = n010 + n001;
	const unsigned int n111 = n100 + n010 + n001;

	// Get direct representation of the isolevel (not always zero since we are not using signed integers yet)
	const Sdf_T isolevel = get_isolevel<Sdf_T>();

	// Iterate all cells with padding (expected to be neighbors)
	Vector3i pos;
	for (pos.z = min_pos.z; pos.z < max_pos.z; ++pos.z) {
		for (pos.y = min_pos.y; pos.y < max_pos.y; ++pos.y) {
			// TODO Optimization: change iteration to be ZXY? (Data is laid out with Y as deepest coordinate)
			unsigned int data_index =
					Vector3iUtil::get_zxy_index(Vector3i(min_pos.x, pos.y, pos.z), block_size_with_padding);

			for (pos.x = min_pos.x; pos.x < max_pos.x; ++pos.x, data_index += block_size_with_padding.y) {
				{
					// The chosen comparison here is very important. This relates to case selections where 4 samples
					// are equal to the isolevel and 4 others are above or below:
					// In one of these two cases, there has to be a surface to extract, otherwise no surface will be
					// allowed to appear if it happens to line up with integer coordinates.
					// If we used `<` instead of `>`, it would appear to work, but would break those edge cases.
					// `>` is chosen because it must match the comparison we do with case selection (in Transvoxel
					// it is inverted).
					const bool s = sdf_data[data_index] > isolevel;

					if ( //
							(sdf_data[data_index + n010] > isolevel) == s &&
							(sdf_data[data_index + n100] > isolevel) == s &&
							(sdf_data[data_index + n110] > isolevel) == s &&
							(sdf_data[data_index + n001] > isolevel) == s &&
							(sdf_data[data_index + n011] > isolevel) == s &&
							(sdf_data[data_index + n101] > isolevel) == s &&
							(sdf_data[data_index + n111] > isolevel) == s) {
						// Not crossing the isolevel, this cell won't produce any geometry.
						// We must figure this out as fast as possible, because it will happen a lot.
						continue;
					}
				}

				// ZN_PROFILE_SCOPE();

				//    6-------7
				//   /|      /|
				//  / |     / |  Corners
				// 4-------5  |
				// |  2----|--3
				// | /     | /   z y
				// |/      |/    |/
				// 0-------1     o--x

				FixedArray<unsigned int, 8> corner_data_indices;
				corner_data_indices[0] = data_index;
				corner_data_indices[1] = data_index + n100;
				corner_data_indices[2] = data_index + n010;
				corner_data_indices[3] = data_index + n110;
				corner_data_indices[4] = data_index + n001;
				corner_data_indices[5] = data_index + n101;
				corner_data_indices[6] = data_index + n011;
				corner_data_indices[7] = data_index + n111;

				FixedArray<float, 8> cell_samples_sdf;
				for (unsigned int i = 0; i < corner_data_indices.size(); ++i) {
					cell_samples_sdf[i] = sdf_as_float(sdf_data[corner_data_indices[i]]);
				}

				// Concatenate the sign of cell values to obtain the case code.
				// Index 0 is the less significant bit, and index 7 is the most significant bit.
				uint8_t case_code = sign_f(cell_samples_sdf[0]);
				case_code |= (sign_f(cell_samples_sdf[1]) << 1);
				case_code |= (sign_f(cell_samples_sdf[2]) << 2);
				case_code |= (sign_f(cell_samples_sdf[3]) << 3);
				case_code |= (sign_f(cell_samples_sdf[4]) << 4);
				case_code |= (sign_f(cell_samples_sdf[5]) << 5);
				case_code |= (sign_f(cell_samples_sdf[6]) << 6);
				case_code |= (sign_f(cell_samples_sdf[7]) << 7);

				// TODO Is this really needed now that we check isolevel earlier?
				if (case_code == 0 || case_code == 255) {
					// If the case_code is 0 or 255, there is no triangulation to do.
					continue;
				}

				ReuseCell &current_reuse_cell = cache.get_reuse_cell(pos);

#if DEBUG_ENABLED
				ZN_ASSERT(case_code <= 255);
#endif

				FixedArray<Vector3i, 8> padded_corner_positions;
				padded_corner_positions[0] = Vector3i(pos.x, pos.y, pos.z);
				padded_corner_positions[1] = Vector3i(pos.x + 1, pos.y, pos.z);
				padded_corner_positions[2] = Vector3i(pos.x, pos.y + 1, pos.z);
				padded_corner_positions[3] = Vector3i(pos.x + 1, pos.y + 1, pos.z);
				padded_corner_positions[4] = Vector3i(pos.x, pos.y, pos.z + 1);
				padded_corner_positions[5] = Vector3i(pos.x + 1, pos.y, pos.z + 1);
				padded_corner_positions[6] = Vector3i(pos.x, pos.y + 1, pos.z + 1);
				padded_corner_positions[7] = Vector3i(pos.x + 1, pos.y + 1, pos.z + 1);

				CellTextureDatas<8> cell_textures;
				if (texturing_mode == TEXTURES_BLEND_4_OVER_16) {
					get_cell_texture_data(cell_textures, texture_indices_data, corner_data_indices, weights_sampler);
					current_reuse_cell.packed_texture_indices = cell_textures.packed_indices;
				}

				FixedArray<Vector3i, 8> corner_positions;
				for (unsigned int i = 0; i < padded_corner_positions.size(); ++i) {
					const Vector3i p = padded_corner_positions[i];
					// Undo padding here. From this point, corner positions are actual positions.
					corner_positions[i] = (p - min_pos) << lod_index;
				}

				// For cells occurring along the minimal boundaries of a block,
				// the preceding cells needed for vertex reuse may not exist.
				// In these cases, we allow new vertex creation on additional edges of a cell.
				// While iterating through the cells in a block, a 3-bit mask is maintained whose bits indicate
				// whether corresponding bits in a direction code are valid
				const uint8_t direction_validity_mask = (pos.x > min_pos.x ? 1 : 0) |
						((pos.y > min_pos.y ? 1 : 0) << 1) | ((pos.z > min_pos.z ? 1 : 0) << 2);

				const uint8_t regular_cell_class_index = tables::get_regular_cell_class(case_code);
				const tables::RegularCellData &regular_cell_data =
						tables::get_regular_cell_data(regular_cell_class_index);
				const uint8_t triangle_count = regular_cell_data.geometryCounts & 0x0f;
				const uint8_t vertex_count = (regular_cell_data.geometryCounts & 0xf0) >> 4;

				FixedArray<int, 12> cell_vertex_indices;
				fill(cell_vertex_indices, -1);

				const uint8_t cell_border_mask = get_border_mask(pos - min_pos, block_size - Vector3i(1, 1, 1));

				// For each vertex in the case
				for (unsigned int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
					// The case index maps to a list of 16-bit codes providing information about the edges on which the
					// vertices lie. The low byte of each 16-bit code contains the corner indexes of the edge’s
					// endpoints in one nibble each, and the high byte contains the mapping code shown in Figure 3.8(b)
					const unsigned short rvd = tables::get_regular_vertex_data(case_code, vertex_index);
					const uint8_t edge_code_low = rvd & 0xff;
					const uint8_t edge_code_high = (rvd >> 8) & 0xff;

					// Get corner indexes in the low nibble (always ordered so the higher comes last)
					const uint8_t v0 = (edge_code_low >> 4) & 0xf;
					const uint8_t v1 = edge_code_low & 0xf;

#ifdef DEBUG_ENABLED
					ZN_ASSERT_RETURN(v1 > v0);
#endif

					// Get voxel values at the corners
					const float sample0 = cell_samples_sdf[v0]; // called d0 in the paper
					const float sample1 = cell_samples_sdf[v1]; // called d1 in the paper

#ifdef DEBUG_ENABLED
					// TODO Zero-division is not mentionned in the paper?? (never happens tho)
					ZN_ASSERT_RETURN(sample1 != sample0);
					ZN_ASSERT_RETURN(sample1 != 0 || sample0 != 0);
#endif

					// Get interpolation position
					// We use an 8-bit fraction, allowing the new vertex to be located at one of 257 possible
					// positions  along  the  edge  when  both  endpoints  are included.
					// const int t = (sample1 << 8) / (sample1 - sample0);
					const float t = sample1 / (sample1 - sample0);

					const Vector3i p0 = corner_positions[v0];
					const Vector3i p1 = corner_positions[v1];

					if (t > 0.f && t < 1.f) {
						// Vertex is between p0 and p1 (inside the edge)

						// Each edge of a cell is assigned an 8-bit code, as shown in Figure 3.8(b),
						// that provides a mapping to a preceding cell and the coincident edge on that preceding cell
						// for which new vertex creation  was  allowed.
						// The high nibble of this code indicates which direction to go in order to reach the correct
						// preceding cell. The bit values 1, 2, and 4 in this nibble indicate that we must subtract one
						// from the x, y, and/or z coordinate, respectively.
						const uint8_t reuse_dir = (edge_code_high >> 4) & 0xf;
						const uint8_t reuse_vertex_index = edge_code_high & 0xf;

						// TODO Some re-use opportunities are missed on negative sides of the block,
						// but I don't really know how to fix it...
						// You can check by "shaking" every vertex randomly in a shader based on its index,
						// you will see vertices touching the -X, -Y or -Z sides of the block aren't connected

						const bool present = (reuse_dir & direction_validity_mask) == reuse_dir;

						if (present) {
							const Vector3i cache_pos = pos + dir_to_prev_vec(reuse_dir);
							const ReuseCell &prev_cell = cache.get_reuse_cell(cache_pos);
							if (prev_cell.packed_texture_indices == cell_textures.packed_indices) {
								// Will reuse a previous vertice
								cell_vertex_indices[vertex_index] = prev_cell.vertices[reuse_vertex_index];
							}
						}

						if (!present || cell_vertex_indices[vertex_index] == -1) {
							// Create new vertex

							const float t0 = t; // static_cast<float>(t) / 256.f;
							const float t1 = 1.f - t; // static_cast<float>(0x100 - t) / 256.f;
							// const int ti0 = t;
							// const int ti1 = 0x100 - t;
							// const Vector3i primary = p0 * ti0 + p1 * ti1;

							Vector3f primaryf;
							if (deep_sdf_sampler != nullptr) {
								primaryf = binary_search_interpolate(
										*deep_sdf_sampler, sample0, sample1, p0, p1, lod_index, 0);
							} else {
								primaryf = to_vec3f(p0) * t0 + to_vec3f(p1) * t1;
							}
							// TODO Binary search gives better positional results, but does not improve normals.
							// I'm not sure how to overcome this because if we sample low-detail normals, we get a
							// "blocky" result due to SDF clipping. If we sample high-detail gradients, we get details,
							// but if details are bumpy, we also get noisy results.
							const Vector3f cg0 = get_corner_gradient<Sdf_T>(
									corner_data_indices[v0], sdf_data, block_size_with_padding);
							const Vector3f cg1 = get_corner_gradient<Sdf_T>(
									corner_data_indices[v1], sdf_data, block_size_with_padding);
							const Vector3f normal = normalized_not_null(cg0 * t0 + cg1 * t1);

							Vector3f secondary;

							uint8_t vertex_border_mask = 0;
							if (cell_border_mask > 0) {
								secondary = get_secondary_position(primaryf, normal, lod_index, block_size);
								vertex_border_mask = (get_border_mask(p0, block_size_scaled) &
										get_border_mask(p1, block_size_scaled));
							}

							cell_vertex_indices[vertex_index] = output.add_vertex(
									primaryf, normal, cell_border_mask, vertex_border_mask, 0, secondary);

							if (texturing_mode == TEXTURES_BLEND_4_OVER_16) {
								const FixedArray<uint8_t, MAX_TEXTURE_BLENDS> weights0 = cell_textures.weights[v0];
								const FixedArray<uint8_t, MAX_TEXTURE_BLENDS> weights1 = cell_textures.weights[v1];
								FixedArray<uint8_t, MAX_TEXTURE_BLENDS> weights;
								for (unsigned int i = 0; i < MAX_TEXTURE_BLENDS; ++i) {
									weights[i] = static_cast<uint8_t>(
											math::clamp(Math::lerp(weights0[i], weights1[i], t1), 0.f, 255.f));
								}
								add_texture_data(output.texturing_data, cell_textures.packed_indices, weights);
							}

							if (reuse_dir & 8) {
								// Store the generated vertex so that other cells can reuse it.
								current_reuse_cell.vertices[reuse_vertex_index] = cell_vertex_indices[vertex_index];
							}
						}

					} else if (t == 0 && v1 == 7) {
						// t == 0: the vertex is on p1
						// v1 == 7: p1 on the max corner of the cell
						// This cell owns the vertex, so it should be created.

						const Vector3i primary = p1;
						const Vector3f primaryf = to_vec3f(primary);
						const Vector3f cg1 =
								get_corner_gradient<Sdf_T>(corner_data_indices[v1], sdf_data, block_size_with_padding);
						const Vector3f normal = normalized_not_null(cg1);

						Vector3f secondary;

						uint8_t vertex_border_mask = 0;
						if (cell_border_mask > 0) {
							secondary = get_secondary_position(primaryf, normal, lod_index, block_size);
							vertex_border_mask = get_border_mask(p1, block_size_scaled);
						}

						cell_vertex_indices[vertex_index] =
								output.add_vertex(primaryf, normal, cell_border_mask, vertex_border_mask, 0, secondary);

						if (texturing_mode == TEXTURES_BLEND_4_OVER_16) {
							const FixedArray<uint8_t, MAX_TEXTURE_BLENDS> weights1 = cell_textures.weights[v1];
							add_texture_data(output.texturing_data, cell_textures.packed_indices, weights1);
						}

						current_reuse_cell.vertices[0] = cell_vertex_indices[vertex_index];

					} else {
						// The vertex is either on p0 or p1.
						// The original Transvoxel tries to reuse previous vertices in these cases,
						// however here we don't do it because of ambiguous cases that makes artifacts appear.
						// It's not a common case so it shouldn't be too bad
						// (unless you do a lot of grid-aligned shapes?).

						// What do we do if the vertex we would re-use is on a cell that had no triangulation?
						// The previous cell might have had all corners of the same sign, except one being 0.
						// Forcing `present=false` seems to fix cases of holes that would be caused by that.
						// Resetting the cache before processing each deck also works, but is slightly slower.
						// Otherwise the code would try to re-use a vertex that hasn't been written as re-usable,
						// so it picks up some garbage from earlier decks.
#ifdef VOXEL_TRANSVOXEL_REUSE_VERTEX_ON_COINCIDENT_CASES
						// A 3-bit direction code leading to the proper cell can easily be obtained by
						// inverting the 3-bit corner index (bitwise, by exclusive ORing with the number 7).
						// The corner index depends on the value of t, t = 0 means that we're at the higher
						// numbered endpoint.
						const uint8_t reuse_dir = (t == 0 ? v1 ^ 7 : v0 ^ 7);
						const bool present = (reuse_dir & direction_validity_mask) == reuse_dir;

						// Note: the only difference with similar code above is that we take vertice 0 in the `else`
						if (present) {
							const Vector3i cache_pos = pos + dir_to_prev_vec(reuse_dir);
							const ReuseCell &prev_cell = cache.get_reuse_cell(cache_pos);
							cell_vertex_indices[vertex_index] = prev_cell.vertices[0];
						}

						if (!present || cell_vertex_indices[vertex_index] == -1)
#endif
						{
							// Create new vertex

							// TODO Earlier we associated t==0 to p0, why are we doing p1 here?
							const unsigned int vi = t == 0 ? v1 : v0;

							const Vector3i primary = t == 0 ? p1 : p0;
							const Vector3f primaryf = to_vec3f(primary);
							const Vector3f cg = get_corner_gradient<Sdf_T>(
									corner_data_indices[vi], sdf_data, block_size_with_padding);
							const Vector3f normal = normalized_not_null(cg);

							// TODO This bit of code is repeated several times, factor it?
							Vector3f secondary;

							uint8_t vertex_border_mask = 0;
							if (cell_border_mask > 0) {
								secondary = get_secondary_position(primaryf, normal, lod_index, block_size);
								vertex_border_mask = get_border_mask(primary, block_size_scaled);
							}

							cell_vertex_indices[vertex_index] = output.add_vertex(
									primaryf, normal, cell_border_mask, vertex_border_mask, 0, secondary);

							if (texturing_mode == TEXTURES_BLEND_4_OVER_16) {
								const FixedArray<uint8_t, MAX_TEXTURE_BLENDS> weights = cell_textures.weights[vi];
								add_texture_data(output.texturing_data, cell_textures.packed_indices, weights);
							}
						}
					}

				} // for each cell vertex

				for (int t = 0; t < triangle_count; ++t) {
					const int t0 = t * 3;
					const int i0 = cell_vertex_indices[regular_cell_data.get_vertex_index(t0)];
					const int i1 = cell_vertex_indices[regular_cell_data.get_vertex_index(t0 + 1)];
					const int i2 = cell_vertex_indices[regular_cell_data.get_vertex_index(t0 + 2)];
					output.indices.push_back(i0);
					output.indices.push_back(i1);
					output.indices.push_back(i2);
				}

				if (cell_info != nullptr) {
					cell_info->push_back(CellInfo{ pos - min_pos, triangle_count });
				}

			} // x
		} // y
	} // z
}

//    y            y
//    |            | z
//    |            |/     OpenGL axis convention
//    o---x    x---o
//   /
//  z

// Convert from face-space to block-space coordinates, considering which face we are working on.
inline Vector3i face_to_block(int x, int y, int z, int dir, const Vector3i &bs) {
	// There are several possible solutions to this, because we can rotate the axes.
	// We'll take configurations where XY map different axes at the same relative orientations,
	// so only Z is flipped in half cases.
	switch (dir) {
		case Cube::SIDE_NEGATIVE_X:
			return Vector3i(z, x, y);

		case Cube::SIDE_POSITIVE_X:
			return Vector3i(bs.x - 1 - z, y, x);

		case Cube::SIDE_NEGATIVE_Y:
			return Vector3i(y, z, x);

		case Cube::SIDE_POSITIVE_Y:
			return Vector3i(x, bs.y - 1 - z, y);

		case Cube::SIDE_NEGATIVE_Z:
			return Vector3i(x, y, z);

		case Cube::SIDE_POSITIVE_Z:
			return Vector3i(y, x, bs.z - 1 - z);

		default:
			CRASH_NOW();
			return Vector3i();
	}
}

// I took the choice of supporting non-cubic area, so...
inline void get_face_axes(int &ax, int &ay, int dir) {
	switch (dir) {
		case Cube::SIDE_NEGATIVE_X:
			ax = Vector3i::AXIS_Y;
			ay = Vector3i::AXIS_Z;
			break;

		case Cube::SIDE_POSITIVE_X:
			ax = Vector3i::AXIS_Z;
			ay = Vector3i::AXIS_Y;
			break;

		case Cube::SIDE_NEGATIVE_Y:
			ax = Vector3i::AXIS_Z;
			ay = Vector3i::AXIS_X;
			break;

		case Cube::SIDE_POSITIVE_Y:
			ax = Vector3i::AXIS_X;
			ay = Vector3i::AXIS_Z;
			break;

		case Cube::SIDE_NEGATIVE_Z:
			ax = Vector3i::AXIS_X;
			ay = Vector3i::AXIS_Y;
			break;

		case Cube::SIDE_POSITIVE_Z:
			ax = Vector3i::AXIS_Y;
			ay = Vector3i::AXIS_X;
			break;

		default:
			ZN_CRASH();
	}
}

// TODO Cube::Side has a legacy issue where Y axes are inverted compared to the others
inline uint8_t get_face_index(int cube_dir) {
	switch (cube_dir) {
		case Cube::SIDE_NEGATIVE_X:
			return 0;

		case Cube::SIDE_POSITIVE_X:
			return 1;

		case Cube::SIDE_NEGATIVE_Y:
			return 2;

		case Cube::SIDE_POSITIVE_Y:
			return 3;

		case Cube::SIDE_NEGATIVE_Z:
			return 4;

		case Cube::SIDE_POSITIVE_Z:
			return 5;

		default:
			ZN_CRASH();
			return 0;
	}
}

template <typename Sdf_T, typename WeightSampler_T>
void build_transition_mesh(Span<const Sdf_T> sdf_data, TextureIndicesData texture_indices_data,
		const WeightSampler_T &weights_sampler, const Vector3i block_size_with_padding, int direction, int lod_index,
		TexturingMode texturing_mode, Cache &cache, MeshArrays &output) {
	// From this point, we expect the buffer to contain allocated data.
	// This function has some comments as quotes from the Transvoxel paper.

	const Vector3i block_size_without_padding =
			block_size_with_padding - Vector3iUtil::create(MIN_PADDING + MAX_PADDING);
	const Vector3i block_size_scaled = block_size_without_padding << lod_index;

	ZN_ASSERT_RETURN(block_size_with_padding.x >= 3);
	ZN_ASSERT_RETURN(block_size_with_padding.y >= 3);
	ZN_ASSERT_RETURN(block_size_with_padding.z >= 3);

	cache.reset_reuse_cells_2d(block_size_with_padding);

	// This part works in "face space", which is 2D along local X and Y axes.
	// In this space, -Z points towards the half resolution cells, while +Z points towards full-resolution cells.
	// Conversion is used to map this space to block space using a direction enum.

	// Note: I made a few changes compared to the paper.
	// Instead of making transition meshes go from low-res blocks to high-res blocks,
	// I do the opposite, going from high-res to low-res. It's easier because half-res voxels are available for free,
	// if we compute the transition meshes right after the regular mesh, with the same voxel data.
	// TODO One issue with this change however, is a "bump" of mesh density which can be noticeable.

	// This represents the actual box of voxels we are working on.
	// It also represents positions of the minimum and maximum vertices that can be generated.
	// Padding is present to allow reaching 1 voxel further for calculating normals
	const Vector3i min_pos = Vector3iUtil::create(MIN_PADDING);
	const Vector3i max_pos = block_size_with_padding - Vector3iUtil::create(MAX_PADDING);

	int axis_x, axis_y;
	get_face_axes(axis_x, axis_y, direction);
	const int min_fpos_x = min_pos[axis_x];
	const int min_fpos_y = min_pos[axis_y];
	const int max_fpos_x = max_pos[axis_x] - 1; // Another -1 here, because the 2D kernel is 3x3
	const int max_fpos_y = max_pos[axis_y] - 1;

	// How much to advance in the data array to get neighbor voxels
	const unsigned int n010 = 1; // Y+1
	const unsigned int n100 = block_size_with_padding.y; // X+1
	const unsigned int n001 = block_size_with_padding.y * block_size_with_padding.x; // Z+1
	// const unsigned int n110 = n010 + n100;
	// const unsigned int n101 = n100 + n001;
	// const unsigned int n011 = n010 + n001;
	// const unsigned int n111 = n100 + n010 + n001;

	// Using temporay locals otherwise clang-format makes it hard to read
	const Vector3i ftb_000 = face_to_block(0, 0, 0, direction, block_size_with_padding);
	const Vector3i ftb_x00 = face_to_block(1, 0, 0, direction, block_size_with_padding);
	const Vector3i ftb_0y0 = face_to_block(0, 1, 0, direction, block_size_with_padding);
	// How much to advance in the data array to get neighbor voxels, using face coordinates
	const int fn00 = Vector3iUtil::get_zxy_index(ftb_000, block_size_with_padding);
	const int fn10 = Vector3iUtil::get_zxy_index(ftb_x00, block_size_with_padding) - fn00;
	const int fn01 = Vector3iUtil::get_zxy_index(ftb_0y0, block_size_with_padding) - fn00;
	const int fn11 = fn10 + fn01;
	const int fn21 = 2 * fn10 + fn01;
	const int fn22 = 2 * fn10 + 2 * fn01;
	const int fn12 = fn10 + 2 * fn01;
	const int fn20 = 2 * fn10;
	const int fn02 = 2 * fn01;

	FixedArray<Vector3i, 13> cell_positions;
	const int fz = MIN_PADDING;

	const Sdf_T isolevel = get_isolevel<Sdf_T>();

	const uint8_t transition_hint_mask = 1 << get_face_index(direction);

	// Iterating in face space
	for (int fy = min_fpos_y; fy < max_fpos_y; fy += 2) {
		for (int fx = min_fpos_x; fx < max_fpos_x; fx += 2) {
			// Cell positions in block space
			// Warning: temporarily includes padding. It is undone later.
			cell_positions[0] = face_to_block(fx, fy, fz, direction, block_size_with_padding);

			const int data_index = Vector3iUtil::get_zxy_index(cell_positions[0], block_size_with_padding);

			{
				const bool s = sdf_data[data_index] > isolevel;

				// `//` prevents clang-format from messing up
				if ( //
						(sdf_data[data_index + fn10] > isolevel) == s && //
						(sdf_data[data_index + fn20] > isolevel) == s && //
						(sdf_data[data_index + fn01] > isolevel) == s && //
						(sdf_data[data_index + fn11] > isolevel) == s && //
						(sdf_data[data_index + fn21] > isolevel) == s && //
						(sdf_data[data_index + fn02] > isolevel) == s && //
						(sdf_data[data_index + fn12] > isolevel) == s && //
						(sdf_data[data_index + fn22] > isolevel) == s) {
					// Not crossing the isolevel, this cell won't produce any geometry.
					// We must figure this out as fast as possible, because it will happen a lot.
					continue;
				}
			}

			FixedArray<unsigned int, 9> cell_data_indices;
			cell_data_indices[0] = data_index;
			cell_data_indices[1] = data_index + fn10;
			cell_data_indices[2] = data_index + fn20;
			cell_data_indices[3] = data_index + fn01;
			cell_data_indices[4] = data_index + fn11;
			cell_data_indices[5] = data_index + fn21;
			cell_data_indices[6] = data_index + fn02;
			cell_data_indices[7] = data_index + fn12;
			cell_data_indices[8] = data_index + fn22;

			//  6---7---8
			//  |   |   |
			//  3---4---5
			//  |   |   |
			//  0---1---2

			// Full-resolution samples 0..8
			FixedArray<float, 13> cell_samples;
			for (unsigned int i = 0; i < 9; ++i) {
				cell_samples[i] = sdf_as_float(sdf_data[cell_data_indices[i]]);
			}

			//  B-------C
			//  |       |
			//  |       |
			//  |       |
			//  9-------A

			// Half-resolution samples 9..C: they are the same
			cell_samples[0x9] = cell_samples[0];
			cell_samples[0xA] = cell_samples[2];
			cell_samples[0xB] = cell_samples[6];
			cell_samples[0xC] = cell_samples[8];

			uint16_t case_code = sign_f(cell_samples[0]);
			case_code |= (sign_f(cell_samples[1]) << 1);
			case_code |= (sign_f(cell_samples[2]) << 2);
			case_code |= (sign_f(cell_samples[5]) << 3);
			case_code |= (sign_f(cell_samples[8]) << 4);
			case_code |= (sign_f(cell_samples[7]) << 5);
			case_code |= (sign_f(cell_samples[6]) << 6);
			case_code |= (sign_f(cell_samples[3]) << 7);
			case_code |= (sign_f(cell_samples[4]) << 8);

			if (case_code == 0 || case_code == 511) {
				// The cell contains no triangles.
				continue;
			}

			CellTextureDatas<13> cell_textures;
			if (texturing_mode == TEXTURES_BLEND_4_OVER_16) {
				CellTextureDatas<9> cell_textures_partial;
				get_cell_texture_data(cell_textures_partial, texture_indices_data, cell_data_indices, weights_sampler);

				cell_textures.indices = cell_textures_partial.indices;
				cell_textures.packed_indices = cell_textures_partial.packed_indices;

				for (unsigned int i = 0; i < cell_textures_partial.weights.size(); ++i) {
					cell_textures.weights[i] = cell_textures_partial.weights[i];
				}

				cell_textures.weights[0x9] = cell_textures_partial.weights[0];
				cell_textures.weights[0xA] = cell_textures_partial.weights[2];
				cell_textures.weights[0xB] = cell_textures_partial.weights[6];
				cell_textures.weights[0xC] = cell_textures_partial.weights[8];

				ReuseTransitionCell &current_reuse_cell = cache.get_reuse_cell_2d(fx, fy);
				current_reuse_cell.packed_texture_indices = cell_textures.packed_indices;
			}

			ZN_ASSERT(case_code <= 511);

			// TODO We may not need all of them!
			FixedArray<Vector3f, 13> cell_gradients;
			for (unsigned int i = 0; i < 9; ++i) {
				const unsigned int di = cell_data_indices[i];

				const float nx = sdf_as_float(sdf_data[di - n100]);
				const float ny = sdf_as_float(sdf_data[di - n010]);
				const float nz = sdf_as_float(sdf_data[di - n001]);
				const float px = sdf_as_float(sdf_data[di + n100]);
				const float py = sdf_as_float(sdf_data[di + n010]);
				const float pz = sdf_as_float(sdf_data[di + n001]);

				cell_gradients[i] = Vector3f(nx - px, ny - py, nz - pz);
			}
			cell_gradients[0x9] = cell_gradients[0];
			cell_gradients[0xA] = cell_gradients[2];
			cell_gradients[0xB] = cell_gradients[6];
			cell_gradients[0xC] = cell_gradients[8];

			// TODO Optimization: get rid of conditionals involved in face_to_block
			cell_positions[1] = face_to_block(fx + 1, fy + 0, fz, direction, block_size_with_padding);
			cell_positions[2] = face_to_block(fx + 2, fy + 0, fz, direction, block_size_with_padding);
			cell_positions[3] = face_to_block(fx + 0, fy + 1, fz, direction, block_size_with_padding);
			cell_positions[4] = face_to_block(fx + 1, fy + 1, fz, direction, block_size_with_padding);
			cell_positions[5] = face_to_block(fx + 2, fy + 1, fz, direction, block_size_with_padding);
			cell_positions[6] = face_to_block(fx + 0, fy + 2, fz, direction, block_size_with_padding);
			cell_positions[7] = face_to_block(fx + 1, fy + 2, fz, direction, block_size_with_padding);
			cell_positions[8] = face_to_block(fx + 2, fy + 2, fz, direction, block_size_with_padding);
			for (unsigned int i = 0; i < 9; ++i) {
				cell_positions[i] = (cell_positions[i] - min_pos) << lod_index;
			}
			cell_positions[0x9] = cell_positions[0];
			cell_positions[0xA] = cell_positions[2];
			cell_positions[0xB] = cell_positions[6];
			cell_positions[0xC] = cell_positions[8];

			const uint8_t cell_class = tables::get_transition_cell_class(case_code);

			CRASH_COND((cell_class & 0x7f) > 55);

			const tables::TransitionCellData cell_data = tables::get_transition_cell_data(cell_class & 0x7f);
			const bool flip_triangles = ((cell_class & 128) != 0);

			const unsigned int vertex_count = cell_data.GetVertexCount();
			FixedArray<int, 12> cell_vertex_indices;
			fill(cell_vertex_indices, -1);
			CRASH_COND(vertex_count > cell_vertex_indices.size());

			const uint8_t direction_validity_mask = (fx > min_fpos_x ? 1 : 0) | ((fy > min_fpos_y ? 1 : 0) << 1);

			const uint8_t cell_border_mask = get_border_mask(cell_positions[0], block_size_scaled);

			for (unsigned int vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
				const uint16_t edge_code = tables::get_transition_vertex_data(case_code, vertex_index);
				const uint8_t index_vertex_a = (edge_code >> 4) & 0xf;
				const uint8_t index_vertex_b = (edge_code & 0xf);

				const float sample_a = cell_samples[index_vertex_a]; // d0 and d1 in the paper
				const float sample_b = cell_samples[index_vertex_b];
				// TODO Zero-division is not mentionned in the paper??
				ZN_ASSERT_RETURN(sample_a != sample_b);
				ZN_ASSERT_RETURN(sample_a != 0 || sample_b != 0);

				// Get interpolation position
				// We use an 8-bit fraction, allowing the new vertex to be located at one of 257 possible
				// positions  along  the  edge  when  both  endpoints  are included.
				// const int t = (sample_b << 8) / (sample_b - sample_a);
				const float t = sample_b / (sample_b - sample_a);

				const float t0 = t; // static_cast<float>(t) / 256.f;
				const float t1 = 1.f - t; // static_cast<float>(0x100 - t) / 256.f;
				// const int ti0 = t;
				// const int ti1 = 0x100 - t;

				if (t > 0.f && t < 1.f) {
					// Vertex lies in the interior of the edge.
					// (i.e t is either 0 or 257, meaning it's either directly on vertex a or vertex b)

					const uint8_t vertex_index_to_reuse_or_create = (edge_code >> 8) & 0xf;

					// The bit values 1 and 2 in this nibble indicate that we must subtract one from the x or y
					// coordinate, respectively, and these two bits are never simultaneously set.
					// The bit value 4 indicates that a new vertex is to be created on an interior edge
					// where it cannot be reused, and the bit value 8 indicates that a new vertex is to be created on a
					// maximal  edge  where  it  can  be  reused.
					//
					// Bit 0 (0x1): need to subtract one to X
					// Bit 1 (0x2): need to subtract one to Y
					// Bit 2 (0x4): vertex is on an interior edge, won't be reused
					// Bit 3 (0x8): vertex is on a maximal edge, it can be reused
					const uint8_t reuse_direction = (edge_code >> 12);

					const bool present = (reuse_direction & direction_validity_mask) == reuse_direction;

					if (present) {
						// The previous cell is available. Retrieve the cached cell
						// from which to retrieve the reused vertex index from.
						const ReuseTransitionCell &prev =
								cache.get_reuse_cell_2d(fx - (reuse_direction & 1), fy - ((reuse_direction >> 1) & 1));
						if (prev.packed_texture_indices == cell_textures.packed_indices) {
							// Reuse the vertex index from the previous cell.
							cell_vertex_indices[vertex_index] = prev.vertices[vertex_index_to_reuse_or_create];
						}
					}

					if (!present || cell_vertex_indices[vertex_index] == -1) {
						// Going to create a new vertex

						const Vector3i p0 = cell_positions[index_vertex_a];
						const Vector3i p1 = cell_positions[index_vertex_b];

						const Vector3f n0 = cell_gradients[index_vertex_a];
						const Vector3f n1 = cell_gradients[index_vertex_b];

						// Vector3i primary = p0 * ti0 + p1 * ti1;
						const Vector3f primaryf = to_vec3f(p0) * t0 + to_vec3f(p1) * t1;
						const Vector3f normal = normalized_not_null(n0 * t0 + n1 * t1);

						const bool fullres_side = (index_vertex_a < 9 || index_vertex_b < 9);

						Vector3f secondary;
						uint8_t cell_border_mask2 = cell_border_mask;
						uint8_t vertex_border_mask = 0;
						if (fullres_side) {
							secondary = get_secondary_position(primaryf, normal, lod_index, block_size_without_padding);
							vertex_border_mask =
									(get_border_mask(p0, block_size_scaled) & get_border_mask(p1, block_size_scaled));
						} else {
							// If the vertex is on the half-res side (in our implementation,
							// it's the side of the block), then we make the mask 0 so that the vertex is never moved.
							// We only move the full-res side to connect with the regular mesh,
							// which will also be moved by the same amount to fit the transition mesh.
							cell_border_mask2 = 0;
						}

						cell_vertex_indices[vertex_index] = output.add_vertex(primaryf, normal, cell_border_mask2,
								vertex_border_mask, transition_hint_mask, secondary);

						if (texturing_mode == TEXTURES_BLEND_4_OVER_16) {
							const FixedArray<uint8_t, MAX_TEXTURE_BLENDS> weights0 =
									cell_textures.weights[index_vertex_a];
							const FixedArray<uint8_t, MAX_TEXTURE_BLENDS> weights1 =
									cell_textures.weights[index_vertex_b];
							FixedArray<uint8_t, MAX_TEXTURE_BLENDS> weights;
							for (unsigned int i = 0; i < cell_textures.indices.size(); ++i) {
								weights[i] = static_cast<uint8_t>(
										math::clamp(Math::lerp(weights0[i], weights1[i], t1), 0.f, 255.f));
							}
							add_texture_data(output.texturing_data, cell_textures.packed_indices, weights);
						}

						if (reuse_direction & 0x8) {
							// The vertex can be re-used later
							ReuseTransitionCell &r = cache.get_reuse_cell_2d(fx, fy);
							r.vertices[vertex_index_to_reuse_or_create] = cell_vertex_indices[vertex_index];
						}
					}

				} else {
					// The vertex is exactly on one of the edge endpoints.
					// Try to reuse corner vertex from a preceding cell.
					// Use the reuse information in transitionCornerData.

					const uint8_t cell_index = (t == 0 ? index_vertex_b : index_vertex_a);
					CRASH_COND(cell_index >= 13);
					const uint8_t corner_data = tables::get_transition_corner_data(cell_index);
					const uint8_t vertex_index_to_reuse_or_create = (corner_data & 0xf);
					const uint8_t reuse_direction = ((corner_data >> 4) & 0xf);

					const bool present = (reuse_direction & direction_validity_mask) == reuse_direction;

					if (present) {
						// The previous cell is available. Retrieve the cached cell
						// from which to retrieve the reused vertex index from.
						const ReuseTransitionCell &prev =
								cache.get_reuse_cell_2d(fx - (reuse_direction & 1), fy - ((reuse_direction >> 1) & 1));
						// Reuse the vertex index from the previous cell.
						cell_vertex_indices[vertex_index] = prev.vertices[vertex_index_to_reuse_or_create];
					}

					if (!present || cell_vertex_indices[vertex_index] == -1) {
						// Going to create a new vertex

						const Vector3i primary = cell_positions[cell_index];
						const Vector3f primaryf = to_vec3f(primary);
						const Vector3f normal = normalized_not_null(cell_gradients[cell_index]);

						const bool fullres_side = (cell_index < 9);

						Vector3f secondary;
						uint8_t vertex_border_mask = 0;
						uint8_t cell_border_mask2 = cell_border_mask;
						if (fullres_side) {
							secondary = get_secondary_position(primaryf, normal, lod_index, block_size_without_padding);
							vertex_border_mask = get_border_mask(primary, block_size_scaled);
						} else {
							cell_border_mask2 = 0;
						}

						cell_vertex_indices[vertex_index] = output.add_vertex(primaryf, normal, cell_border_mask2,
								vertex_border_mask, transition_hint_mask, secondary);

						if (texturing_mode == TEXTURES_BLEND_4_OVER_16) {
							const FixedArray<uint8_t, MAX_TEXTURE_BLENDS> weights = cell_textures.weights[cell_index];
							add_texture_data(output.texturing_data, cell_textures.packed_indices, weights);
						}

						// We are on a corner so the vertex will be re-usable later
						ReuseTransitionCell &r = cache.get_reuse_cell_2d(fx, fy);
						r.vertices[vertex_index_to_reuse_or_create] = cell_vertex_indices[vertex_index];
					}
				}

			} // for vertex

			const unsigned int triangle_count = cell_data.GetTriangleCount();

			for (unsigned int ti = 0; ti < triangle_count; ++ti) {
				if (flip_triangles) {
					output.indices.push_back(cell_vertex_indices[cell_data.get_vertex_index(ti * 3)]);
					output.indices.push_back(cell_vertex_indices[cell_data.get_vertex_index(ti * 3 + 1)]);
					output.indices.push_back(cell_vertex_indices[cell_data.get_vertex_index(ti * 3 + 2)]);
				} else {
					output.indices.push_back(cell_vertex_indices[cell_data.get_vertex_index(ti * 3 + 2)]);
					output.indices.push_back(cell_vertex_indices[cell_data.get_vertex_index(ti * 3 + 1)]);
					output.indices.push_back(cell_vertex_indices[cell_data.get_vertex_index(ti * 3)]);
				}
			}

		} // for x
	} // for y
}

template <typename T>
Span<const T> get_or_decompress_channel(
		const VoxelBufferInternal &voxels, std::vector<T> &backing_buffer, unsigned int channel) {
	//
	ZN_ASSERT_RETURN_V(
			voxels.get_channel_depth(channel) == VoxelBufferInternal::get_depth_from_size(sizeof(T)), Span<const T>());

	if (voxels.get_channel_compression(channel) == VoxelBufferInternal::COMPRESSION_UNIFORM) {
		backing_buffer.resize(Vector3iUtil::get_volume(voxels.get_size()));
		const T v = voxels.get_voxel(Vector3i(), channel);
		// TODO Could use a fast fill using 8-byte blocks or intrinsics?
		for (unsigned int i = 0; i < backing_buffer.size(); ++i) {
			backing_buffer[i] = v;
		}
		return to_span_const(backing_buffer);

	} else {
		Span<uint8_t> data_bytes;
		ZN_ASSERT(voxels.get_channel_raw(channel, data_bytes) == true);
		return data_bytes.reinterpret_cast_to<const T>();
	}
}

TextureIndicesData get_texture_indices_data(const VoxelBufferInternal &voxels, unsigned int channel,
		DefaultTextureIndicesData &out_default_texture_indices_data) {
	ZN_ASSERT_RETURN_V(voxels.get_channel_depth(channel) == VoxelBufferInternal::DEPTH_16_BIT, TextureIndicesData());

	TextureIndicesData data;

	if (voxels.is_uniform(channel)) {
		const uint16_t encoded_indices = voxels.get_voxel(Vector3i(), channel);
		data.default_indices = decode_indices_from_packed_u16(encoded_indices);
		data.packed_default_indices = pack_bytes(data.default_indices);

		out_default_texture_indices_data.indices = data.default_indices;
		out_default_texture_indices_data.packed_indices = data.packed_default_indices;
		out_default_texture_indices_data.use = true;

	} else {
		Span<uint8_t> data_bytes;
		ZN_ASSERT(voxels.get_channel_raw(channel, data_bytes) == true);
		data.buffer = data_bytes.reinterpret_cast_to<const uint16_t>();

		out_default_texture_indices_data.use = false;
	}

	return data;
}

// I'm not really decided if doing this is better or not yet?
//#define USE_TRICHANNEL

#ifdef USE_TRICHANNEL
// TODO Is this a faster/equivalent option with better precision?
struct WeightSampler3U8 {
	Span<const uint8_t> u8_data0;
	Span<const uint8_t> u8_data1;
	Span<const uint8_t> u8_data2;
	inline FixedArray<uint8_t, 4> get_weights(int i) const {
		FixedArray<uint8_t, 4> w;
		w[0] = u8_data0[i];
		w[1] = u8_data1[i];
		w[2] = u8_data2[i];
		w[3] = 255 - (w[0] + w[1] + w[2]);
		return w;
	}
};

thread_local std::vector<uint8_t> s_weights_backing_buffer_u8_0;
thread_local std::vector<uint8_t> s_weights_backing_buffer_u8_1;
thread_local std::vector<uint8_t> s_weights_backing_buffer_u8_2;

#else
struct WeightSamplerPackedU16 {
	Span<const uint16_t> u16_data;
	inline FixedArray<uint8_t, 4> get_weights(int i) const {
		return decode_weights_from_packed_u16(u16_data[i]);
	}
};

std::vector<uint16_t> &get_tls_weights_backing_buffer_u16() {
	thread_local std::vector<uint16_t> tls_weights_backing_buffer_u16;
	return tls_weights_backing_buffer_u16;
}
#endif

// Presence of zeroes in samples occurs more often when precision is scarce
// (8-bit, scaled SDF, or slow gradients).
// This causes two symptoms:
// - Degenerate triangles. Potentially bad for systems using the mesh later (MeshOptimizer, physics)
// - Glitched triangles. Wrong vertices get re-used.
//   Needs closer investigation to know why, maybe related to case selection
//
// See also https://github.com/zeux/meshoptimizer/issues/312
//
// A quick fix to avoid it is to add a tiny offset to values equal to the isolevel.
// It must be done on the whole buffer to ensure consistency (and not after early cell rejection),
// otherwise it can create gaps in the final mesh.
//
// Not used anymore for now, but if we get this problem again we may have to investigate
//
/*template <typename Sdf_T>
Span<const Sdf_T> apply_zero_sdf_fix(Span<const Sdf_T> p_sdf_data) {
	ZN_PROFILE_SCOPE();

	static thread_local std::vector<Sdf_T> s_sdf_backing_buffer;
	std::vector<Sdf_T> &sdf_data = s_sdf_backing_buffer;

	sdf_data.resize(p_sdf_data.size());
	memcpy(sdf_data.data(), p_sdf_data.data(), p_sdf_data.size());

	for (auto it = sdf_data.begin(); it != sdf_data.end(); ++it) {
		if (*it == get_isolevel<Sdf_T>()) {
			// Assuming Sdf_T is an integer. Might not be needed for floats.
			*it += 1;
		}
	}
	return to_span_const(sdf_data);
}*/

DefaultTextureIndicesData build_regular_mesh(const VoxelBufferInternal &voxels, unsigned int sdf_channel,
		uint32_t lod_index, TexturingMode texturing_mode, Cache &cache, MeshArrays &output,
		const IDeepSDFSampler *deep_sdf_sampler, std::vector<CellInfo> *cell_infos) {
	ZN_PROFILE_SCOPE();
	// From this point, we expect the buffer to contain allocated data in the relevant channels.

	Span<uint8_t> sdf_data_raw;
	ZN_ASSERT(voxels.get_channel_raw(sdf_channel, sdf_data_raw) == true);

	const unsigned int voxels_count = Vector3iUtil::get_volume(voxels.get_size());

	DefaultTextureIndicesData default_texture_indices_data;
	default_texture_indices_data.use = false;
	TextureIndicesData indices_data;
#ifdef USE_TRICHANNEL
	WeightSampler3U8 weights_data;
	if (texturing_mode == TEXTURES_BLEND_4_OVER_16) {
		// From this point we know SDF is not uniform so it has an allocated buffer,
		// but it might have uniform indices or weights so we need to ensure there is a backing buffer.
		indices_data =
				get_texture_indices_data(voxels, VoxelBufferInternal::CHANNEL_INDICES, default_texture_indices_data);
		weights_data.u8_data0 =
				get_or_decompress_channel(voxels, s_weights_backing_buffer_u8_0, VoxelBufferInternal::CHANNEL_WEIGHTS);
		weights_data.u8_data1 =
				get_or_decompress_channel(voxels, s_weights_backing_buffer_u8_1, VoxelBufferInternal::CHANNEL_DATA5);
		weights_data.u8_data2 =
				get_or_decompress_channel(voxels, s_weights_backing_buffer_u8_2, VoxelBufferInternal::CHANNEL_DATA6);
		ERR_FAIL_COND_V(weights_data.u8_data0.size() != voxels_count, default_texture_indices_data);
		ERR_FAIL_COND_V(weights_data.u8_data1.size() != voxels_count, default_texture_indices_data);
		ERR_FAIL_COND_V(weights_data.u8_data2.size() != voxels_count, default_texture_indices_data);
	}
#else
	WeightSamplerPackedU16 weights_data;
	if (texturing_mode == TEXTURES_BLEND_4_OVER_16) {
		// From this point we know SDF is not uniform so it has an allocated buffer,
		// but it might have uniform indices or weights so we need to ensure there is a backing buffer.
		indices_data =
				get_texture_indices_data(voxels, VoxelBufferInternal::CHANNEL_INDICES, default_texture_indices_data);
		weights_data.u16_data = get_or_decompress_channel(
				voxels, get_tls_weights_backing_buffer_u16(), VoxelBufferInternal::CHANNEL_WEIGHTS);
		ZN_ASSERT_RETURN_V(weights_data.u16_data.size() == voxels_count, default_texture_indices_data);
	}
#endif

	// We settle data types up-front so we can get rid of abstraction layers and conditionals,
	// which would otherwise harm performance in tight iterations
	switch (voxels.get_channel_depth(sdf_channel)) {
		case VoxelBufferInternal::DEPTH_8_BIT: {
			Span<const int8_t> sdf_data = sdf_data_raw.reinterpret_cast_to<const int8_t>();
			build_regular_mesh<int8_t>(sdf_data, indices_data, weights_data, voxels.get_size(), lod_index,
					texturing_mode, cache, output, deep_sdf_sampler, cell_infos);
		} break;

		case VoxelBufferInternal::DEPTH_16_BIT: {
			Span<const int16_t> sdf_data = sdf_data_raw.reinterpret_cast_to<const int16_t>();
			build_regular_mesh<int16_t>(sdf_data, indices_data, weights_data, voxels.get_size(), lod_index,
					texturing_mode, cache, output, deep_sdf_sampler, cell_infos);
		} break;

		// TODO Remove support for 32-bit SDF in Transvoxel?
		// I don't think it's worth it. And it could reduce executable size significantly
		// (the optimized obj size for just transvoxel.cpp is 1.2 Mb on Windows)
		case VoxelBufferInternal::DEPTH_32_BIT: {
			Span<const float> sdf_data = sdf_data_raw.reinterpret_cast_to<const float>();
			build_regular_mesh<float>(sdf_data, indices_data, weights_data, voxels.get_size(), lod_index,
					texturing_mode, cache, output, deep_sdf_sampler, cell_infos);
		} break;

		case VoxelBufferInternal::DEPTH_64_BIT:
			ZN_PRINT_ERROR("Double-precision SDF channel is not supported");
			// Not worth growing executable size for relatively pointless double-precision sdf
			break;

		default:
			ZN_PRINT_ERROR("Invalid channel");
			break;
	}

	return default_texture_indices_data;
}

void build_transition_mesh(const VoxelBufferInternal &voxels, unsigned int sdf_channel, int direction,
		uint32_t lod_index, TexturingMode texturing_mode, Cache &cache, MeshArrays &output,
		DefaultTextureIndicesData default_texture_indices_data) {
	ZN_PROFILE_SCOPE();
	// From this point, we expect the buffer to contain allocated data in the relevant channels.

	Span<uint8_t> sdf_data_raw;
	ZN_ASSERT(voxels.get_channel_raw(sdf_channel, sdf_data_raw) == true);

	const unsigned int voxels_count = Vector3iUtil::get_volume(voxels.get_size());

	// TODO Support more texturing data configurations
	TextureIndicesData indices_data;
#ifdef USE_TRICHANNEL
	WeightSampler3U8 weights_data;
	if (texturing_mode == TEXTURES_BLEND_4_OVER_16) {
		if (default_texture_indices_data.use) {
			indices_data.default_indices = default_texture_indices_data.indices;
			indices_data.packed_default_indices = default_texture_indices_data.packed_indices;
		} else {
			// From this point we know SDF is not uniform so it has an allocated buffer,
			// but it might have uniform indices or weights so we need to ensure there is a backing buffer.
			// TODO Is it worth doing conditionnals instead during meshing?
			indices_data = get_texture_indices_data(
					voxels, VoxelBufferInternal::CHANNEL_INDICES, default_texture_indices_data);
		}
		weights_data.u8_data0 =
				get_or_decompress_channel(voxels, s_weights_backing_buffer_u8_0, VoxelBufferInternal::CHANNEL_WEIGHTS);
		weights_data.u8_data1 =
				get_or_decompress_channel(voxels, s_weights_backing_buffer_u8_1, VoxelBufferInternal::CHANNEL_DATA5);
		weights_data.u8_data2 =
				get_or_decompress_channel(voxels, s_weights_backing_buffer_u8_2, VoxelBufferInternal::CHANNEL_DATA6);
		ERR_FAIL_COND(weights_data.u8_data0.size() != voxels_count);
		ERR_FAIL_COND(weights_data.u8_data1.size() != voxels_count);
		ERR_FAIL_COND(weights_data.u8_data2.size() != voxels_count);
	}
#else
	WeightSamplerPackedU16 weights_data;
	if (texturing_mode == TEXTURES_BLEND_4_OVER_16) {
		if (default_texture_indices_data.use) {
			indices_data.default_indices = default_texture_indices_data.indices;
			indices_data.packed_default_indices = default_texture_indices_data.packed_indices;
		} else {
			// From this point we know SDF is not uniform so it has an allocated buffer,
			// but it might have uniform indices or weights so we need to ensure there is a backing buffer.
			// TODO Is it worth doing conditionnals instead during meshing?
			indices_data = get_texture_indices_data(
					voxels, VoxelBufferInternal::CHANNEL_INDICES, default_texture_indices_data);
		}
		weights_data.u16_data = get_or_decompress_channel(
				voxels, get_tls_weights_backing_buffer_u16(), VoxelBufferInternal::CHANNEL_WEIGHTS);
		ZN_ASSERT_RETURN(weights_data.u16_data.size() == voxels_count);
	}
#endif

	switch (voxels.get_channel_depth(sdf_channel)) {
		case VoxelBufferInternal::DEPTH_8_BIT: {
			Span<const int8_t> sdf_data = sdf_data_raw.reinterpret_cast_to<const int8_t>();
			build_transition_mesh<int8_t>(sdf_data, indices_data, weights_data, voxels.get_size(), direction, lod_index,
					texturing_mode, cache, output);
		} break;

		case VoxelBufferInternal::DEPTH_16_BIT: {
			Span<const int16_t> sdf_data = sdf_data_raw.reinterpret_cast_to<const int16_t>();
			build_transition_mesh<int16_t>(sdf_data, indices_data, weights_data, voxels.get_size(), direction,
					lod_index, texturing_mode, cache, output);
		} break;

		case VoxelBufferInternal::DEPTH_32_BIT: {
			Span<const float> sdf_data = sdf_data_raw.reinterpret_cast_to<const float>();
			build_transition_mesh<float>(sdf_data, indices_data, weights_data, voxels.get_size(), direction, lod_index,
					texturing_mode, cache, output);
		} break;

		case VoxelBufferInternal::DEPTH_64_BIT:
			ZN_PRINT_ERROR("Double-precision SDF channel is not supported");
			// Not worth growing executable size for relatively pointless double-precision sdf
			break;

		default:
			ZN_PRINT_ERROR("Invalid channel");
			break;
	}
}

} // namespace zylann::voxel::transvoxel
