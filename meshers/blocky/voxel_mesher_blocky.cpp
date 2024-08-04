#include "voxel_mesher_blocky.h"
#include "../../constants/cube_tables.h"
#include "../../storage/voxel_buffer.h"
#include "../../util/containers/span.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/packed_arrays.h"
#include "../../util/macros.h"
#include "../../util/math/conv.h"
// TODO GDX: String has no `operator+=`
#include "../../util/godot/core/string.h"
#include "../../util/profiling.h"

using namespace zylann::godot;

namespace zylann::voxel {

// Utility functions
namespace {
const int g_opposite_side[6] = {
	Cube::SIDE_NEGATIVE_X, //
	Cube::SIDE_POSITIVE_X, //
	Cube::SIDE_POSITIVE_Y, //
	Cube::SIDE_NEGATIVE_Y, //
	Cube::SIDE_POSITIVE_Z, //
	Cube::SIDE_NEGATIVE_Z //
};

inline bool is_face_visible(
		const VoxelBlockyLibraryBase::BakedData &lib,
		const VoxelBlockyModel::BakedData &vt,
		uint32_t other_voxel_id,
		int side
) {
	if (other_voxel_id < lib.models.size()) {
		const VoxelBlockyModel::BakedData &other_vt = lib.models[other_voxel_id];
		// TODO Maybe we could get rid of `empty` here and instead set `culls_neighbors` to false during baking
		if (other_vt.empty || (other_vt.transparency_index > vt.transparency_index) || !other_vt.culls_neighbors) {
			return true;
		} else {
			const unsigned int ai = vt.model.side_pattern_indices[side];
			const unsigned int bi = other_vt.model.side_pattern_indices[g_opposite_side[side]];
			// Patterns are not the same, and B does not occlude A
			return (ai != bi) && !lib.get_side_pattern_occlusion(bi, ai);
		}
	}
	return true;
}

inline bool contributes_to_ao(const VoxelBlockyLibraryBase::BakedData &lib, uint32_t voxel_id) {
	if (voxel_id < lib.models.size()) {
		const VoxelBlockyModel::BakedData &t = lib.models[voxel_id];
		return t.contributes_to_ao;
	}
	return true;
}

StdVector<int> &get_tls_index_offsets() {
	static thread_local StdVector<int> tls_index_offsets;
	return tls_index_offsets;
}

} // namespace

template <typename Type_T>
void generate_blocky_mesh( //
		StdVector<VoxelMesherBlocky::Arrays> &out_arrays_per_material, //
		VoxelMesher::Output::CollisionSurface *collision_surface, //
		const Span<const Type_T> type_buffer, //
		const Vector3i block_size, //
		const VoxelBlockyLibraryBase::BakedData &library, //
		bool bake_occlusion, //
		float baked_occlusion_darkness //
) {
	// TODO Optimization: not sure if this mandates a template function. There is so much more happening in this
	// function other than reading voxels, although reading is on the hottest path. It needs to be profiled. If
	// changing makes no difference, we could use a function pointer or switch inside instead to reduce executable size.

	ERR_FAIL_COND(
			block_size.x < static_cast<int>(2 * VoxelMesherBlocky::PADDING) ||
			block_size.y < static_cast<int>(2 * VoxelMesherBlocky::PADDING) ||
			block_size.z < static_cast<int>(2 * VoxelMesherBlocky::PADDING)
	);

	// Build lookup tables so to speed up voxel access.
	// These are values to add to an address in order to get given neighbor.

	const int row_size = block_size.y;
	const int deck_size = block_size.x * row_size;

	// Data must be padded, hence the off-by-one
	const Vector3i min = Vector3iUtil::create(VoxelMesherBlocky::PADDING);
	const Vector3i max = block_size - Vector3iUtil::create(VoxelMesherBlocky::PADDING);

	StdVector<int> &index_offsets = get_tls_index_offsets();
	index_offsets.clear();
	index_offsets.resize(out_arrays_per_material.size(), 0);

	int collision_surface_index_offset = 0;

	FixedArray<int, Cube::SIDE_COUNT> side_neighbor_lut;
	side_neighbor_lut[Cube::SIDE_LEFT] = row_size;
	side_neighbor_lut[Cube::SIDE_RIGHT] = -row_size;
	side_neighbor_lut[Cube::SIDE_BACK] = -deck_size;
	side_neighbor_lut[Cube::SIDE_FRONT] = deck_size;
	side_neighbor_lut[Cube::SIDE_BOTTOM] = -1;
	side_neighbor_lut[Cube::SIDE_TOP] = 1;

	FixedArray<int, Cube::EDGE_COUNT> edge_neighbor_lut;
	edge_neighbor_lut[Cube::EDGE_BOTTOM_BACK] =
			side_neighbor_lut[Cube::SIDE_BOTTOM] + side_neighbor_lut[Cube::SIDE_BACK];
	edge_neighbor_lut[Cube::EDGE_BOTTOM_FRONT] =
			side_neighbor_lut[Cube::SIDE_BOTTOM] + side_neighbor_lut[Cube::SIDE_FRONT];
	edge_neighbor_lut[Cube::EDGE_BOTTOM_LEFT] =
			side_neighbor_lut[Cube::SIDE_BOTTOM] + side_neighbor_lut[Cube::SIDE_LEFT];
	edge_neighbor_lut[Cube::EDGE_BOTTOM_RIGHT] =
			side_neighbor_lut[Cube::SIDE_BOTTOM] + side_neighbor_lut[Cube::SIDE_RIGHT];
	edge_neighbor_lut[Cube::EDGE_BACK_LEFT] = side_neighbor_lut[Cube::SIDE_BACK] + side_neighbor_lut[Cube::SIDE_LEFT];
	edge_neighbor_lut[Cube::EDGE_BACK_RIGHT] = side_neighbor_lut[Cube::SIDE_BACK] + side_neighbor_lut[Cube::SIDE_RIGHT];
	edge_neighbor_lut[Cube::EDGE_FRONT_LEFT] = side_neighbor_lut[Cube::SIDE_FRONT] + side_neighbor_lut[Cube::SIDE_LEFT];
	edge_neighbor_lut[Cube::EDGE_FRONT_RIGHT] =
			side_neighbor_lut[Cube::SIDE_FRONT] + side_neighbor_lut[Cube::SIDE_RIGHT];
	edge_neighbor_lut[Cube::EDGE_TOP_BACK] = side_neighbor_lut[Cube::SIDE_TOP] + side_neighbor_lut[Cube::SIDE_BACK];
	edge_neighbor_lut[Cube::EDGE_TOP_FRONT] = side_neighbor_lut[Cube::SIDE_TOP] + side_neighbor_lut[Cube::SIDE_FRONT];
	edge_neighbor_lut[Cube::EDGE_TOP_LEFT] = side_neighbor_lut[Cube::SIDE_TOP] + side_neighbor_lut[Cube::SIDE_LEFT];
	edge_neighbor_lut[Cube::EDGE_TOP_RIGHT] = side_neighbor_lut[Cube::SIDE_TOP] + side_neighbor_lut[Cube::SIDE_RIGHT];

	FixedArray<int, Cube::CORNER_COUNT> corner_neighbor_lut;

	corner_neighbor_lut[Cube::CORNER_BOTTOM_BACK_LEFT] = side_neighbor_lut[Cube::SIDE_BOTTOM] +
			side_neighbor_lut[Cube::SIDE_BACK] + side_neighbor_lut[Cube::SIDE_LEFT];

	corner_neighbor_lut[Cube::CORNER_BOTTOM_BACK_RIGHT] = side_neighbor_lut[Cube::SIDE_BOTTOM] +
			side_neighbor_lut[Cube::SIDE_BACK] + side_neighbor_lut[Cube::SIDE_RIGHT];

	corner_neighbor_lut[Cube::CORNER_BOTTOM_FRONT_RIGHT] = side_neighbor_lut[Cube::SIDE_BOTTOM] +
			side_neighbor_lut[Cube::SIDE_FRONT] + side_neighbor_lut[Cube::SIDE_RIGHT];

	corner_neighbor_lut[Cube::CORNER_BOTTOM_FRONT_LEFT] = side_neighbor_lut[Cube::SIDE_BOTTOM] +
			side_neighbor_lut[Cube::SIDE_FRONT] + side_neighbor_lut[Cube::SIDE_LEFT];

	corner_neighbor_lut[Cube::CORNER_TOP_BACK_LEFT] =
			side_neighbor_lut[Cube::SIDE_TOP] + side_neighbor_lut[Cube::SIDE_BACK] + side_neighbor_lut[Cube::SIDE_LEFT];

	corner_neighbor_lut[Cube::CORNER_TOP_BACK_RIGHT] = side_neighbor_lut[Cube::SIDE_TOP] +
			side_neighbor_lut[Cube::SIDE_BACK] + side_neighbor_lut[Cube::SIDE_RIGHT];

	corner_neighbor_lut[Cube::CORNER_TOP_FRONT_RIGHT] = side_neighbor_lut[Cube::SIDE_TOP] +
			side_neighbor_lut[Cube::SIDE_FRONT] + side_neighbor_lut[Cube::SIDE_RIGHT];

	corner_neighbor_lut[Cube::CORNER_TOP_FRONT_LEFT] = side_neighbor_lut[Cube::SIDE_TOP] +
			side_neighbor_lut[Cube::SIDE_FRONT] + side_neighbor_lut[Cube::SIDE_LEFT];

	// uint64_t time_prep = Time::get_singleton()->get_ticks_usec() - time_before;
	// time_before = Time::get_singleton()->get_ticks_usec();

	for (unsigned int z = min.z; z < (unsigned int)max.z; ++z) {
		for (unsigned int x = min.x; x < (unsigned int)max.x; ++x) {
			for (unsigned int y = min.y; y < (unsigned int)max.y; ++y) {
				// min and max are chosen such that you can visit 1 neighbor away from the current voxel without size
				// check

				const int voxel_index = y + x * row_size + z * deck_size;
				const int voxel_id = type_buffer[voxel_index];

				if (voxel_id == VoxelBlockyModel::AIR_ID || !library.has_model(voxel_id)) {
					continue;
				}

				const VoxelBlockyModel::BakedData &voxel = library.models[voxel_id];
				const VoxelBlockyModel::BakedData::Model &model = voxel.model;

				// Hybrid approach: extract cube faces and decimate those that aren't visible,
				// and still allow voxels to have geometry that is not a cube.

				// Sides
				for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {
					if ((model.empty_sides_mask & (1 << side)) != 0) {
						// This side is empty
						continue;
					}

					const uint32_t neighbor_voxel_id = type_buffer[voxel_index + side_neighbor_lut[side]];

					if (!is_face_visible(library, voxel, neighbor_voxel_id, side)) {
						continue;
					}

					// The face is visible

					int shaded_corner[8] = { 0 };

					if (bake_occlusion) {
						// Combinatory solution for
						// https://0fps.net/2013/07/03/ambient-occlusion-for-minecraft-like-worlds/ (inverted)
						//	function vertexAO(side1, side2, corner) {
						//	  if(side1 && side2) {
						//		return 0
						//	  }
						//	  return 3 - (side1 + side2 + corner)
						//	}

						for (unsigned int j = 0; j < 4; ++j) {
							const unsigned int edge = Cube::g_side_edges[side][j];
							const int edge_neighbor_id = type_buffer[voxel_index + edge_neighbor_lut[edge]];
							if (contributes_to_ao(library, edge_neighbor_id)) {
								++shaded_corner[Cube::g_edge_corners[edge][0]];
								++shaded_corner[Cube::g_edge_corners[edge][1]];
							}
						}
						for (unsigned int j = 0; j < 4; ++j) {
							const unsigned int corner = Cube::g_side_corners[side][j];
							if (shaded_corner[corner] == 2) {
								shaded_corner[corner] = 3;
							} else {
								const int corner_neigbor_id = type_buffer[voxel_index + corner_neighbor_lut[corner]];
								if (contributes_to_ao(library, corner_neigbor_id)) {
									++shaded_corner[corner];
								}
							}
						}
					}

					// Subtracting 1 because the data is padded
					const Vector3f pos(x - 1, y - 1, z - 1);

					for (unsigned int surface_index = 0; surface_index < model.surface_count; ++surface_index) {
						const VoxelBlockyModel::BakedData::Surface &surface = model.surfaces[surface_index];

						VoxelMesherBlocky::Arrays &arrays = out_arrays_per_material[surface.material_id];

						ZN_ASSERT(surface.material_id >= 0 && surface.material_id < index_offsets.size());
						int &index_offset = index_offsets[surface.material_id];

						const VoxelBlockyModel::BakedData::SideSurface &side_surface = surface.sides[side];

						const StdVector<Vector3f> &side_positions = side_surface.positions;
						const unsigned int vertex_count = side_surface.positions.size();

						const StdVector<Vector2f> &side_uvs = side_surface.uvs;
						const StdVector<float> &side_tangents = side_surface.tangents;

						// Append vertices of the faces in one go, don't use push_back

						{
							const int append_index = arrays.positions.size();
							arrays.positions.resize(arrays.positions.size() + vertex_count);
							Vector3f *w = arrays.positions.data() + append_index;
							for (unsigned int i = 0; i < vertex_count; ++i) {
								w[i] = side_positions[i] + pos;
							}
						}

						{
							const int append_index = arrays.uvs.size();
							arrays.uvs.resize(arrays.uvs.size() + vertex_count);
							memcpy(arrays.uvs.data() + append_index, side_uvs.data(), vertex_count * sizeof(Vector2f));
						}

						if (side_tangents.size() > 0) {
							const int append_index = arrays.tangents.size();
							arrays.tangents.resize(arrays.tangents.size() + vertex_count * 4);
							memcpy(arrays.tangents.data() + append_index,
								   side_tangents.data(),
								   (vertex_count * 4) * sizeof(float));
						}

						{
							const int append_index = arrays.normals.size();
							arrays.normals.resize(arrays.normals.size() + vertex_count);
							Vector3f *w = arrays.normals.data() + append_index;
							for (unsigned int i = 0; i < vertex_count; ++i) {
								w[i] = to_vec3f(Cube::g_side_normals[side]);
							}
						}

						{
							const int append_index = arrays.colors.size();
							arrays.colors.resize(arrays.colors.size() + vertex_count);
							Color *w = arrays.colors.data() + append_index;
							const Color modulate_color = voxel.color;

							if (bake_occlusion) {
								for (unsigned int i = 0; i < vertex_count; ++i) {
									const Vector3f vertex_pos = side_positions[i];

									// General purpose occlusion colouring.
									// TODO Optimize for cubes
									// TODO Fix occlusion inconsistency caused by triangles orientation? Not sure if
									// worth it
									float shade = 0;
									for (unsigned int j = 0; j < 4; ++j) {
										unsigned int corner = Cube::g_side_corners[side][j];
										if (shaded_corner[corner]) {
											float s = baked_occlusion_darkness *
													static_cast<float>(shaded_corner[corner]);
											// float k = 1.f - Cube::g_corner_position[corner].distance_to(v);
											float k = 1.f -
													math::distance_squared(Cube::g_corner_position[corner], vertex_pos);
											if (k < 0.0) {
												k = 0.0;
											}
											s *= k;
											if (s > shade) {
												shade = s;
											}
										}
									}
									const float gs = 1.0 - shade;
									w[i] = Color(gs, gs, gs) * modulate_color;
								}

							} else {
								for (unsigned int i = 0; i < vertex_count; ++i) {
									w[i] = modulate_color;
								}
							}
						}

						const StdVector<int> &side_indices = side_surface.indices;
						const unsigned int index_count = side_indices.size();

						{
							int i = arrays.indices.size();
							arrays.indices.resize(arrays.indices.size() + index_count);
							int *w = arrays.indices.data();
							for (unsigned int j = 0; j < index_count; ++j) {
								w[i++] = index_offset + side_indices[j];
							}
						}

						if (collision_surface != nullptr && surface.collision_enabled) {
							StdVector<Vector3f> &dst_positions = collision_surface->positions;
							StdVector<int> &dst_indices = collision_surface->indices;

							{
								const unsigned int append_index = dst_positions.size();
								dst_positions.resize(dst_positions.size() + vertex_count);
								Vector3f *w = dst_positions.data() + append_index;
								for (unsigned int i = 0; i < vertex_count; ++i) {
									w[i] = side_positions[i] + pos;
								}
							}

							{
								int i = dst_indices.size();
								dst_indices.resize(dst_indices.size() + index_count);
								int *w = dst_indices.data();
								for (unsigned int j = 0; j < index_count; ++j) {
									w[i++] = collision_surface_index_offset + side_indices[j];
								}
							}

							collision_surface_index_offset += vertex_count;
						}

						index_offset += vertex_count;
					}
				}

				// Inside
				for (unsigned int surface_index = 0; surface_index < model.surface_count; ++surface_index) {
					const VoxelBlockyModel::BakedData::Surface &surface = model.surfaces[surface_index];
					if (surface.positions.size() == 0) {
						continue;
					}
					// TODO Get rid of push_backs

					VoxelMesherBlocky::Arrays &arrays = out_arrays_per_material[surface.material_id];

					ZN_ASSERT(surface.material_id >= 0 && surface.material_id < index_offsets.size());
					int &index_offset = index_offsets[surface.material_id];

					const StdVector<Vector3f> &positions = surface.positions;
					const unsigned int vertex_count = positions.size();
					const Color modulate_color = voxel.color;

					const StdVector<Vector3f> &normals = surface.normals;
					const StdVector<Vector2f> &uvs = surface.uvs;
					const StdVector<float> &tangents = surface.tangents;

					const Vector3f pos(x - 1, y - 1, z - 1);

					if (tangents.size() > 0) {
						const int append_index = arrays.tangents.size();
						arrays.tangents.resize(arrays.tangents.size() + vertex_count * 4);
						memcpy(arrays.tangents.data() + append_index,
							   tangents.data(),
							   (vertex_count * 4) * sizeof(float));
					}

					for (unsigned int i = 0; i < vertex_count; ++i) {
						arrays.normals.push_back(normals[i]);
						arrays.uvs.push_back(uvs[i]);
						arrays.positions.push_back(positions[i] + pos);
						// TODO handle ambient occlusion on inner parts
						arrays.colors.push_back(modulate_color);
					}

					const StdVector<int> &indices = surface.indices;
					const unsigned int index_count = indices.size();

					for (unsigned int i = 0; i < index_count; ++i) {
						arrays.indices.push_back(index_offset + indices[i]);
					}

					if (collision_surface != nullptr && surface.collision_enabled) {
						StdVector<Vector3f> &dst_positions = collision_surface->positions;
						StdVector<int> &dst_indices = collision_surface->indices;

						for (unsigned int i = 0; i < vertex_count; ++i) {
							dst_positions.push_back(positions[i] + pos);
						}
						for (unsigned int i = 0; i < index_count; ++i) {
							dst_indices.push_back(collision_surface_index_offset + indices[i]);
						}

						collision_surface_index_offset += vertex_count;
					}

					index_offset += vertex_count;
				}
			}
		}
	}
}

struct OccluderArrays {
	StdVector<Vector3f> vertices;
	StdVector<int32_t> indices;
};

void generate_occluders_geometry(
		OccluderArrays &out_arrays,
		const Vector3f maxf,
		bool positive_x,
		bool positive_y,
		bool positive_z,
		bool negative_x,
		bool negative_y,
		bool negative_z
) {
	const unsigned int quad_count = //
			static_cast<unsigned int>(positive_x) + //
			static_cast<unsigned int>(positive_y) + //
			static_cast<unsigned int>(positive_z) + //
			static_cast<unsigned int>(negative_x) + //
			static_cast<unsigned int>(negative_y) + //
			static_cast<unsigned int>(negative_z);

	if (quad_count == 0) {
		return;
	}

	out_arrays.vertices.resize(out_arrays.vertices.size() + 4 * quad_count);
	out_arrays.indices.resize(out_arrays.indices.size() + 6 * quad_count);

	unsigned int vi0 = 0;
	unsigned int ii0 = 0;

	if (positive_x) {
		// 3---2  y
		// |   |  |
		// 0---1  x--z

		Span<Vector3f> vertices = to_span_from_position_and_size(out_arrays.vertices, 0, 4);
		Span<int32_t> indices = to_span_from_position_and_size(out_arrays.indices, 0, 6);

		vertices[0] = Vector3f(maxf.x, 0.f, 0.f);
		vertices[1] = Vector3f(maxf.x, 0.f, maxf.z);
		vertices[2] = Vector3f(maxf.x, maxf.y, maxf.z);
		vertices[3] = Vector3f(maxf.x, maxf.y, 0.f);

		indices[0] = 0;
		indices[1] = 1;
		indices[2] = 2;
		indices[3] = 0;
		indices[4] = 2;
		indices[5] = 3;

		vi0 += 4;
		ii0 += 6;
	}

	if (positive_y) {
		// 3---2  z
		// |   |  |
		// 0---1  y--x

		Span<Vector3f> vertices = to_span_from_position_and_size(out_arrays.vertices, vi0, 4);
		Span<int32_t> indices = to_span_from_position_and_size(out_arrays.indices, ii0, 6);

		vertices[0] = Vector3f(0.f, maxf.y, 0.f);
		vertices[1] = Vector3f(maxf.x, maxf.y, 0.f);
		vertices[2] = Vector3f(maxf.x, maxf.y, maxf.z);
		vertices[3] = Vector3f(0.f, maxf.y, maxf.z);

		indices[0] = vi0 + 0;
		indices[1] = vi0 + 1;
		indices[2] = vi0 + 2;
		indices[3] = vi0 + 0;
		indices[4] = vi0 + 2;
		indices[5] = vi0 + 3;

		vi0 += 4;
		ii0 += 6;
	}

	if (positive_z) {
		// 3---2  y
		// |   |  |
		// 0---1  z--x

		Span<Vector3f> vertices = to_span_from_position_and_size(out_arrays.vertices, vi0, 4);
		Span<int32_t> indices = to_span_from_position_and_size(out_arrays.indices, ii0, 6);

		vertices[0] = Vector3f(0.f, 0.f, maxf.z);
		vertices[1] = Vector3f(maxf.x, 0.f, maxf.z);
		vertices[2] = Vector3f(maxf.x, maxf.y, maxf.z);
		vertices[3] = Vector3f(0.f, maxf.y, maxf.z);

		indices[0] = vi0 + 0;
		indices[1] = vi0 + 2;
		indices[2] = vi0 + 1;
		indices[3] = vi0 + 0;
		indices[4] = vi0 + 3;
		indices[5] = vi0 + 2;

		vi0 += 4;
		ii0 += 6;
	}

	if (negative_x) {
		// 3---2  y
		// |   |  |
		// 0---1  x--z

		Span<Vector3f> vertices = to_span_from_position_and_size(out_arrays.vertices, vi0, 4);
		Span<int32_t> indices = to_span_from_position_and_size(out_arrays.indices, ii0, 6);

		vertices[0] = Vector3f(0.f, 0.f, 0.f);
		vertices[1] = Vector3f(0.f, 0.f, maxf.z);
		vertices[2] = Vector3f(0.f, maxf.y, maxf.z);
		vertices[3] = Vector3f(0.f, maxf.y, 0.f);

		indices[0] = vi0 + 0;
		indices[1] = vi0 + 2;
		indices[2] = vi0 + 1;
		indices[3] = vi0 + 0;
		indices[4] = vi0 + 3;
		indices[5] = vi0 + 2;

		vi0 += 4;
		ii0 += 6;
	}

	if (negative_y) {
		// 3---2  z
		// |   |  |
		// 0---1  y--x

		Span<Vector3f> vertices = to_span_from_position_and_size(out_arrays.vertices, vi0, 4);
		Span<int32_t> indices = to_span_from_position_and_size(out_arrays.indices, ii0, 6);

		vertices[0] = Vector3f(0.f, 0.f, 0.f);
		vertices[1] = Vector3f(maxf.x, 0.f, 0.f);
		vertices[2] = Vector3f(maxf.x, 0.f, maxf.z);
		vertices[3] = Vector3f(0.f, 0.f, maxf.z);

		indices[0] = vi0 + 0;
		indices[1] = vi0 + 2;
		indices[2] = vi0 + 1;
		indices[3] = vi0 + 0;
		indices[4] = vi0 + 3;
		indices[5] = vi0 + 2;

		vi0 += 4;
		ii0 += 6;
	}

	if (negative_z) {
		// 3---2  y
		// |   |  |
		// 0---1  z--x

		Span<Vector3f> vertices = to_span_from_position_and_size(out_arrays.vertices, vi0, 4);
		Span<int32_t> indices = to_span_from_position_and_size(out_arrays.indices, ii0, 6);

		vertices[0] = Vector3f(0.f, 0.f, 0.f);
		vertices[1] = Vector3f(maxf.x, 0.f, 0.f);
		vertices[2] = Vector3f(maxf.x, maxf.y, 0.f);
		vertices[3] = Vector3f(0.f, maxf.y, 0.f);

		indices[0] = vi0 + 0;
		indices[1] = vi0 + 1;
		indices[2] = vi0 + 2;
		indices[3] = vi0 + 0;
		indices[4] = vi0 + 2;
		indices[5] = vi0 + 3;
	}
}

template <typename TModelID>
void classify_chunk_occlusion_from_voxels(
		Span<const TModelID> id_buffer,
		const VoxelBlockyLibraryBase::BakedData &baked_data,
		const Vector3i block_size,
		const Vector3i min,
		const Vector3i max,
		const uint8_t enabled_mask,
		bool &out_positive_x,
		bool &out_positive_y,
		bool &out_positive_z,
		bool &out_negative_x,
		bool &out_negative_y,
		bool &out_negative_z
) {
	struct L {
		static inline bool is_fully_occluded(
				const TModelID v0,
				const TModelID v1,
				const VoxelBlockyLibraryBase::BakedData &baked_data,
				const uint8_t side0_mask,
				const uint8_t side1_mask
		) {
			if (v0 >= baked_data.models.size() || v1 >= baked_data.models.size()) {
				return false;
			}
			const VoxelBlockyModel::BakedData &b0 = baked_data.models[v0];
			const VoxelBlockyModel::BakedData &b1 = baked_data.models[v1];
			if (b0.empty || b1.empty) {
				return false;
			}
			if (b0.transparency_index > 0 || b1.transparency_index > 0) {
				// Either side is transparent
				return false;
			}
			if ((b0.model.full_sides_mask & side0_mask) == 0) {
				return false;
			}
			if ((b1.model.full_sides_mask & side1_mask) == 0) {
				return false;
			}
			return true;
		}

		static bool is_chunk_side_occluded_x(
				const Vector3i min,
				const Vector3i max,
				Span<const TModelID> id_buffer,
				const Vector3i block_size,
				const int sign,
				const VoxelBlockyLibraryBase::BakedData &baked_data
		) {
			const Cube::SideAxis side0 = sign < 0 ? Cube::SIDE_NEGATIVE_X : Cube::SIDE_POSITIVE_X;
			const Cube::SideAxis side1 = sign < 0 ? Cube::SIDE_POSITIVE_X : Cube::SIDE_NEGATIVE_X;
			const uint8_t side0_mask = (1 << side0);
			const uint8_t side1_mask = (1 << side1);

			Vector3i pos(sign < 0 ? min.x : max.x - 1, 0, 0);
			for (pos.z = min.z; pos.z < max.z; ++pos.z) {
				for (pos.y = min.y; pos.y < max.y; ++pos.y) {
					const unsigned int loc0 = Vector3iUtil::get_zxy_index(pos, block_size);
					const unsigned int loc1 = Vector3iUtil::get_zxy_index(pos + Vector3i(sign, 0, 0), block_size);

					const TModelID v0 = id_buffer[loc0];
					const TModelID v1 = id_buffer[loc1];

					if (!is_fully_occluded(v0, v1, baked_data, side0_mask, side1_mask)) {
						return false;
					}
				}
			}
			return true;
		}

		static bool is_chunk_side_occluded_y(
				const Vector3i min,
				const Vector3i max,
				Span<const TModelID> id_buffer,
				const Vector3i block_size,
				const int sign,
				const VoxelBlockyLibraryBase::BakedData &baked_data
		) {
			const Cube::SideAxis side0 = sign < 0 ? Cube::SIDE_NEGATIVE_Y : Cube::SIDE_POSITIVE_Y;
			const Cube::SideAxis side1 = sign < 0 ? Cube::SIDE_POSITIVE_Y : Cube::SIDE_NEGATIVE_Y;
			const uint8_t side0_mask = (1 << side0);
			const uint8_t side1_mask = (1 << side1);

			Vector3i pos(0, sign < 0 ? min.y : max.y - 1, 0);
			for (pos.z = min.z; pos.z < max.z; ++pos.z) {
				for (pos.x = min.x; pos.x < max.x; ++pos.x) {
					const unsigned int loc0 = Vector3iUtil::get_zxy_index(pos, block_size);
					const unsigned int loc1 = Vector3iUtil::get_zxy_index(pos + Vector3i(0, sign, 0), block_size);

					const TModelID v0 = id_buffer[loc0];
					const TModelID v1 = id_buffer[loc1];

					if (!is_fully_occluded(v0, v1, baked_data, side0_mask, side1_mask)) {
						return false;
					}
				}
			}
			return true;
		}

		static bool is_chunk_side_occluded_z(
				const Vector3i min,
				const Vector3i max,
				Span<const TModelID> id_buffer,
				const Vector3i block_size,
				const int sign,
				const VoxelBlockyLibraryBase::BakedData &baked_data
		) {
			const Cube::SideAxis side0 = sign < 0 ? Cube::SIDE_NEGATIVE_Z : Cube::SIDE_POSITIVE_Z;
			const Cube::SideAxis side1 = sign < 0 ? Cube::SIDE_POSITIVE_Z : Cube::SIDE_NEGATIVE_Z;
			const uint8_t side0_mask = (1 << side0);
			const uint8_t side1_mask = (1 << side1);

			Vector3i pos(0, 0, sign < 0 ? min.z : max.z - 1);
			for (pos.x = min.x; pos.x < max.x; ++pos.x) {
				for (pos.y = min.y; pos.y < max.y; ++pos.y) {
					const unsigned int loc0 = Vector3iUtil::get_zxy_index(pos, block_size);
					const unsigned int loc1 = Vector3iUtil::get_zxy_index(pos + Vector3i(0, 0, sign), block_size);

					const TModelID v0 = id_buffer[loc0];
					const TModelID v1 = id_buffer[loc1];

					if (!is_fully_occluded(v0, v1, baked_data, side0_mask, side1_mask)) {
						return false;
					}
				}
			}
			return true;
		}
	};

	const bool positive_x_enabled = (enabled_mask & (1 << VoxelMesherBlocky::SIDE_POSITIVE_X)) != 0;
	const bool positive_y_enabled = (enabled_mask & (1 << VoxelMesherBlocky::SIDE_POSITIVE_Y)) != 0;
	const bool positive_z_enabled = (enabled_mask & (1 << VoxelMesherBlocky::SIDE_POSITIVE_Z)) != 0;

	const bool negative_x_enabled = (enabled_mask & (1 << VoxelMesherBlocky::SIDE_NEGATIVE_X)) != 0;
	const bool negative_y_enabled = (enabled_mask & (1 << VoxelMesherBlocky::SIDE_NEGATIVE_Y)) != 0;
	const bool negative_z_enabled = (enabled_mask & (1 << VoxelMesherBlocky::SIDE_NEGATIVE_Z)) != 0;

	out_positive_x = positive_x_enabled && L::is_chunk_side_occluded_x(min, max, id_buffer, block_size, 1, baked_data);
	out_positive_y = positive_y_enabled && L::is_chunk_side_occluded_y(min, max, id_buffer, block_size, 1, baked_data);
	out_positive_z = positive_z_enabled && L::is_chunk_side_occluded_z(min, max, id_buffer, block_size, 1, baked_data);

	out_negative_x = negative_x_enabled && L::is_chunk_side_occluded_x(min, max, id_buffer, block_size, -1, baked_data);
	out_negative_y = negative_y_enabled && L::is_chunk_side_occluded_y(min, max, id_buffer, block_size, -1, baked_data);
	out_negative_z = negative_z_enabled && L::is_chunk_side_occluded_z(min, max, id_buffer, block_size, -1, baked_data);
}

void generate_shadow_occluders(
		OccluderArrays &out_arrays,
		const Span<const uint8_t> id_buffer_raw,
		const VoxelBuffer::Depth depth,
		const Vector3i block_size,
		const VoxelBlockyLibraryBase::BakedData &baked_data,
		const uint8_t enabled_mask
) {
	ZN_PROFILE_SCOPE();

	// Data must be padded, hence the off-by-one
	const Vector3i min = Vector3iUtil::create(VoxelMesherBlocky::PADDING);
	const Vector3i max = block_size - Vector3iUtil::create(VoxelMesherBlocky::PADDING);

	// Not doing only positive sides, because it allows to self-contain the calculation in the chunk.
	// Doing only positive sides requires to also do this when the main mesh is actually empty, which means we would end
	// up with lots of useless occluders across all space instead of just near chunks that actually have meshes. That
	// would in turn require to lookup neighbor chunks, which introduces dependencies that complicate multi-threading
	// and sorting.

	// Rendering idea: instead of baking this into every mesh, batch all occluders into a single mesh? Then rebuild it
	// at most once per frame if there was any change? Means we have to store occlusion info somewhere that is fast
	// to access.

	bool positive_x;
	bool positive_y;
	bool positive_z;
	bool negative_x;
	bool negative_y;
	bool negative_z;

	switch (depth) {
		case VoxelBuffer::DEPTH_8_BIT:
			classify_chunk_occlusion_from_voxels(
					id_buffer_raw,
					baked_data,
					block_size,
					min,
					max,
					enabled_mask,
					positive_x,
					positive_y,
					positive_z,
					negative_x,
					negative_y,
					negative_z
			);
			break;

		case VoxelBuffer::DEPTH_16_BIT:
			classify_chunk_occlusion_from_voxels(
					id_buffer_raw.reinterpret_cast_to<const uint16_t>(),
					baked_data,
					block_size,
					min,
					max,
					enabled_mask,
					positive_x,
					positive_y,
					positive_z,
					negative_x,
					negative_y,
					negative_z
			);
			break;

		default:
			ERR_PRINT("Unsupported voxel depth");
			return;
	}

	generate_occluders_geometry(
			out_arrays, to_vec3f(max - min), positive_x, positive_y, positive_z, negative_x, negative_y, negative_z
	);
}

bool is_empty(const StdVector<VoxelMesherBlocky::Arrays> &arrays_per_material) {
	for (const VoxelMesherBlocky::Arrays &arrays : arrays_per_material) {
		if (arrays.indices.size() > 0) {
			return false;
		}
	}
	return true;
}

Vector3f side_to_block_coordinates(const Vector3f v, const VoxelBlockyModel::Side side) {
	switch (side) {
		case VoxelBlockyModel::SIDE_NEGATIVE_X:
		case VoxelBlockyModel::SIDE_POSITIVE_X:
			return v.zyx();
		case VoxelBlockyModel::SIDE_NEGATIVE_Y:
		case VoxelBlockyModel::SIDE_POSITIVE_Y:
			// return v.zxy();
			return v.yzx();
		case VoxelBlockyModel::SIDE_NEGATIVE_Z:
		case VoxelBlockyModel::SIDE_POSITIVE_Z:
			return v;
		default:
			ZN_CRASH();
			return v;
	}
}

int get_side_sign(const VoxelBlockyModel::Side side) {
	switch (side) {
		case VoxelBlockyModel::SIDE_NEGATIVE_X:
		case VoxelBlockyModel::SIDE_NEGATIVE_Y:
		case VoxelBlockyModel::SIDE_NEGATIVE_Z:
			return -1;
		case VoxelBlockyModel::SIDE_POSITIVE_X:
		case VoxelBlockyModel::SIDE_POSITIVE_Y:
		case VoxelBlockyModel::SIDE_POSITIVE_Z:
			return 1;
		default:
			ZN_CRASH();
			return 1;
	}
}

// Adds extra voxel side geometry on the sides of the chunk for every voxel exposed to air. This creates
// "seams" that hide LOD cracks when meshes of different LOD are put next to each other.
// This method doesn't require to access voxels of the child LOD. The downside is that it won't always hide all the
// cracks, but the assumption is that it will do most of the time.
// AO is not handled, and probably doesn't need to be
template <typename TModelID>
void append_side_seams(
		Span<const TModelID> buffer,
		const Vector3T<int> jump,
		const int z, // Coordinate of the first or last voxel (not within the padded region)
		const int size_x,
		const int size_y,
		const VoxelBlockyModel::Side side,
		const VoxelBlockyLibraryBase::BakedData &library,
		StdVector<VoxelMesherBlocky::Arrays> &out_arrays_per_material
) {
	constexpr TModelID AIR = 0;
	constexpr int pad = 1;

	const int z_base = z * jump.z;
	const int side_sign = get_side_sign(side);

	// Buffers sent to chunk meshing have outer and inner voxels.
	// Inner voxels are those that are actually being meshed.
	// Outer voxels are not made part of the final mesh, but they exist to know how to occlude sides of inner voxels
	// touching them.

	// For each outer voxel on the side of the chunk (using side-relative coordinates)
	for (int x = pad; x < size_x - pad; ++x) {
		for (int y = pad; y < size_y - pad; ++y) {
			const int buffer_index = x * jump.x + y * jump.y + z_base;
			const int v = buffer[buffer_index];

			if (v == AIR) {
				continue;
			}

			// Check if the voxel is exposed to air

			const int nv0 = buffer[buffer_index - jump.x];
			const int nv1 = buffer[buffer_index + jump.x];
			const int nv2 = buffer[buffer_index - jump.y];
			const int nv3 = buffer[buffer_index + jump.y];

			if (nv0 != AIR && nv1 != AIR && nv2 != AIR && nv3 != AIR) {
				continue;
			}

			// Check if the outer voxel occludes an inner voxel
			// (this check is not actually accurate, maybe we'd have to do a full occlusion check using the library?)

			const int nv4 = buffer[buffer_index - side_sign * jump.z];
			if (nv4 == AIR) {
				continue;
			}

			// If it does, add geometry for the side of that inner voxel

			const Vector3f pos = side_to_block_coordinates(Vector3f(x - pad, y - pad, z - (side_sign + 1)), side);

			const VoxelBlockyModel::BakedData &voxel = library.models[nv4];
			const VoxelBlockyModel::BakedData::Model &model = voxel.model;

			for (unsigned int surface_index = 0; surface_index < model.surface_count; ++surface_index) {
				const VoxelBlockyModel::BakedData::Surface &surface = model.surfaces[surface_index];
				const VoxelBlockyModel::BakedData::SideSurface &side_surface = surface.sides[side];
				const unsigned int vertex_count = side_surface.positions.size();

				VoxelMesherBlocky::Arrays &arrays = out_arrays_per_material[surface.material_id];

				// TODO The following code is pretty much the same as the main meshing function.
				// We should put it in common once blocky mesher features are merged (blocky fluids, shadows occluders).
				// The baked occlusion part should be separated to run on top of color modulate.
				// Index offsets might not need a vector after all.

				const unsigned int index_offset = arrays.positions.size();

				{
					const unsigned int append_index = arrays.positions.size();
					arrays.positions.resize(arrays.positions.size() + vertex_count);
					Vector3f *w = arrays.positions.data() + append_index;
					for (unsigned int i = 0; i < vertex_count; ++i) {
						w[i] = side_surface.positions[i] + pos;
					}
				}

				{
					const unsigned int append_index = arrays.uvs.size();
					arrays.uvs.resize(arrays.uvs.size() + vertex_count);
					memcpy(arrays.uvs.data() + append_index, side_surface.uvs.data(), vertex_count * sizeof(Vector2f));
				}

				if (side_surface.tangents.size() > 0) {
					const unsigned int append_index = arrays.tangents.size();
					arrays.tangents.resize(arrays.tangents.size() + vertex_count * 4);
					memcpy(arrays.tangents.data() + append_index,
						   side_surface.tangents.data(),
						   (vertex_count * 4) * sizeof(float));
				}

				{
					const int append_index = arrays.normals.size();
					arrays.normals.resize(arrays.normals.size() + vertex_count);
					Vector3f *w = arrays.normals.data() + append_index;
					for (unsigned int i = 0; i < vertex_count; ++i) {
						w[i] = to_vec3f(Cube::g_side_normals[side]);
					}
				}

				{
					const int append_index = arrays.colors.size();
					arrays.colors.resize(arrays.colors.size() + vertex_count);
					Color *w = arrays.colors.data() + append_index;
					const Color modulate_color = voxel.color;

					for (unsigned int i = 0; i < vertex_count; ++i) {
						w[i] = modulate_color;
					}
				}

				{
					const unsigned int index_count = side_surface.indices.size();
					unsigned int i = arrays.indices.size();
					arrays.indices.resize(arrays.indices.size() + index_count);
					int *w = arrays.indices.data();
					for (unsigned int j = 0; j < index_count; ++j) {
						w[i++] = index_offset + side_surface.indices[j];
					}
				}
			}
		}
	}
}

template <typename TModelID>
void append_seams(
		Span<const TModelID> buffer,
		const Vector3i size,
		StdVector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		const VoxelBlockyLibraryBase::BakedData &library
) {
	ZN_PROFILE_SCOPE();

	const Vector3T<int> jump(size.y, 1, size.x * size.y);

	// Shortcuts
	StdVector<VoxelMesherBlocky::Arrays> &out = out_arrays_per_material;
	constexpr VoxelBlockyModel::Side NEGATIVE_X = VoxelBlockyModel::SIDE_NEGATIVE_X;
	constexpr VoxelBlockyModel::Side POSITIVE_X = VoxelBlockyModel::SIDE_POSITIVE_X;
	constexpr VoxelBlockyModel::Side NEGATIVE_Y = VoxelBlockyModel::SIDE_NEGATIVE_Y;
	constexpr VoxelBlockyModel::Side POSITIVE_Y = VoxelBlockyModel::SIDE_POSITIVE_Y;
	constexpr VoxelBlockyModel::Side NEGATIVE_Z = VoxelBlockyModel::SIDE_NEGATIVE_Z;
	constexpr VoxelBlockyModel::Side POSITIVE_Z = VoxelBlockyModel::SIDE_POSITIVE_Z;

	append_side_seams(buffer, jump.xyz(), 0, size.x, size.y, NEGATIVE_Z, library, out);
	append_side_seams(buffer, jump.xyz(), (size.z - 1), size.x, size.y, POSITIVE_Z, library, out);
	append_side_seams(buffer, jump.zyx(), 0, size.z, size.y, NEGATIVE_X, library, out);
	append_side_seams(buffer, jump.zyx(), (size.x - 1), size.z, size.y, POSITIVE_X, library, out);
	append_side_seams(buffer, jump.zxy(), 0, size.z, size.x, NEGATIVE_Y, library, out);
	append_side_seams(buffer, jump.zxy(), (size.y - 1), size.z, size.x, POSITIVE_Y, library, out);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelMesherBlocky::VoxelMesherBlocky() {
	set_padding(PADDING, PADDING);
}

VoxelMesherBlocky::~VoxelMesherBlocky() {}

VoxelMesherBlocky::Cache &VoxelMesherBlocky::get_tls_cache() {
	thread_local Cache cache;
	return cache;
}

void VoxelMesherBlocky::set_library(Ref<VoxelBlockyLibraryBase> library) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.library = library;
}

Ref<VoxelBlockyLibraryBase> VoxelMesherBlocky::get_library() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.library;
}

void VoxelMesherBlocky::set_occlusion_darkness(float darkness) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.baked_occlusion_darkness = math::clamp(darkness, 0.0f, 1.0f);
}

float VoxelMesherBlocky::get_occlusion_darkness() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.baked_occlusion_darkness;
}

void VoxelMesherBlocky::set_occlusion_enabled(bool enable) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.bake_occlusion = enable;
}

bool VoxelMesherBlocky::get_occlusion_enabled() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.bake_occlusion;
}

void VoxelMesherBlocky::set_shadow_occluder_side(Side side, bool enabled) {
	RWLockWrite wlock(_parameters_lock);
	if (enabled) {
		_parameters.shadow_occluders_mask |= (1 << side);
	} else {
		_parameters.shadow_occluders_mask &= ~(1 << side);
	}
}

bool VoxelMesherBlocky::get_shadow_occluder_side(Side side) const {
	RWLockRead rlock(_parameters_lock);
	return (_parameters.shadow_occluders_mask & (1 << side)) != 0;
}

uint8_t VoxelMesherBlocky::get_shadow_occluder_mask() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.shadow_occluders_mask;
}

void VoxelMesherBlocky::build(VoxelMesher::Output &output, const VoxelMesher::Input &input) {
	const VoxelBuffer::ChannelId channel = VoxelBuffer::CHANNEL_TYPE;
	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	if (params.library.is_null()) {
		// This may be a configuration warning, the mesh will be left empty.
		// If it was an error it would spam unnecessarily in the editor as users set things up.
		return;
	}
	// ERR_FAIL_COND(params.library.is_null());

	Cache &cache = get_tls_cache();

	StdVector<Arrays> &arrays_per_material = cache.arrays_per_material;
	for (unsigned int i = 0; i < arrays_per_material.size(); ++i) {
		Arrays &a = arrays_per_material[i];
		a.clear();
	}

	float baked_occlusion_darkness = 0;
	if (params.bake_occlusion) {
		baked_occlusion_darkness = params.baked_occlusion_darkness / 3.0f;
	}

	// The technique is Culled faces.
	// Could be improved with greedy meshing: https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/
	// However I don't feel it's worth it yet:
	// - Not so much gain for organic worlds with lots of texture variations
	// - Works well with cubes but not with any shape
	// - Slower
	// => Could be implemented in a separate class?

	const VoxelBuffer &voxels = input.voxels;

	// Iterate 3D padded data to extract voxel faces.
	// This is the most intensive job in this class, so all required data should be as fit as possible.

	// The buffer we receive MUST be dense (i.e not compressed, and channels allocated).
	// That means we can use raw pointers to voxel data inside instead of using the higher-level getters,
	// and then save a lot of time.

	if (voxels.get_channel_compression(channel) == VoxelBuffer::COMPRESSION_UNIFORM) {
		// All voxels have the same type.
		// If it's all air, nothing to do. If it's all cubes, nothing to do either.
		// TODO Handle edge case of uniform block with non-cubic voxels!
		// If the type of voxel still produces geometry in this situation (which is an absurd use case but not an
		// error), decompress into a backing array to still allow the use of the same algorithm.
		return;

	} else if (voxels.get_channel_compression(channel) != VoxelBuffer::COMPRESSION_NONE) {
		// No other form of compression is allowed
		ERR_PRINT("VoxelMesherBlocky received unsupported voxel compression");
		return;
	}

	Span<const uint8_t> raw_channel;
	if (!voxels.get_channel_as_bytes_read_only(channel, raw_channel)) {
		// Case supposedly handled before...
		ERR_PRINT("Something wrong happened");
		return;
	}

	const Vector3i block_size = voxels.get_size();
	const VoxelBuffer::Depth channel_depth = voxels.get_channel_depth(channel);

	VoxelMesher::Output::CollisionSurface *collision_surface = nullptr;
	if (input.collision_hint) {
		collision_surface = &output.collision_surface;
	}

	unsigned int material_count = 0;
	{
		// We can only access baked data. Only this data is made for multithreaded access.
		RWLockRead lock(params.library->get_baked_data_rw_lock());
		const VoxelBlockyLibraryBase::BakedData &library_baked_data = params.library->get_baked_data();

		material_count = library_baked_data.indexed_materials_count;

		if (arrays_per_material.size() < material_count) {
			arrays_per_material.resize(material_count);
		}

		switch (channel_depth) {
			case VoxelBuffer::DEPTH_8_BIT:
				generate_blocky_mesh( //
						arrays_per_material, //
						collision_surface, //
						raw_channel, //
						block_size, //
						library_baked_data, //
						params.bake_occlusion, //
						baked_occlusion_darkness //
				);
				if (input.lod_index > 0) {
					append_seams(raw_channel, block_size, arrays_per_material, library_baked_data);
				}
				break;

			case VoxelBuffer::DEPTH_16_BIT: {
				Span<const uint16_t> model_ids = raw_channel.reinterpret_cast_to<const uint16_t>();
				generate_blocky_mesh(
						arrays_per_material,
						collision_surface,
						model_ids,
						block_size,
						library_baked_data,
						params.bake_occlusion,
						baked_occlusion_darkness
				);
				if (input.lod_index > 0) {
					append_seams(model_ids, block_size, arrays_per_material, library_baked_data);
				}
			} break;

			default:
				ERR_PRINT("Unsupported voxel depth");
				return;
		}
	}

	if (input.lod_index > 0) {
		// Might not look good, but at least it's something
		const float lod_scale = 1 << input.lod_index;
		for (Arrays &arrays : arrays_per_material) {
			for (Vector3f &p : arrays.positions) {
				p = p * lod_scale;
			}
		}
		if (collision_surface != nullptr) {
			for (Vector3f &p : collision_surface->positions) {
				p = p * lod_scale;
			}
		}
	}

	// TODO Optimization: we could return a single byte array and use Mesh::add_surface down the line?
	// That API does not seem to exist yet though.

	for (unsigned int material_index = 0; material_index < material_count; ++material_index) {
		const Arrays &arrays = arrays_per_material[material_index];

		if (arrays.positions.size() != 0) {
			Array mesh_arrays;
			mesh_arrays.resize(Mesh::ARRAY_MAX);

			{
				PackedVector3Array positions;
				PackedVector2Array uvs;
				PackedVector3Array normals;
				PackedColorArray colors;
				PackedInt32Array indices;

				copy_to(positions, arrays.positions);
				copy_to(uvs, arrays.uvs);
				copy_to(normals, arrays.normals);
				copy_to(colors, arrays.colors);
				copy_to(indices, arrays.indices);

				mesh_arrays[Mesh::ARRAY_VERTEX] = positions;
				mesh_arrays[Mesh::ARRAY_TEX_UV] = uvs;
				mesh_arrays[Mesh::ARRAY_NORMAL] = normals;
				mesh_arrays[Mesh::ARRAY_COLOR] = colors;
				mesh_arrays[Mesh::ARRAY_INDEX] = indices;

				if (arrays.tangents.size() > 0) {
					PackedFloat32Array tangents;
					copy_to(tangents, arrays.tangents);
					mesh_arrays[Mesh::ARRAY_TANGENT] = tangents;
				}
			}

			output.surfaces.push_back(Output::Surface());
			Output::Surface &surface = output.surfaces.back();
			surface.arrays = mesh_arrays;
			surface.material_index = material_index;
		}
		//  else {
		// 	// Empty
		// 	output.surfaces.push_back(Output::Surface());
		// }
	}

	if (params.shadow_occluders_mask != 0 && !is_empty(arrays_per_material)) {
		// TODO Candidate for temp allocator (maybe even stack allocator in this case?)
		OccluderArrays occluder_arrays;

		RWLockRead lock(params.library->get_baked_data_rw_lock());
		const VoxelBlockyLibraryBase::BakedData &library_baked_data = params.library->get_baked_data();

		generate_shadow_occluders( //
				occluder_arrays, //
				raw_channel, //
				channel_depth, //
				block_size, //
				library_baked_data, //
				params.shadow_occluders_mask //
		);

		if (input.lod_index > 0) {
			const float lod_scale = 1 << input.lod_index;
			for (Vector3f &p : occluder_arrays.vertices) {
				p *= lod_scale;
			}
		}

		if (occluder_arrays.indices.size() > 0) {
			Array mesh_arrays;
			mesh_arrays.resize(Mesh::ARRAY_MAX);

			{
				PackedVector3Array vertices;
				PackedInt32Array indices;

				copy_to(vertices, occluder_arrays.vertices);
				copy_to(indices, occluder_arrays.indices);

				mesh_arrays[Mesh::ARRAY_VERTEX] = vertices;
				mesh_arrays[Mesh::ARRAY_INDEX] = indices;
			}

			output.shadow_occluder = mesh_arrays;
			// output.surfaces.push_back(Output::Surface());
			// Output::Surface &surface = output.surfaces.back();
			// surface.arrays = mesh_arrays;
			// surface.material_index = 0;
		}
	}

	output.primitive_type = Mesh::PRIMITIVE_TRIANGLES;
}

Ref<Resource> VoxelMesherBlocky::duplicate(bool p_subresources) const {
	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	if (p_subresources && params.library.is_valid()) {
		params.library = params.library->duplicate(true);
	}

	Ref<VoxelMesherBlocky> c;
	c.instantiate();
	c->_parameters = params;
	return c;
}

int VoxelMesherBlocky::get_used_channels_mask() const {
	return (1 << VoxelBuffer::CHANNEL_TYPE);
}

Ref<Material> VoxelMesherBlocky::get_material_by_index(unsigned int index) const {
	Ref<VoxelBlockyLibraryBase> lib = get_library();
	if (lib.is_null()) {
		return Ref<Material>();
	}
	return lib->get_material_by_index(index);
}

unsigned int VoxelMesherBlocky::get_material_index_count() const {
	Ref<VoxelBlockyLibraryBase> lib = get_library();
	if (lib.is_null()) {
		return 0;
	}
	return lib->get_material_index_count();
}

#ifdef TOOLS_ENABLED

void VoxelMesherBlocky::get_configuration_warnings(PackedStringArray &out_warnings) const {
	Ref<VoxelBlockyLibraryBase> library = get_library();

	if (library.is_null()) {
		out_warnings.append(String(ZN_TTR("{0} has no {1} assigned."))
									.format(
											varray(VoxelMesherBlocky::get_class_static(),
												   VoxelBlockyLibraryBase::get_class_static())
									));
		return;
	}

	const VoxelBlockyLibraryBase::BakedData &baked_data = library->get_baked_data();
	RWLockRead rlock(library->get_baked_data_rw_lock());

	if (baked_data.models.size() == 0) {
		out_warnings.append(String(ZN_TTR("The {0} assigned to {1} has no baked models."))
									.format(varray(library->get_class(), VoxelMesherBlocky::get_class_static())));
		return;
	}

	library->get_configuration_warnings(out_warnings);
}

#endif // TOOLS_ENABLED

void VoxelMesherBlocky::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_library", "voxel_library"), &VoxelMesherBlocky::set_library);
	ClassDB::bind_method(D_METHOD("get_library"), &VoxelMesherBlocky::get_library);

	ClassDB::bind_method(D_METHOD("set_occlusion_enabled", "enable"), &VoxelMesherBlocky::set_occlusion_enabled);
	ClassDB::bind_method(D_METHOD("get_occlusion_enabled"), &VoxelMesherBlocky::get_occlusion_enabled);

	ClassDB::bind_method(D_METHOD("set_occlusion_darkness", "value"), &VoxelMesherBlocky::set_occlusion_darkness);
	ClassDB::bind_method(D_METHOD("get_occlusion_darkness"), &VoxelMesherBlocky::get_occlusion_darkness);

	ClassDB::bind_method(
			D_METHOD("set_shadow_occluder_side", "side", "enabled"), &VoxelMesherBlocky::set_shadow_occluder_side
	);
	ClassDB::bind_method(D_METHOD("get_shadow_occluder_side", "side"), &VoxelMesherBlocky::get_shadow_occluder_side);

	ADD_PROPERTY(
			PropertyInfo(
					Variant::OBJECT,
					"library",
					PROPERTY_HINT_RESOURCE_TYPE,
					VoxelBlockyLibraryBase::get_class_static(),
					PROPERTY_USAGE_DEFAULT
					// Sadly we can't use this hint because the property type is abstract... can't just choose a
					// default child class. This hint becomes less and less useful everytime I come across it...
					//| PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT
			),
			"set_library",
			"get_library"
	);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "occlusion_enabled"), "set_occlusion_enabled", "get_occlusion_enabled");
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "occlusion_darkness", PROPERTY_HINT_RANGE, "0,1,0.01"),
			"set_occlusion_darkness",
			"get_occlusion_darkness"
	);

	ADD_GROUP("Shadow Occluders", "shadow_occluder_");

#define ADD_SHADOW_OCCLUDER_PROPERTY(m_name, m_flag)                                                                   \
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, m_name), "set_shadow_occluder_side", "get_shadow_occluder_side", m_flag);

	ADD_SHADOW_OCCLUDER_PROPERTY("shadow_occluder_negative_x", SIDE_NEGATIVE_X);
	ADD_SHADOW_OCCLUDER_PROPERTY("shadow_occluder_positive_x", SIDE_POSITIVE_X);
	ADD_SHADOW_OCCLUDER_PROPERTY("shadow_occluder_negative_y", SIDE_NEGATIVE_Y);
	ADD_SHADOW_OCCLUDER_PROPERTY("shadow_occluder_positive_y", SIDE_POSITIVE_Y);
	ADD_SHADOW_OCCLUDER_PROPERTY("shadow_occluder_negative_z", SIDE_NEGATIVE_Z);
	ADD_SHADOW_OCCLUDER_PROPERTY("shadow_occluder_positive_z", SIDE_POSITIVE_Z);

	BIND_ENUM_CONSTANT(SIDE_NEGATIVE_X);
	BIND_ENUM_CONSTANT(SIDE_POSITIVE_X);
	BIND_ENUM_CONSTANT(SIDE_NEGATIVE_Y);
	BIND_ENUM_CONSTANT(SIDE_POSITIVE_Y);
	BIND_ENUM_CONSTANT(SIDE_NEGATIVE_Z);
	BIND_ENUM_CONSTANT(SIDE_POSITIVE_Z);
}

} // namespace zylann::voxel
