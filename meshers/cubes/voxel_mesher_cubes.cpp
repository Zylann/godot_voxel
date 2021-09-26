#include "voxel_mesher_cubes.h"
#include "../../storage/voxel_buffer.h"
#include "../../util/funcs.h"
#include "../../util/profiling.h"

namespace {
// 2-----3
// |     |
// |     |
// 0-----1
// [axis][front/back][i]
const uint8_t g_indices_lut[3][2][6] = {
	// X
	{
			// Front
			{ 0, 3, 2, 0, 1, 3 },
			// Back
			{ 0, 2, 3, 0, 3, 1 },
	},
	// Y
	{
			// Front
			{ 0, 2, 3, 0, 3, 1 },
			// Back
			{ 0, 3, 2, 0, 1, 3 },
	},
	// Z
	{
			// Front
			{ 0, 3, 2, 0, 1, 3 },
			// Back
			{ 0, 2, 3, 0, 3, 1 },
	}
};

const uint8_t g_face_axes_lut[Vector3i::AXIS_COUNT][2] = {
	// X
	{ Vector3i::AXIS_Y, Vector3i::AXIS_Z },
	// Y
	{ Vector3i::AXIS_X, Vector3i::AXIS_Z },
	// Z
	{ Vector3i::AXIS_X, Vector3i::AXIS_Y }
};

enum Side {
	SIDE_FRONT = 0,
	SIDE_BACK,
	SIDE_NONE // Either means there is no face, or it was consumed
};

} // namespace

// Returns:
// 0 if alpha is zero,
// 1 if alpha is neither zero neither max,
// 2 if alpha is max
inline uint8_t get_alpha_index(Color8 c) {
	return (c.a == 0xf) + (c.a > 0);
}

template <typename Voxel_T, typename Color_F>
void build_voxel_mesh_as_simple_cubes(
		FixedArray<VoxelMesherCubes::Arrays, VoxelMesherCubes::MATERIAL_COUNT> &out_arrays_per_material,
		const Span<Voxel_T> voxel_buffer,
		const Vector3i block_size,
		Color_F color_func) {
	//
	ERR_FAIL_COND(block_size.x < static_cast<int>(2 * VoxelMesherCubes::PADDING) ||
				  block_size.y < static_cast<int>(2 * VoxelMesherCubes::PADDING) ||
				  block_size.z < static_cast<int>(2 * VoxelMesherCubes::PADDING));

	const Vector3i min_pos = Vector3i(VoxelMesherCubes::PADDING);
	const Vector3i max_pos = block_size - Vector3i(VoxelMesherCubes::PADDING);
	const unsigned int row_size = block_size.y;
	const unsigned int deck_size = block_size.x * row_size;

	// Note: voxel buffers are indexed in ZXY order
	FixedArray<uint32_t, Vector3i::AXIS_COUNT> neighbor_offset_d_lut;
	neighbor_offset_d_lut[Vector3i::AXIS_X] = block_size.y;
	neighbor_offset_d_lut[Vector3i::AXIS_Y] = 1;
	neighbor_offset_d_lut[Vector3i::AXIS_Z] = block_size.x * block_size.y;

	FixedArray<uint32_t, VoxelMesherCubes::MATERIAL_COUNT> index_offsets(0);

	// For each axis
	for (unsigned int za = 0; za < Vector3i::AXIS_COUNT; ++za) {
		const unsigned int xa = g_face_axes_lut[za][0];
		const unsigned int ya = g_face_axes_lut[za][1];

		// For each deck
		for (unsigned int d = min_pos[za] - VoxelMesherCubes::PADDING; d < (unsigned int)max_pos[za]; ++d) {
			// For each cell of the deck, gather face info
			for (unsigned int fy = min_pos[ya]; fy < (unsigned int)max_pos[ya]; ++fy) {
				for (unsigned int fx = min_pos[xa]; fx < (unsigned int)max_pos[xa]; ++fx) {
					FixedArray<unsigned int, Vector3i::AXIS_COUNT> pos;
					pos[xa] = fx;
					pos[ya] = fy;
					pos[za] = d;

					const unsigned int voxel_index = pos[Vector3i::AXIS_Y] +
													 pos[Vector3i::AXIS_X] * row_size +
													 pos[Vector3i::AXIS_Z] * deck_size;

					const Voxel_T raw_color0 = voxel_buffer[voxel_index];
					const Voxel_T raw_color1 = voxel_buffer[voxel_index + neighbor_offset_d_lut[za]];

					const Color8 color0 = color_func(raw_color0);
					const Color8 color1 = color_func(raw_color1);

					// TODO Change this
					const uint8_t ai0 = get_alpha_index(color0);
					const uint8_t ai1 = get_alpha_index(color1);

					Color8 color;
					Side side;
					if (ai0 == ai1) {
						continue;
					} else if (ai0 > ai1) {
						color = color0;
						side = SIDE_BACK;
					} else {
						color = color1;
						side = SIDE_FRONT;
					}

					// Commit face to the mesh

					const uint8_t material_index = color.a < 255;
					VoxelMesherCubes::Arrays &arrays = out_arrays_per_material[material_index];

					const int vx0 = fx - VoxelMesherCubes::PADDING;
					const int vy0 = fy - VoxelMesherCubes::PADDING;
					const int vx1 = vx0 + 1;
					const int vy1 = vy0 + 1;

					Vector3 v0;
					v0[xa] = vx0;
					v0[ya] = vy0;
					v0[za] = d;

					Vector3 v1;
					v1[xa] = vx1;
					v1[ya] = vy0;
					v1[za] = d;

					Vector3 v2;
					v2[xa] = vx0;
					v2[ya] = vy1;
					v2[za] = d;

					Vector3 v3;
					v3[xa] = vx1;
					v3[ya] = vy1;
					v3[za] = d;

					Vector3 n;
					n[za] = side == SIDE_FRONT ? -1 : 1;

					// 2-----3
					// |     |
					// |     |
					// 0-----1

					arrays.positions.push_back(v0);
					arrays.positions.push_back(v1);
					arrays.positions.push_back(v2);
					arrays.positions.push_back(v3);

					// TODO Any way to not need Color anywhere? It's wasteful
					const Color colorf = color;
					arrays.colors.push_back(colorf);
					arrays.colors.push_back(colorf);
					arrays.colors.push_back(colorf);
					arrays.colors.push_back(colorf);

					arrays.normals.push_back(n);
					arrays.normals.push_back(n);
					arrays.normals.push_back(n);
					arrays.normals.push_back(n);

					const unsigned int index_offset = index_offsets[material_index];
					CRASH_COND(za >= 3 || side >= 2);
					const uint8_t *lut = g_indices_lut[za][side];
					for (unsigned int i = 0; i < 6; ++i) {
						arrays.indices.push_back(index_offset + lut[i]);
					}
					index_offsets[material_index] += 4;
				}
			}
		}
	}
}

template <typename Voxel_T, typename Color_F>
void build_voxel_mesh_as_greedy_cubes(
		FixedArray<VoxelMesherCubes::Arrays, VoxelMesherCubes::MATERIAL_COUNT> &out_arrays_per_material,
		const Span<Voxel_T> voxel_buffer,
		const Vector3i block_size,
		std::vector<uint8_t> &mask_memory_pool,
		Color_F color_func) {
	//
	ERR_FAIL_COND(block_size.x < static_cast<int>(2 * VoxelMesherCubes::PADDING) ||
				  block_size.y < static_cast<int>(2 * VoxelMesherCubes::PADDING) ||
				  block_size.z < static_cast<int>(2 * VoxelMesherCubes::PADDING));

	struct MaskValue {
		Voxel_T color;
		uint8_t side;

		inline bool operator==(const MaskValue &other) const {
			return color == other.color && side == other.side;
		}

		inline bool operator!=(const MaskValue &other) const {
			return color != other.color || side != other.side;
		}
	};

	const Vector3i min_pos = Vector3i(VoxelMesherCubes::PADDING);
	const Vector3i max_pos = block_size - Vector3i(VoxelMesherCubes::PADDING);
	const unsigned int row_size = block_size.y;
	const unsigned int deck_size = block_size.x * row_size;

	// Note: voxel buffers are indexed in ZXY order
	FixedArray<uint32_t, Vector3i::AXIS_COUNT> neighbor_offset_d_lut;
	neighbor_offset_d_lut[Vector3i::AXIS_X] = block_size.y;
	neighbor_offset_d_lut[Vector3i::AXIS_Y] = 1;
	neighbor_offset_d_lut[Vector3i::AXIS_Z] = block_size.x * block_size.y;

	FixedArray<uint32_t, VoxelMesherCubes::MATERIAL_COUNT> index_offsets(0);

	// For each axis
	for (unsigned int za = 0; za < Vector3i::AXIS_COUNT; ++za) {
		const unsigned int xa = g_face_axes_lut[za][0];
		const unsigned int ya = g_face_axes_lut[za][1];

		const unsigned int mask_size_x = (max_pos[xa] - min_pos[xa]);
		const unsigned int mask_size_y = (max_pos[ya] - min_pos[ya]);
		const unsigned int mask_area = mask_size_x * mask_size_y;
		// Using the vector as memory pool
		mask_memory_pool.resize(mask_area * sizeof(MaskValue));
		Span<MaskValue> mask(reinterpret_cast<MaskValue *>(mask_memory_pool.data()), 0, mask_area);

		// For each deck
		for (unsigned int d = min_pos[za] - VoxelMesherCubes::PADDING; d < (unsigned int)max_pos[za]; ++d) {
			// For each cell of the deck, gather face info
			for (unsigned int fy = min_pos[ya]; fy < (unsigned int)max_pos[ya]; ++fy) {
				for (unsigned int fx = min_pos[xa]; fx < (unsigned int)max_pos[xa]; ++fx) {
					FixedArray<unsigned int, Vector3i::AXIS_COUNT> pos;
					pos[xa] = fx;
					pos[ya] = fy;
					pos[za] = d;

					const unsigned int voxel_index = pos[Vector3i::AXIS_Y] +
													 pos[Vector3i::AXIS_X] * row_size +
													 pos[Vector3i::AXIS_Z] * deck_size;

					const Voxel_T raw_color0 = voxel_buffer[voxel_index];
					const Voxel_T raw_color1 = voxel_buffer[voxel_index + neighbor_offset_d_lut[za]];

					const Color8 color0 = color_func(raw_color0);
					const Color8 color1 = color_func(raw_color1);

					const uint8_t ai0 = get_alpha_index(color0);
					const uint8_t ai1 = get_alpha_index(color1);

					MaskValue mv;
					if (ai0 == ai1) {
						mv.side = SIDE_NONE;
					} else if (ai0 > ai1) {
						mv.color = raw_color0;
						mv.side = SIDE_BACK;
					} else {
						mv.color = raw_color1;
						mv.side = SIDE_FRONT;
					}

					mask[(fx - VoxelMesherCubes::PADDING) + (fy - VoxelMesherCubes::PADDING) * mask_size_x] = mv;
				}
			}

			struct L {
				static inline bool is_range_equal(
						const Span<MaskValue> &mask, unsigned int xmin, unsigned int xmax, MaskValue v) {
					for (unsigned int x = xmin; x < xmax; ++x) {
						if (mask[x] != v) {
							return false;
						}
					}
					return true;
				}
			};

			// Greedy quads
			for (unsigned int fy = 0; fy < mask_size_y; ++fy) {
				for (unsigned int fx = 0; fx < mask_size_x; ++fx) {
					const unsigned int mask_index = fx + fy * mask_size_x;
					const MaskValue m = mask[mask_index];

					if (m.side == SIDE_NONE) {
						continue;
					}

					// Check if the next faces are the same along X
					unsigned int rx = fx + 1;
					while (rx < mask_size_x && mask[rx + fy * mask_size_x] == m) {
						++rx;
					}

					// Check if the next rows of faces are the same along Y
					unsigned int ry = fy + 1;
					while (ry < mask_size_y &&
							L::is_range_equal(mask,
									fx + ry * mask_size_x,
									rx + ry * mask_size_x, m)) {
						++ry;
					}

					// Commit face to the mesh

					const Color colorf = color_func(m.color);
					const uint8_t material_index = colorf.a < 0.999f;
					VoxelMesherCubes::Arrays &arrays = out_arrays_per_material[material_index];

					Vector3 v0;
					v0[xa] = fx;
					v0[ya] = fy;
					v0[za] = d;

					Vector3 v1;
					v1[xa] = rx;
					v1[ya] = fy;
					v1[za] = d;

					Vector3 v2;
					v2[xa] = fx;
					v2[ya] = ry;
					v2[za] = d;

					Vector3 v3;
					v3[xa] = rx;
					v3[ya] = ry;
					v3[za] = d;

					Vector3 n;
					n[za] = m.side == SIDE_FRONT ? -1 : 1;

					// 2-----3
					// |     |
					// |     |
					// 0-----1

					arrays.positions.push_back(v0);
					arrays.positions.push_back(v1);
					arrays.positions.push_back(v2);
					arrays.positions.push_back(v3);

					arrays.colors.push_back(colorf);
					arrays.colors.push_back(colorf);
					arrays.colors.push_back(colorf);
					arrays.colors.push_back(colorf);

					arrays.normals.push_back(n);
					arrays.normals.push_back(n);
					arrays.normals.push_back(n);
					arrays.normals.push_back(n);

					const unsigned int index_offset = index_offsets[material_index];
					CRASH_COND(za >= 3 || m.side >= 2);
					const uint8_t *lut = g_indices_lut[za][m.side];
					for (unsigned int i = 0; i < 6; ++i) {
						arrays.indices.push_back(index_offset + lut[i]);
					}
					index_offsets[material_index] += 4;

					for (unsigned int j = fy; j < ry; ++j) {
						for (unsigned int i = fx; i < rx; ++i) {
							mask[i + j * mask_size_x].side = SIDE_NONE;
						}
					}
				}
			}
		}
	}
}

template <typename Voxel_T, typename Color_F>
void build_voxel_mesh_as_greedy_cubes_atlased(
		FixedArray<VoxelMesherCubes::Arrays, VoxelMesherCubes::MATERIAL_COUNT> &out_arrays_per_material,
		VoxelMesherCubes::GreedyAtlasData &out_greedy_atlas_data,
		const Span<Voxel_T> voxel_buffer,
		const Vector3i block_size,
		std::vector<uint8_t> &mask_memory_pool,
		Color_F color_func) {
	//
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND(block_size.x < static_cast<int>(2 * VoxelMesherCubes::PADDING) ||
				  block_size.y < static_cast<int>(2 * VoxelMesherCubes::PADDING) ||
				  block_size.z < static_cast<int>(2 * VoxelMesherCubes::PADDING));

	struct MaskValue {
		uint8_t side;
		uint8_t material_index;

		inline bool operator==(const MaskValue &other) const {
			return side == other.side && material_index == other.material_index;
		}

		inline bool operator!=(const MaskValue &other) const {
			return side != other.side || material_index != other.material_index;
		}
	};

	out_greedy_atlas_data.clear();

	const Vector3i min_pos = Vector3i(VoxelMesherCubes::PADDING);
	const Vector3i max_pos = block_size - Vector3i(VoxelMesherCubes::PADDING);
	const unsigned int row_size = block_size.y;
	const unsigned int deck_size = block_size.x * row_size;

	// Note: voxel buffers are indexed in ZXY order
	FixedArray<uint32_t, Vector3i::AXIS_COUNT> neighbor_offset_d_lut;
	neighbor_offset_d_lut[Vector3i::AXIS_X] = block_size.y;
	neighbor_offset_d_lut[Vector3i::AXIS_Y] = 1;
	neighbor_offset_d_lut[Vector3i::AXIS_Z] = block_size.x * block_size.y;

	FixedArray<uint32_t, VoxelMesherCubes::MATERIAL_COUNT> index_offsets(0);

	// For each axis
	for (unsigned int za = 0; za < Vector3i::AXIS_COUNT; ++za) {
		const unsigned int xa = g_face_axes_lut[za][0];
		const unsigned int ya = g_face_axes_lut[za][1];

		const unsigned int mask_size_x = (max_pos[xa] - min_pos[xa]);
		const unsigned int mask_size_y = (max_pos[ya] - min_pos[ya]);
		const unsigned int mask_area = mask_size_x * mask_size_y;
		// Using the vector as memory pool
		const unsigned int mask_memory_size = mask_area * sizeof(MaskValue);
		mask_memory_pool.resize(mask_memory_size + mask_area * sizeof(Color8));
		// `mask` and `colors` are grids covering one deck
		Span<MaskValue> mask(reinterpret_cast<MaskValue *>(mask_memory_pool.data()), 0, mask_area);
		Span<Color8> colors(reinterpret_cast<Color8 *>(mask_memory_pool.data() + mask_memory_size), 0, mask_area);

		// For each deck
		for (unsigned int d = min_pos[za] - VoxelMesherCubes::PADDING; d < (unsigned int)max_pos[za]; ++d) {
			// For each cell of the deck, gather face info
			for (unsigned int fy = min_pos[ya]; fy < (unsigned int)max_pos[ya]; ++fy) {
				for (unsigned int fx = min_pos[xa]; fx < (unsigned int)max_pos[xa]; ++fx) {
					FixedArray<unsigned int, Vector3i::AXIS_COUNT> pos;
					pos[xa] = fx;
					pos[ya] = fy;
					pos[za] = d;

					const unsigned int voxel_index = pos[Vector3i::AXIS_Y] +
													 pos[Vector3i::AXIS_X] * row_size +
													 pos[Vector3i::AXIS_Z] * deck_size;

					const Voxel_T raw_color0 = voxel_buffer[voxel_index];
					const Voxel_T raw_color1 = voxel_buffer[voxel_index + neighbor_offset_d_lut[za]];

					const Color8 color0 = color_func(raw_color0);
					const Color8 color1 = color_func(raw_color1);

					const uint8_t ai0 = get_alpha_index(color0);
					const uint8_t ai1 = get_alpha_index(color1);

					MaskValue mv;
					Color8 color;
					if (ai0 == ai1) {
						mv.side = SIDE_NONE;
					} else if (ai0 > ai1) {
						color = color0;
						mv.side = SIDE_BACK;
						mv.material_index = color.a < 0.999f;
					} else {
						color = color1;
						mv.side = SIDE_FRONT;
						mv.material_index = color.a < 0.999f;
					}

					const unsigned int mask_index =
							(fx - VoxelMesherCubes::PADDING) + (fy - VoxelMesherCubes::PADDING) * mask_size_x;
					mask[mask_index] = mv;
					colors[mask_index] = color;
				}
			}

			struct L {
				static inline bool is_range_equal(
						const Span<MaskValue> &mask, unsigned int xmin, unsigned int xmax, MaskValue v) {
					for (unsigned int x = xmin; x < xmax; ++x) {
						if (mask[x] != v) {
							return false;
						}
					}
					return true;
				}
			};

			// Greedy quads
			for (unsigned int fy = 0; fy < mask_size_y; ++fy) {
				for (unsigned int fx = 0; fx < mask_size_x; ++fx) {
					const unsigned int mask_index = fx + fy * mask_size_x;
					const MaskValue m = mask[mask_index];

					if (m.side == SIDE_NONE) {
						continue;
					}

					// Check if the next faces are the same along X
					unsigned int rx = fx + 1;
					while (rx < mask_size_x && mask[rx + fy * mask_size_x] == m) {
						++rx;
					}

					// Check if the next rows of faces are the same along Y
					unsigned int ry = fy + 1;
					while (ry < mask_size_y &&
							L::is_range_equal(mask,
									fx + ry * mask_size_x,
									rx + ry * mask_size_x, m)) {
						++ry;
					}

					// Commit face to the mesh

					const uint8_t material_index = m.material_index;
					VoxelMesherCubes::Arrays &arrays = out_arrays_per_material[material_index];

					Vector3 v0;
					v0[xa] = fx;
					v0[ya] = fy;
					v0[za] = d;

					Vector3 v1;
					v1[xa] = rx;
					v1[ya] = fy;
					v1[za] = d;

					Vector3 v2;
					v2[xa] = fx;
					v2[ya] = ry;
					v2[za] = d;

					Vector3 v3;
					v3[xa] = rx;
					v3[ya] = ry;
					v3[za] = d;

					Vector3 n;
					n[za] = m.side == SIDE_FRONT ? -1 : 1;

					// 2-----3
					// |     |
					// |     |
					// 0-----1

					arrays.positions.push_back(v0);
					arrays.positions.push_back(v1);
					arrays.positions.push_back(v2);
					arrays.positions.push_back(v3);

					VoxelMesherCubes::GreedyAtlasData::ImageInfo image_info;
					image_info.first_vertex_index = arrays.uvs.size();
					arrays.uvs.resize(arrays.uvs.size() + 4); // Values will be assigned in a second pass

					arrays.normals.push_back(n);
					arrays.normals.push_back(n);
					arrays.normals.push_back(n);
					arrays.normals.push_back(n);

					image_info.size_x = rx - fx;
					image_info.size_y = ry - fy;
					image_info.first_color_index = out_greedy_atlas_data.colors.size();
					out_greedy_atlas_data.colors.resize(
							out_greedy_atlas_data.colors.size() + image_info.size_x * image_info.size_y);

					const unsigned int index_offset = index_offsets[material_index];
					CRASH_COND(za >= 3 || m.side >= 2);
					const uint8_t *lut = g_indices_lut[za][m.side];
					for (unsigned int i = 0; i < 6; ++i) {
						arrays.indices.push_back(index_offset + lut[i]);
					}
					index_offsets[material_index] += 4;

					unsigned int im_i = image_info.first_color_index;
					for (unsigned int my = fy; my < ry; ++my) {
						const unsigned int i0 = fx + my * mask_size_x;
						{
							unsigned int i = i0;
							for (unsigned int mx = fx; mx < rx; ++mx) {
								mask[i].side = SIDE_NONE;
								++i;
							}
						}
						{
							unsigned int i = i0;
							for (unsigned int mx = fx; mx < rx; ++mx) {
								out_greedy_atlas_data.colors[im_i] = colors[i];
								++im_i;
								++i;
							}
						}
						// TODO Actually that code only missed an offset to its destination for each row?
						// Copy colors row by row
						// memcpy(out_greedy_atlas_data.colors.data() + image_info.first_color_index,
						// 		colors.data() + i0,
						// 		image_info.size_x * sizeof(Color8));
					}

					// TODO Optimization: if colors are uniform, we could allocate a shared single pixel instead.
					// This would reduce texture size and packing cost

					image_info.surface_index = material_index;
					out_greedy_atlas_data.images.push_back(image_info);
				}
			}
		}
	}
}

static Ref<Image> make_greedy_atlas(const VoxelMesherCubes::GreedyAtlasData &atlas_data,
		Span<VoxelMesherCubes::Arrays> surfaces) {
	//
	ERR_FAIL_COND_V(atlas_data.images.size() == 0, Ref<Image>());
	VOXEL_PROFILE_SCOPE();

	// Pack rectangles
	Vector<Vector2i> result_points;
	Vector2i result_size;
	{
		VOXEL_PROFILE_SCOPE_NAMED("Packing");
		Vector<Vector2i> sizes;
		sizes.resize(atlas_data.images.size());
		for (unsigned int i = 0; i < atlas_data.images.size(); ++i) {
			const VoxelMesherCubes::GreedyAtlasData::ImageInfo &im = atlas_data.images[i];
			sizes.write[i] = Vector2i(im.size_x, im.size_y);
		}
		Geometry::make_atlas(sizes, result_points, result_size);
	}

	// DEBUG
	// Ref<Image> debug_im;
	// debug_im.instance();
	// debug_im->create(result_size.x, result_size.y, false, Image::FORMAT_RGBA8);
	// debug_im->fill(Color(0, 0, 0));
	// for (unsigned int i = 0; i < atlas_data.images.size(); ++i) {
	// 	const Vector2i dst_pos = result_points[i];
	// 	const VoxelMesherCubes::GreedyAtlasData::ImageInfo &im = atlas_data.images[i];
	// 	Ref<Image> tmp;
	// 	tmp.instance();
	// 	tmp->create(im.size_x, im.size_y, false, debug_im->get_format());
	// 	tmp->fill(Color(Math::randf(), Math::randf(), Math::randf()));
	// 	debug_im->blit_rect(tmp, Rect2(0, 0, tmp->get_width(), tmp->get_height()), Vector2(dst_pos));
	// }
	// debug_im->save_png("debug_atlas_packing.png");

	// Update UVs
	const Vector2 uv_scale(1.f / float(result_size.x), 1.f / float(result_size.y));
	for (unsigned int i = 0; i < atlas_data.images.size(); ++i) {
		const VoxelMesherCubes::GreedyAtlasData::ImageInfo &im = atlas_data.images[i];
		VoxelMesherCubes::Arrays &surface = surfaces[im.surface_index];
		ERR_FAIL_COND_V(im.first_vertex_index + 4 > surface.uvs.size(), Ref<Image>());
		const unsigned int vi = im.first_vertex_index;
		const Vector2 pos(result_points[i]);
		// 2-----3
		// |     |
		// |     |
		// 0-----1
		surface.uvs[vi] = pos * uv_scale;
		surface.uvs[vi + 1] = (pos + Vector2(im.size_x, 0)) * uv_scale;
		surface.uvs[vi + 2] = (pos + Vector2(0, im.size_y)) * uv_scale;
		surface.uvs[vi + 3] = (pos + Vector2(im.size_x, im.size_y)) * uv_scale;
	}

	// Create image
	PoolVector<uint8_t> im_data;
	im_data.resize(result_size.x * result_size.y * 4 * sizeof(uint8_t));
	{
		PoolVector<uint8_t>::Write w = im_data.write();
		Span<Color8> dst_data = Span<Color8>(reinterpret_cast<Color8 *>(w.ptr()), result_size.x * result_size.y);

		// For all rectangles
		for (unsigned int i = 0; i < atlas_data.images.size(); ++i) {
			const VoxelMesherCubes::GreedyAtlasData::ImageInfo &im = atlas_data.images[i];
			const Vector2i dst_pos = result_points[i];
			Span<const Color8> src_data =
					const_span_from_position_and_size(atlas_data.colors, im.first_color_index, im.size_x * im.size_y);

			// Blit rectangle
			for (unsigned int y = 0; y < im.size_y; ++y) {
				for (unsigned int x = 0; x < im.size_x; ++x) {
					const unsigned int src_i = x + y * im.size_x;
					const unsigned int dst_i = (dst_pos.x + x) + (dst_pos.y + y) * result_size.x;
					dst_data[dst_i] = src_data[src_i];
				}
			}
		}
	}
	Ref<Image> image;
	image.instance();
	image->create(result_size.x, result_size.y, false, Image::FORMAT_RGBA8, im_data);

	return image;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

thread_local VoxelMesherCubes::Cache VoxelMesherCubes::_cache;

VoxelMesherCubes::VoxelMesherCubes() {
	set_padding(PADDING, PADDING);
}

VoxelMesherCubes::~VoxelMesherCubes() {
}

void VoxelMesherCubes::build(VoxelMesher::Output &output, const VoxelMesher::Input &input) {
	VOXEL_PROFILE_SCOPE();
	const int channel = VoxelBufferInternal::CHANNEL_COLOR;
	Cache &cache = _cache;

	for (unsigned int i = 0; i < cache.arrays_per_material.size(); ++i) {
		Arrays &a = cache.arrays_per_material[i];
		a.clear();
	}

	const VoxelBufferInternal &voxels = input.voxels;

	// Iterate 3D padded data to extract voxel faces.
	// This is the most intensive job in this class, so all required data should be as fit as possible.

	// The buffer we receive MUST be dense (i.e not compressed, and channels allocated).
	// That means we can use raw pointers to voxel data inside instead of using the higher-level getters,
	// and then save a lot of time.

	if (voxels.get_channel_compression(channel) == VoxelBufferInternal::COMPRESSION_UNIFORM) {
		// All voxels have the same type.
		// If it's all air, nothing to do. If it's all cubes, nothing to do either.
		return;

	} else if (voxels.get_channel_compression(channel) != VoxelBufferInternal::COMPRESSION_NONE) {
		// No other form of compression is allowed
		ERR_PRINT("VoxelMesherCubes received unsupported voxel compression");
		return;
	}

	Span<uint8_t> raw_channel;
	if (!voxels.get_channel_raw(channel, raw_channel)) {
		// Case supposedly handled before...
		ERR_PRINT("Something wrong happened");
		return;
	}

	const Vector3i block_size = voxels.get_size();
	const VoxelBufferInternal::Depth channel_depth = voxels.get_channel_depth(channel);

	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}
	// Note, we don't lock the palette because its data has fixed-size

	Ref<Image> atlas_image;

	switch (params.color_mode) {
		case COLOR_RAW:
			switch (channel_depth) {
				case VoxelBuffer::DEPTH_8_BIT:
					if (params.greedy_meshing) {
						build_voxel_mesh_as_greedy_cubes(
								cache.arrays_per_material,
								raw_channel,
								block_size,
								cache.mask_memory_pool,
								Color8::from_u8);
					} else {
						build_voxel_mesh_as_simple_cubes(
								cache.arrays_per_material,
								raw_channel,
								block_size,
								Color8::from_u8);
					}
					break;

				case VoxelBuffer::DEPTH_16_BIT:
					if (params.greedy_meshing) {
						build_voxel_mesh_as_greedy_cubes(
								cache.arrays_per_material,
								raw_channel.reinterpret_cast_to<uint16_t>(),
								block_size,
								cache.mask_memory_pool,
								Color8::from_u16);
					} else {
						build_voxel_mesh_as_simple_cubes(
								cache.arrays_per_material,
								raw_channel.reinterpret_cast_to<uint16_t>(),
								block_size,
								Color8::from_u16);
					}
					break;

				default:
					ERR_PRINT("Unsupported voxel depth");
					return;
			}
			break;

		case COLOR_MESHER_PALETTE: {
			ERR_FAIL_COND_MSG(params.palette.is_null(), "Palette mode is used but no palette was specified");

			struct GetColorFromPalette {
				VoxelColorPalette &palette;
				Color8 operator()(uint64_t i) const {
					// Note: even though this code may run in a thread, I'm not locking the palette at all because
					// it stores colors in a fixed-size array, and reading the wrong color won't cause any serious
					// problem. It's not supposed to change often in game anyways. If it does, better use shader mode.
					return palette.get_color8(i);
				}
			};
			const GetColorFromPalette get_color_from_palette{ **params.palette };

			switch (channel_depth) {
				case VoxelBuffer::DEPTH_8_BIT:
					if (params.greedy_meshing) {
						if (params.store_colors_in_texture) {
							build_voxel_mesh_as_greedy_cubes_atlased(
									cache.arrays_per_material,
									cache.greedy_atlas_data,
									raw_channel,
									block_size,
									cache.mask_memory_pool,
									get_color_from_palette);
							atlas_image = make_greedy_atlas(
									cache.greedy_atlas_data, to_span(cache.arrays_per_material));
						} else {
							build_voxel_mesh_as_greedy_cubes(
									cache.arrays_per_material,
									raw_channel,
									block_size,
									cache.mask_memory_pool,
									get_color_from_palette);
						}
					} else {
						build_voxel_mesh_as_simple_cubes(
								cache.arrays_per_material,
								raw_channel,
								block_size,
								get_color_from_palette);
					}
					break;

				case VoxelBuffer::DEPTH_16_BIT:
					if (params.greedy_meshing) {
						build_voxel_mesh_as_greedy_cubes(
								cache.arrays_per_material,
								raw_channel.reinterpret_cast_to<uint16_t>(),
								block_size,
								cache.mask_memory_pool,
								get_color_from_palette);
					} else {
						build_voxel_mesh_as_simple_cubes(
								cache.arrays_per_material,
								raw_channel.reinterpret_cast_to<uint16_t>(),
								block_size,
								get_color_from_palette);
					}
					break;

				default:
					ERR_PRINT("Unsupported voxel depth");
					return;
			}
		} break;

		case COLOR_SHADER_PALETTE: {
			ERR_FAIL_COND_MSG(params.palette.is_null(), "Palette mode is used but no palette was specified");

			struct GetIndexFromPalette {
				VoxelColorPalette &palette;
				Color8 operator()(uint64_t i) const {
					// Still providing alpha because it allows to separate the opaque and transparent surfaces
					return Color8(i, 0, 0, palette.get_color8(i).a);
				}
			};
			const GetIndexFromPalette get_index_from_palette{ **params.palette };

			switch (channel_depth) {
				case VoxelBuffer::DEPTH_8_BIT:
					if (params.greedy_meshing) {
						build_voxel_mesh_as_greedy_cubes(
								cache.arrays_per_material,
								raw_channel,
								block_size,
								cache.mask_memory_pool,
								get_index_from_palette);
					} else {
						build_voxel_mesh_as_simple_cubes(
								cache.arrays_per_material,
								raw_channel,
								block_size,
								get_index_from_palette);
					}
					break;

				case VoxelBuffer::DEPTH_16_BIT:
					if (params.greedy_meshing) {
						build_voxel_mesh_as_greedy_cubes(
								cache.arrays_per_material,
								raw_channel.reinterpret_cast_to<uint16_t>(),
								block_size,
								cache.mask_memory_pool,
								get_index_from_palette);
					} else {
						build_voxel_mesh_as_simple_cubes(
								cache.arrays_per_material,
								raw_channel.reinterpret_cast_to<uint16_t>(),
								block_size,
								get_index_from_palette);
					}
					break;

				default:
					ERR_PRINT("Unsupported voxel depth");
					return;
			}
		} break;

		default:
			CRASH_NOW();
			break;
	}

	if (input.lod > 0) {
		// TODO This is very crude LOD, there will be cracks at the borders.
		// One way would be to not cull faces on chunk borders if any neighbor face is air
		const float lod_scale = 1 << input.lod;
		for (unsigned int material_index = 0; material_index < cache.arrays_per_material.size(); ++material_index) {
			Arrays &arrays = cache.arrays_per_material[material_index];
			for (unsigned int i = 0; i < arrays.positions.size(); ++i) {
				arrays.positions[i] *= lod_scale;
			}
		}
	}

	// TODO We could return a single byte array and use Mesh::add_surface down the line?

	for (unsigned int material_index = 0; material_index < MATERIAL_COUNT; ++material_index) {
		const Arrays &arrays = cache.arrays_per_material[material_index];

		if (arrays.positions.size() != 0) {
			Array mesh_arrays;
			mesh_arrays.resize(Mesh::ARRAY_MAX);

			{
				PoolVector<Vector3> positions;
				PoolVector<Vector3> normals;
				PoolVector<int> indices;

				raw_copy_to(positions, arrays.positions);
				raw_copy_to(normals, arrays.normals);
				raw_copy_to(indices, arrays.indices);

				mesh_arrays[Mesh::ARRAY_VERTEX] = positions;
				mesh_arrays[Mesh::ARRAY_NORMAL] = normals;
				mesh_arrays[Mesh::ARRAY_INDEX] = indices;

				if (arrays.colors.size() > 0) {
					PoolVector<Color> colors;
					raw_copy_to(colors, arrays.colors);
					mesh_arrays[Mesh::ARRAY_COLOR] = colors;
				}
				if (arrays.uvs.size() > 0) {
					PoolVector<Vector2> uvs;
					raw_copy_to(uvs, arrays.uvs);
					mesh_arrays[Mesh::ARRAY_TEX_UV] = uvs;
				}
			}

			output.surfaces.push_back(mesh_arrays);

		} else {
			// Empty
			output.surfaces.push_back(Array());
		}
	}

	output.primitive_type = Mesh::PRIMITIVE_TRIANGLES;
	output.atlas_image = atlas_image;

	if (params.store_colors_in_texture) {
		// Don't compress UVs, they need to be precise. Not doing this causes noticeable offsets.
		output.compression_flags = Mesh::ARRAY_COMPRESS_DEFAULT & ~Mesh::ARRAY_COMPRESS_TEX_UV;
	}
	//output.compression_flags = Mesh::ARRAY_COMPRESS_COLOR;
}

void VoxelMesherCubes::set_greedy_meshing_enabled(bool enable) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.greedy_meshing = enable;
}

bool VoxelMesherCubes::is_greedy_meshing_enabled() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.greedy_meshing;
}

void VoxelMesherCubes::set_palette(Ref<VoxelColorPalette> palette) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.palette = palette;
}

Ref<VoxelColorPalette> VoxelMesherCubes::get_palette() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.palette;
}

void VoxelMesherCubes::set_color_mode(ColorMode mode) {
	ERR_FAIL_INDEX(mode, COLOR_MODE_COUNT);
	RWLockWrite wlock(_parameters_lock);
	_parameters.color_mode = mode;
}

VoxelMesherCubes::ColorMode VoxelMesherCubes::get_color_mode() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.color_mode;
}

void VoxelMesherCubes::set_store_colors_in_texture(bool enable) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.store_colors_in_texture = enable;
}

bool VoxelMesherCubes::get_store_colors_in_texture() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.store_colors_in_texture;
}

Ref<Resource> VoxelMesherCubes::duplicate(bool p_subresources) const {
	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	if (p_subresources && params.palette.is_valid()) {
		params.palette = params.palette->duplicate(true);
	}
	VoxelMesherCubes *d = memnew(VoxelMesherCubes);
	d->_parameters = params;

	return d;
}

int VoxelMesherCubes::get_used_channels_mask() const {
	return (1 << VoxelBuffer::CHANNEL_COLOR);
}

void VoxelMesherCubes::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_greedy_meshing_enabled", "enable"),
			&VoxelMesherCubes::set_greedy_meshing_enabled);
	ClassDB::bind_method(D_METHOD("is_greedy_meshing_enabled"), &VoxelMesherCubes::is_greedy_meshing_enabled);

	ClassDB::bind_method(D_METHOD("set_palette", "palette"), &VoxelMesherCubes::set_palette);
	ClassDB::bind_method(D_METHOD("get_palette"), &VoxelMesherCubes::get_palette);

	ClassDB::bind_method(D_METHOD("set_color_mode", "mode"), &VoxelMesherCubes::set_color_mode);
	ClassDB::bind_method(D_METHOD("get_color_mode"), &VoxelMesherCubes::get_color_mode);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "greedy_meshing_enabled"),
			"set_greedy_meshing_enabled", "is_greedy_meshing_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "color_mode"), "set_color_mode", "get_color_mode");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "palette", PROPERTY_HINT_RESOURCE_TYPE, "VoxelColorPalette"),
			"set_palette", "get_palette");

	BIND_ENUM_CONSTANT(MATERIAL_OPAQUE);
	BIND_ENUM_CONSTANT(MATERIAL_TRANSPARENT);
	BIND_ENUM_CONSTANT(MATERIAL_COUNT);

	BIND_ENUM_CONSTANT(COLOR_RAW);
	BIND_ENUM_CONSTANT(COLOR_MESHER_PALETTE);
	BIND_ENUM_CONSTANT(COLOR_SHADER_PALETTE);
}
