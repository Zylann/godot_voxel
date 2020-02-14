#include "voxel_mesher_transvoxel.h"
#include "transvoxel_tables.cpp"
#include <core/os/os.h>

namespace {

static const float TRANSITION_CELL_SCALE = 0.25;
static const unsigned int MESH_COMPRESSION_FLAGS =
		Mesh::ARRAY_COMPRESS_NORMAL |
		Mesh::ARRAY_COMPRESS_TANGENT |
		//Mesh::ARRAY_COMPRESS_COLOR | // Using color as 4 full floats to transfer extra attributes for now...
		Mesh::ARRAY_COMPRESS_TEX_UV |
		Mesh::ARRAY_COMPRESS_TEX_UV2 |
		Mesh::ARRAY_COMPRESS_WEIGHTS;

// Multiply integer math results by this
static const float FIXED_FACTOR = 1.f / 256.f;

inline float tof(int8_t v) {
	return static_cast<float>(v) / 256.f;
}

inline int8_t tos(uint8_t v) {
	return v - 128;
}

// Values considered negative have a sign bit of 1
inline uint8_t sign(int8_t v) {
	return (v >> 7) & 1;
}

// Wrapped to invert SDF data, Transvoxel apparently works backwards?
inline uint8_t get_voxel(const VoxelBuffer &vb, int x, int y, int z, int channel) {
	return 255 - vb.get_voxel(x, y, z, channel);
}

inline uint8_t get_voxel(const VoxelBuffer &vb, Vector3i pos, int channel) {
	return get_voxel(vb, pos.x, pos.y, pos.z, channel);
}

Vector3 get_border_offset(const Vector3 pos, const int lod_index, const Vector3i block_size) {

	// When transition meshes are inserted between blocks of different LOD, we need to make space for them.
	// Secondary vertex positions can be calculated by linearly transforming positions inside boundary cells
	// so that the full-size cell is scaled to a smaller size that allows space for between one and three transition cells,
	// as necessary, depending on the location with respect to the edges and corners of the entire block.
	// This can be accomplished by computing offsets (Δx, Δy, Δz) for the coordinates (x, y, z) in any boundary cell.

	Vector3 delta;

	const float p2k = 1 << lod_index; // 2 ^ lod
	const float p2mk = 1.f / p2k; // 2 ^ (-lod)

	const float wk = TRANSITION_CELL_SCALE * p2k; // 2 ^ (lod - 2), if scale is 0.25

	for (unsigned int i = 0; i < Vector3i::AXIS_COUNT; ++i) {

		const float p = pos[i];
		const float s = block_size[i];

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

inline Vector3 project_border_offset(Vector3 delta, Vector3 normal) {

	// Secondary position can be obtained with the following formula:
	//
	// | x |   | 1 - nx²   ,  -nx * ny  ,  -nx * nz |   | Δx |
	// | y | + | -nx * ny  ,  1 - ny²   ,  -ny * nz | * | Δy |
	// | z |   | -nx * nz  ,  -ny * nz  ,  1 - nz²  |   | Δz |

	return Vector3(
			(1 - normal.x * normal.x) * delta.x /**/ - normal.y * normal.x * delta.y /*     */ - normal.z * normal.x * delta.z,
			/**/ -normal.x * normal.y * delta.x + (1 - normal.y * normal.y) * delta.y /*    */ - normal.z * normal.y * delta.z,
			/**/ -normal.x * normal.z * delta.x /**/ - normal.y * normal.z * delta.y /**/ + (1 - normal.z * normal.z) * delta.z);
}

inline Vector3 get_secondary_position(const Vector3 primary, const Vector3 normal, const int lod_index, const Vector3i block_size) {
	Vector3 delta = get_border_offset(primary, lod_index, block_size);
	delta = project_border_offset(delta, normal);
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

	for (unsigned int i = 0; i < Vector3i::AXIS_COUNT; i++) {
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

inline Vector3 normalized_not_null(Vector3 n) {
	real_t lengthsq = n.length_squared();
	if (lengthsq == 0) {
		return Vector3(0, 1, 0);
	} else {
		real_t length = Math::sqrt(lengthsq);
		return Vector3(n.x / length, n.y / length, n.z / length);
	}
}

} // namespace

VoxelMesherTransvoxel::VoxelMesherTransvoxel() {
	set_padding(MIN_PADDING, MAX_PADDING);
}

void VoxelMesherTransvoxel::clear_output() {
	// Important: memory is NOT deallocated. I rely on vectors keeping their capacity.
	// This is extremely important for performance, while Godot Vector on the same usage caused 50% slowdown.
	_output_indices.clear();
	_output_normals.clear();
	_output_vertices.clear();
	_output_extra.clear();
}

void VoxelMesherTransvoxel::fill_surface_arrays(Array &arrays) {

	PoolVector<Vector3> vertices;
	PoolVector<Vector3> normals;
	PoolVector<Color> extra;
	PoolVector<int> indices;

	raw_copy_to(vertices, _output_vertices);
	raw_copy_to(normals, _output_normals);
	raw_copy_to(extra, _output_extra);
	raw_copy_to(indices, _output_indices);

	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	if (_output_normals.size() != 0) {
		arrays[Mesh::ARRAY_NORMAL] = normals;
	}
	arrays[Mesh::ARRAY_COLOR] = extra;
	arrays[Mesh::ARRAY_INDEX] = indices;
}

void VoxelMesherTransvoxel::build(VoxelMesher::Output &output, const VoxelMesher::Input &input) {

	int channel = VoxelBuffer::CHANNEL_SDF;

	// Initialize dynamic memory:
	// These vectors are re-used.
	// We don't know in advance how much geometry we are going to produce.
	// Once capacity is big enough, no more memory should be allocated
	clear_output();

	const VoxelBuffer &voxels = input.voxels;
	ERR_FAIL_COND(voxels.get_channel_depth(channel) != VoxelBuffer::DEPTH_8_BIT);

	build_internal(voxels, channel, input.lod);

	if (_output_vertices.size() == 0) {
		// The mesh can be empty
		return;
	}

	Array regular_arrays;
	fill_surface_arrays(regular_arrays);
	output.surfaces.push_back(regular_arrays);

	for (int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {

		clear_output();

		build_transition(voxels, channel, dir, input.lod);

		if (_output_vertices.size() == 0) {
			continue;
		}

		Array transition_arrays;
		fill_surface_arrays(transition_arrays);
		output.transition_surfaces[dir].push_back(transition_arrays);
	}

	output.primitive_type = Mesh::PRIMITIVE_TRIANGLES;
	output.compression_flags = MESH_COMPRESSION_FLAGS;
}

// TODO For testing at the moment
Ref<ArrayMesh> VoxelMesherTransvoxel::build_transition_mesh(Ref<VoxelBuffer> voxels, int direction) {

	clear_output();

	ERR_FAIL_COND_V(voxels.is_null(), Ref<ArrayMesh>());

	build_transition(**voxels, VoxelBuffer::CHANNEL_SDF, direction, 0);

	Ref<ArrayMesh> mesh;

	if (_output_vertices.size() == 0) {
		return mesh;
	}

	Array arrays;
	fill_surface_arrays(arrays);
	mesh.instance();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays, Array(), MESH_COMPRESSION_FLAGS);
	return mesh;
}

void VoxelMesherTransvoxel::build_internal(const VoxelBuffer &voxels, unsigned int channel, int lod_index) {

	struct L {
		inline static Vector3i dir_to_prev_vec(uint8_t dir) {
			//return g_corner_dirs[mask] - Vector3(1,1,1);
			return Vector3i(
					-(dir & 1),
					-((dir >> 1) & 1),
					-((dir >> 2) & 1));
		}
	};

	if (voxels.is_uniform(channel)) {
		// Nothing to extract, because constant isolevels never cross the threshold and describe no surface
		return;
	}

	const Vector3i block_size_with_padding = voxels.get_size();
	const Vector3i block_size = block_size_with_padding - Vector3i(MIN_PADDING + MAX_PADDING);
	const Vector3i block_size_scaled = block_size << lod_index;

	// Prepare vertex reuse cache
	reset_reuse_cells(block_size_with_padding);

	// We iterate 2x2 voxel groups, which the paper calls "cells".
	// We also reach one voxel further to compute normals, so we adjust the iterated area
	const Vector3i min_pos = Vector3i(MIN_PADDING);
	const Vector3i max_pos = block_size_with_padding - Vector3i(MAX_PADDING);
	//const Vector3i max_pos_c = max_pos - Vector3i(1);

	FixedArray<int8_t, 8> cell_samples;
	FixedArray<Vector3, 8> corner_gradients;
	FixedArray<Vector3i, 8> corner_positions;

	// Iterate all cells with padding (expected to be neighbors)
	Vector3i pos;
	for (pos.z = min_pos.z; pos.z < max_pos.z; ++pos.z) {
		for (pos.y = min_pos.y; pos.y < max_pos.y; ++pos.y) {
			for (pos.x = min_pos.x; pos.x < max_pos.x; ++pos.x) {

				//    6-------7
				//   /|      /|
				//  / |     / |  Corners
				// 4-------5  |
				// |  2----|--3
				// | /     | /   z y
				// |/      |/    |/
				// 0-------1     o--x
				//
				// Warning: temporarily includes padding. It is undone later.
				corner_positions[0] = Vector3i(pos.x, pos.y, pos.z);
				corner_positions[1] = Vector3i(pos.x + 1, pos.y, pos.z);
				corner_positions[2] = Vector3i(pos.x, pos.y + 1, pos.z);
				corner_positions[3] = Vector3i(pos.x + 1, pos.y + 1, pos.z);
				corner_positions[4] = Vector3i(pos.x, pos.y, pos.z + 1);
				corner_positions[5] = Vector3i(pos.x + 1, pos.y, pos.z + 1);
				corner_positions[6] = Vector3i(pos.x, pos.y + 1, pos.z + 1);
				corner_positions[7] = Vector3i(pos.x + 1, pos.y + 1, pos.z + 1);

				// Get the value of cells.
				// Negative values are "solid" and positive are "air".
				// Due to raw cells being unsigned 8-bit, they get converted to signed.
				for (unsigned int i = 0; i < corner_positions.size(); ++i) {
					cell_samples[i] = tos(get_voxel(voxels, corner_positions[i], channel));
				}

				// Concatenate the sign of cell values to obtain the case code.
				// Index 0 is the less significant bit, and index 7 is the most significant bit.
				uint8_t case_code = sign(cell_samples[0]);
				case_code |= (sign(cell_samples[1]) << 1);
				case_code |= (sign(cell_samples[2]) << 2);
				case_code |= (sign(cell_samples[3]) << 3);
				case_code |= (sign(cell_samples[4]) << 4);
				case_code |= (sign(cell_samples[5]) << 5);
				case_code |= (sign(cell_samples[6]) << 6);
				case_code |= (sign(cell_samples[7]) << 7);

				ReuseCell &current_reuse_cell = get_reuse_cell(pos);
				// Mark as unusable for now
				current_reuse_cell.vertices[0] = -1;

				if (case_code == 0 || case_code == 255) {
					// If the case_code is 0 or 255, there is no triangulation to do
					continue;
				}

				CRASH_COND(case_code > 255);

				// TODO We might not always need all of them
				// Compute normals
				for (unsigned int i = 0; i < corner_positions.size(); ++i) {

					Vector3i p = corner_positions[i];

					float nx = tof(tos(get_voxel(voxels, p.x - 1, p.y, p.z, channel)));
					float ny = tof(tos(get_voxel(voxels, p.x, p.y - 1, p.z, channel)));
					float nz = tof(tos(get_voxel(voxels, p.x, p.y, p.z - 1, channel)));
					float px = tof(tos(get_voxel(voxels, p.x + 1, p.y, p.z, channel)));
					float py = tof(tos(get_voxel(voxels, p.x, p.y + 1, p.z, channel)));
					float pz = tof(tos(get_voxel(voxels, p.x, p.y, p.z + 1, channel)));

					//get_gradient_normal(nx, px, ny, py, nz, pz, cell_samples[i]);
					corner_gradients[i] = Vector3(nx - px, ny - py, nz - pz);

					// Undo padding here. From this point, corner positions are actual positions.
					corner_positions[i] = (corner_positions[i] - min_pos) << lod_index;
				}

				// For cells occurring along the minimal boundaries of a block,
				// the preceding cells needed for vertex reuse may not exist.
				// In these cases, we allow new vertex creation on additional edges of a cell.
				// While iterating through the cells in a block, a 3-bit mask is maintained whose bits indicate
				// whether corresponding bits in a direction code are valid
				uint8_t direction_validity_mask =
						(pos.x > min_pos.x ? 1 : 0) |
						((pos.y > min_pos.y ? 1 : 0) << 1) |
						((pos.z > min_pos.z ? 1 : 0) << 2);

				uint8_t regular_cell_class_index = Transvoxel::get_regular_cell_class(case_code);
				const Transvoxel::RegularCellData &regular_cell_data = Transvoxel::get_regular_cell_data(regular_cell_class_index);
				uint8_t triangle_count = regular_cell_data.geometryCounts & 0x0f;
				uint8_t vertex_count = (regular_cell_data.geometryCounts & 0xf0) >> 4;

				FixedArray<int, 12> cell_vertex_indices(-1);

				uint8_t cell_border_mask = get_border_mask(pos - min_pos, block_size - Vector3i(1));

				// For each vertex in the case
				for (unsigned int i = 0; i < vertex_count; ++i) {

					// The case index maps to a list of 16-bit codes providing information about the edges on which the vertices lie.
					// The low byte of each 16-bit code contains the corner indexes of the edge’s endpoints in one nibble each,
					// and the high byte contains the mapping code shown in Figure 3.8(b)
					unsigned short rvd = Transvoxel::get_regular_vertex_data(case_code, i);
					uint8_t edge_code_low = rvd & 0xff;
					uint8_t edge_code_high = (rvd >> 8) & 0xff;

					// Get corner indexes in the low nibble (always ordered so the higher comes last)
					uint8_t v0 = (edge_code_low >> 4) & 0xf;
					uint8_t v1 = edge_code_low & 0xf;

					ERR_FAIL_COND(v1 <= v0);

					// Get voxel values at the corners
					int sample0 = cell_samples[v0]; // called d0 in the paper
					int sample1 = cell_samples[v1]; // called d1 in the paper

					// TODO Zero-division is not mentionned in the paper??
					ERR_FAIL_COND(sample1 == sample0);
					ERR_FAIL_COND(sample1 == 0 && sample0 == 0);

					// Get interpolation position
					// We use an 8-bit fraction, allowing the new vertex to be located at one of 257 possible
					// positions  along  the  edge  when  both  endpoints  are included.
					int t = (sample1 << 8) / (sample1 - sample0);

					float t0 = static_cast<float>(t) / 256.f;
					float t1 = static_cast<float>(0x100 - t) / 256.f;
					int ti0 = t;
					int ti1 = 0x100 - t;

					const Vector3i p0 = corner_positions[v0];
					const Vector3i p1 = corner_positions[v1];

					if (t & 0xff) {
						// Vertex is between p0 and p1 (inside the edge)

						// Each edge of a cell is assigned an 8-bit code, as shown in Figure 3.8(b),
						// that provides a mapping to a preceding cell and the coincident edge on that preceding cell
						// for which new vertex creation  was  allowed.
						// The high nibble of this code indicates which direction to go in order to reach the correct preceding cell.
						// The bit values 1, 2, and 4 in this nibble indicate that we must subtract one
						// from the x, y, and/or z coordinate, respectively.
						uint8_t reuse_dir = (edge_code_high >> 4) & 0xf;
						uint8_t reuse_vertex_index = edge_code_high & 0xf;

						// TODO Some re-use opportunities are missed on negative sides of the block,
						// but I don't really know how to fix it...
						// You can check by "shaking" every vertex randomly in a shader based on its index,
						// you will see vertices touching the -X, -Y or -Z sides of the block aren't connected

						bool present = (reuse_dir & direction_validity_mask) == reuse_dir;

						if (present) {
							Vector3i cache_pos = pos + L::dir_to_prev_vec(reuse_dir);
							ReuseCell &prev_cell = get_reuse_cell(cache_pos);
							// Will reuse a previous vertice
							cell_vertex_indices[i] = prev_cell.vertices[reuse_vertex_index];
						}

						if (!present || cell_vertex_indices[i] == -1) {
							// Going to create a new vertice

							// TODO Implement surface shifting interpolation (see other places we interpolate too).
							// See issue https://github.com/Zylann/godot_voxel/issues/60
							// Seen in the paper, it fixes "steps" between LODs on flat surfaces.
							// It is using a binary search through higher lods to find the zero-crossing edge.
							// I did not do it here, because our data model is such that when we have low-resolution voxels,
							// we cannot just have a look at the high-res ones, because they are not in memory.
							// However, it might be possible on low-res blocks bordering high-res ones due to neighboring rules,
							// or by falling back on the generator that was used to produce the volume.

							Vector3i primary = p0 * ti0 + p1 * ti1;
							Vector3 primaryf = primary.to_vec3() * FIXED_FACTOR;
							Vector3 normal = normalized_not_null(corner_gradients[v0] * t0 + corner_gradients[v1] * t1);

							Vector3 secondary;
							uint16_t border_mask = cell_border_mask;

							if (cell_border_mask > 0) {
								secondary = get_secondary_position(primaryf, normal, 0, block_size_scaled);
								border_mask |= (get_border_mask(p0, block_size_scaled) & get_border_mask(p1, block_size_scaled)) << 6;
							}

							cell_vertex_indices[i] = emit_vertex(primaryf, normal, border_mask, secondary);

							if (reuse_dir & 8) {
								// Store the generated vertex so that other cells can reuse it.
								current_reuse_cell.vertices[reuse_vertex_index] = cell_vertex_indices[i];
							}
						}

					} else if (t == 0 && v1 == 7) {
						// t == 0: the vertex is on p1
						// v1 == 7: p1 on the max corner of the cell
						// This cell owns the vertex, so it should be created.

						Vector3i primary = p1;
						Vector3 primaryf = primary.to_vec3();
						Vector3 normal = normalized_not_null(corner_gradients[v1]);

						Vector3 secondary;
						uint16_t border_mask = cell_border_mask;

						if (cell_border_mask > 0) {
							secondary = get_secondary_position(primaryf, normal, 0, block_size_scaled);
							border_mask |= get_border_mask(p1, block_size_scaled) << 6;
						}

						cell_vertex_indices[i] = emit_vertex(primaryf, normal, border_mask, secondary);

						current_reuse_cell.vertices[0] = cell_vertex_indices[i];

					} else {
						// The vertex is either on p0 or p1
						// Always try to reuse previous vertices in these cases

						// A 3-bit direction code leading to the proper cell can easily be obtained by
						// inverting the 3-bit corner index (bitwise, by exclusive ORing with the number 7).
						// The corner index depends on the value of t, t = 0 means that we're at the higher
						// numbered endpoint.
						uint8_t reuse_dir = (t == 0 ? v1 ^ 7 : v0 ^ 7);
						bool present = (reuse_dir & direction_validity_mask) == reuse_dir;

						// Note: the only difference with similar code above is that we take vertice 0 in the `else`
						if (present) {
							Vector3i cache_pos = pos + L::dir_to_prev_vec(reuse_dir);
							ReuseCell prev_cell = get_reuse_cell(cache_pos);
							cell_vertex_indices[i] = prev_cell.vertices[0];
						}

						if (!present || cell_vertex_indices[i] < 0) {

							Vector3i primary = t == 0 ? p1 : p0;
							Vector3 primaryf = primary.to_vec3();
							Vector3 normal = normalized_not_null(corner_gradients[t == 0 ? v1 : v0]);

							// TODO This bit of code is repeated several times, factor it?
							Vector3 secondary;
							uint16_t border_mask = cell_border_mask;

							if (cell_border_mask > 0) {
								secondary = get_secondary_position(primaryf, normal, 0, block_size_scaled);
								border_mask |= get_border_mask(primary, block_size_scaled) << 6;
							}

							cell_vertex_indices[i] = emit_vertex(primaryf, normal, border_mask, secondary);
						}
					}

				} // for each cell vertex

				for (int t = 0; t < triangle_count; ++t) {
					for (int i = 0; i < 3; ++i) {
						int index = cell_vertex_indices[regular_cell_data.get_vertex_index(t * 3 + i)];
						_output_indices.push_back(index);
					}
				}

			} // x
		} // y
	} // z
}

void VoxelMesherTransvoxel::build_transition(const VoxelBuffer &p_voxels, unsigned int channel, int direction, int lod_index) {

	//    y            y
	//    |            | z
	//    |            |/     OpenGL axis convention
	//    o---x    x---o
	//   /
	//  z

	struct L {
		// Convert from face-space to block-space coordinates, considering which face we are working on.
		static inline Vector3i face_to_block(int x, int y, int z, int dir, const Vector3i &bs) {

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
					CRASH_COND(true);
					return Vector3i();
			}
		}
		// I took the choice of supporting non-cubic area, so...
		static inline void get_face_axes(int &ax, int &ay, int dir) {
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
					CRASH_COND(true);
			}
		}
	};

	if (p_voxels.is_uniform(channel)) {
		// Nothing to extract, because constant isolevels never cross the threshold and describe no surface
		return;
	}

	const Vector3i block_size_with_padding = p_voxels.get_size();
	const Vector3i block_size_without_padding = block_size_with_padding - Vector3i(MIN_PADDING + MAX_PADDING);
	const Vector3i block_size_scaled = block_size_without_padding << lod_index;

	ERR_FAIL_COND(block_size_with_padding.x < 3);
	ERR_FAIL_COND(block_size_with_padding.y < 3);
	ERR_FAIL_COND(block_size_with_padding.z < 3);

	reset_reuse_cells_2d(block_size_with_padding);

	// This part works in "face space", which is 2D along local X and Y axes.
	// In this space, -Z points towards the half resolution cells, while +Z points towards full-resolution cells.
	// Conversion is used to map this space to block space using a direction enum.

	// Note: I made a few changes compared to the paper.
	// Instead of making transition meshes go from low-res blocks to high-res blocks,
	// I do the opposite, going from high-res to low-res. It's easier because half-res voxels are available for free,
	// if we compute the transition meshes right after the regular mesh, with the same voxel data.

	// This represents the actual box of voxels we are working on.
	// It also represents positions of the minimum and maximum vertices that can be generated.
	// Padding is present to allow reaching 1 voxel further for calculating normals
	const Vector3i min_pos = Vector3i(MIN_PADDING);
	const Vector3i max_pos = block_size_with_padding - Vector3i(MAX_PADDING);

	int axis_x, axis_y;
	L::get_face_axes(axis_x, axis_y, direction);
	const int min_fpos_x = min_pos[axis_x];
	const int min_fpos_y = min_pos[axis_y];
	const int max_fpos_x = max_pos[axis_x] - 1; // Another -1 here, because the 2D kernel is 3x3
	const int max_fpos_y = max_pos[axis_y] - 1;

	FixedArray<int8_t, 13> cell_samples;
	FixedArray<Vector3i, 13> cell_positions;
	FixedArray<Vector3, 13> cell_gradients;

	// Iterating in face space
	for (int fy = min_fpos_y; fy < max_fpos_y; fy += 2) {
		for (int fx = min_fpos_x; fx < max_fpos_x; fx += 2) {

			const int fz = MIN_PADDING;

			const VoxelBuffer &fvoxels = p_voxels;

			// Cell positions in block space
			// Warning: temporarily includes padding. It is undone later.
			cell_positions[0] = L::face_to_block(fx, fy, fz, direction, block_size_with_padding);
			cell_positions[1] = L::face_to_block(fx + 1, fy, fz, direction, block_size_with_padding);
			cell_positions[2] = L::face_to_block(fx + 2, fy, fz, direction, block_size_with_padding);
			cell_positions[3] = L::face_to_block(fx, fy + 1, fz, direction, block_size_with_padding);
			cell_positions[4] = L::face_to_block(fx + 1, fy + 1, fz, direction, block_size_with_padding);
			cell_positions[5] = L::face_to_block(fx + 2, fy + 1, fz, direction, block_size_with_padding);
			cell_positions[6] = L::face_to_block(fx, fy + 2, fz, direction, block_size_with_padding);
			cell_positions[7] = L::face_to_block(fx + 1, fy + 2, fz, direction, block_size_with_padding);
			cell_positions[8] = L::face_to_block(fx + 2, fy + 2, fz, direction, block_size_with_padding);
			cell_positions[0x9] = cell_positions[0];
			cell_positions[0xA] = cell_positions[2];
			cell_positions[0xB] = cell_positions[6];
			cell_positions[0xC] = cell_positions[8];

			//  6---7---8
			//  |   |   |
			//  3---4---5
			//  |   |   |
			//  0---1---2

			// Full-resolution samples 0..8
			for (unsigned int i = 0; i < 9; ++i) {
				cell_samples[i] = tos(get_voxel(fvoxels, cell_positions[i], channel));
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

			// TODO We may not need all of them!
			for (unsigned int i = 0; i < 9; ++i) {

				Vector3i p = cell_positions[i];

				float nx = tof(tos(get_voxel(fvoxels, p.x - 1, p.y, p.z, channel)));
				float ny = tof(tos(get_voxel(fvoxels, p.x, p.y - 1, p.z, channel)));
				float nz = tof(tos(get_voxel(fvoxels, p.x, p.y, p.z - 1, channel)));
				float px = tof(tos(get_voxel(fvoxels, p.x + 1, p.y, p.z, channel)));
				float py = tof(tos(get_voxel(fvoxels, p.x, p.y + 1, p.z, channel)));
				float pz = tof(tos(get_voxel(fvoxels, p.x, p.y, p.z + 1, channel)));

				cell_gradients[i] = Vector3(nx - px, ny - py, nz - pz);
			}
			cell_gradients[0x9] = cell_gradients[0];
			cell_gradients[0xA] = cell_gradients[2];
			cell_gradients[0xB] = cell_gradients[6];
			cell_gradients[0xC] = cell_gradients[8];

			// Convert grid positions into actual positions, since we don't need to use them to access the buffer anymore
			for (unsigned int i = 0; i < cell_positions.size(); ++i) {
				cell_positions[i] = (cell_positions[i] - min_pos) << lod_index;
			}

			uint16_t case_code = sign(cell_samples[0]);
			case_code |= (sign(cell_samples[1]) << 1);
			case_code |= (sign(cell_samples[2]) << 2);
			case_code |= (sign(cell_samples[5]) << 3);
			case_code |= (sign(cell_samples[8]) << 4);
			case_code |= (sign(cell_samples[7]) << 5);
			case_code |= (sign(cell_samples[6]) << 6);
			case_code |= (sign(cell_samples[3]) << 7);
			case_code |= (sign(cell_samples[4]) << 8);

			ReuseTransitionCell &current_reuse_cell = get_reuse_cell_2d(fx, fy);
			// Mark current cell unused for now
			current_reuse_cell.vertices[0] = -1;

			if (case_code == 0 || case_code == 511) {
				// The cell contains no triangles.
				continue;
			}

			CRASH_COND(case_code > 511);

			const uint8_t cell_class = Transvoxel::get_transition_cell_class(case_code);

			CRASH_COND((cell_class & 0x7f) > 55);

			const Transvoxel::TransitionCellData cell_data = Transvoxel::get_transition_cell_data(cell_class & 0x7f);
			const bool flip_triangles = ((cell_class & 128) != 0);

			unsigned int vertex_count = cell_data.GetVertexCount();
			FixedArray<int, 12> cell_vertex_indices(-1);
			CRASH_COND(vertex_count > cell_vertex_indices.size());

			uint8_t direction_validity_mask = (fx > min_fpos_x ? 1 : 0) | ((fy > min_fpos_y ? 1 : 0) << 1);

			uint8_t cell_border_mask = get_border_mask(cell_positions[0], block_size_scaled);

			for (unsigned int i = 0; i < vertex_count; ++i) {

				const uint16_t edge_code = Transvoxel::get_transition_vertex_data(case_code, i);
				const uint8_t index_vertex_a = (edge_code >> 4) & 0xf;
				const uint8_t index_vertex_b = (edge_code & 0xf);

				const int sample_a = cell_samples[index_vertex_a]; // d0 and d1 in the paper
				const int sample_b = cell_samples[index_vertex_b];
				// TODO Zero-division is not mentionned in the paper??
				ERR_FAIL_COND(sample_a == sample_b);
				ERR_FAIL_COND(sample_a == 0 && sample_b == 0);

				// Get interpolation position
				// We use an 8-bit fraction, allowing the new vertex to be located at one of 257 possible
				// positions  along  the  edge  when  both  endpoints  are included.
				const int t = (sample_b << 8) / (sample_b - sample_a);

				const float t0 = static_cast<float>(t) / 256.f;
				const float t1 = static_cast<float>(0x100 - t) / 256.f;
				const int ti0 = t;
				const int ti1 = 0x100 - t;

				if (t & 0xff) {
					// Vertex lies in the interior of the edge.
					// (i.e t is either 0 or 257, meaning it's either directly on vertex a or vertex b)

					uint8_t vertex_index_to_reuse_or_create = (edge_code >> 8) & 0xf;

					// The bit values 1 and 2 in this nibble indicate that we must subtract one from the x or y coordinate, respectively,
					// and these two bits are never simultaneously set. The bit value 4 indicates that a new vertex is to be created on an interior edge
					// where it cannot be reused, and the bit value 8 indicates that a new vertex is to be created on a  maximal  edge  where  it  can  be  reused.
					//
					// Bit 0 (0x1): need to subtract one to X
					// Bit 1 (0x2): need to subtract one to Y
					// Bit 2 (0x4): vertex is on an interior edge, won't be reused
					// Bit 3 (0x8): vertex is on a maximal edge, it can be reused
					uint8_t reuse_direction = (edge_code >> 12);

					bool present = (reuse_direction & direction_validity_mask) == reuse_direction;

					if (present) {
						// The previous cell is available. Retrieve the cached cell
						// from which to retrieve the reused vertex index from.
						const ReuseTransitionCell &prev = get_reuse_cell_2d(fx - (reuse_direction & 1), fy - ((reuse_direction >> 1) & 1));
						// Reuse the vertex index from the previous cell.
						cell_vertex_indices[i] = prev.vertices[vertex_index_to_reuse_or_create];
					}

					if (!present || cell_vertex_indices[i] == -1) {
						// Going to create a new vertex

						const Vector3i p0 = cell_positions[index_vertex_a];
						const Vector3i p1 = cell_positions[index_vertex_b];

						const Vector3 n0 = cell_gradients[index_vertex_a];
						const Vector3 n1 = cell_gradients[index_vertex_b];

						Vector3i primary = p0 * ti0 + p1 * ti1;
						Vector3 primaryf = primary.to_vec3() * FIXED_FACTOR;
						Vector3 normal = normalized_not_null(n0 * t0 + n1 * t1);

						bool fullres_side = (index_vertex_a < 9 || index_vertex_b < 9);
						uint16_t border_mask = cell_border_mask;

						Vector3 secondary;
						if (fullres_side) {

							secondary = get_secondary_position(primaryf, normal, 0, block_size_scaled);
							border_mask |= (get_border_mask(p0, block_size_scaled) & get_border_mask(p1, block_size_scaled)) << 6;

						} else {
							// If the vertex is on the half-res side (in our implementation, it's the side of the block),
							// then we make the mask 0 so that the vertex is never moved. We only move the full-res side to
							// connect with the regular mesh, which will also be moved by the same amount to fit the transition mesh.
							border_mask = 0;
						}

						cell_vertex_indices[i] = emit_vertex(primaryf, normal, border_mask, secondary);

						if (reuse_direction & 0x8) {
							// The vertex can be re-used later
							ReuseTransitionCell &r = get_reuse_cell_2d(fx, fy);
							r.vertices[vertex_index_to_reuse_or_create] = cell_vertex_indices[i];
						}
					}

				} else {
					// The vertex is exactly on one of the edge endpoints.
					// Try to reuse corner vertex from a preceding cell.
					// Use the reuse information in transitionCornerData.

					uint8_t index_vertex = (t == 0 ? index_vertex_b : index_vertex_a);
					CRASH_COND(index_vertex >= 13);
					uint8_t corner_data = Transvoxel::get_transition_corner_data(index_vertex);
					uint8_t vertex_index_to_reuse_or_create = (corner_data & 0xf);
					uint8_t reuse_direction = ((corner_data >> 4) & 0xf);

					bool present = (reuse_direction & direction_validity_mask) == reuse_direction;

					if (present) {
						// The previous cell is available. Retrieve the cached cell
						// from which to retrieve the reused vertex index from.
						const ReuseTransitionCell &prev = get_reuse_cell_2d(fx - (reuse_direction & 1), fy - ((reuse_direction >> 1) & 1));
						// Reuse the vertex index from the previous cell.
						cell_vertex_indices[i] = prev.vertices[vertex_index_to_reuse_or_create];
					}

					if (!present || cell_vertex_indices[i] == -1) {
						// Going to create a new vertex

						Vector3i primary = cell_positions[index_vertex];
						Vector3 primaryf = primary.to_vec3();
						Vector3 normal = normalized_not_null(cell_gradients[index_vertex]);

						bool fullres_side = (index_vertex < 9);
						uint16_t border_mask = cell_border_mask;

						Vector3 secondary;
						if (fullres_side) {

							secondary = get_secondary_position(primaryf, normal, 0, block_size_scaled);
							border_mask |= get_border_mask(primary, block_size_scaled) << 6;

						} else {
							border_mask = 0;
						}

						cell_vertex_indices[i] = emit_vertex(primaryf, normal, border_mask, secondary);

						// We are on a corner so the vertex will be re-usable later
						ReuseTransitionCell &r = get_reuse_cell_2d(fx, fy);
						r.vertices[vertex_index_to_reuse_or_create] = cell_vertex_indices[i];
					}
				}

			} // for vertex

			unsigned int triangle_count = cell_data.GetTriangleCount();

			for (unsigned int ti = 0; ti < triangle_count; ++ti) {
				if (flip_triangles) {
					_output_indices.push_back(cell_vertex_indices[cell_data.get_vertex_index(ti * 3)]);
					_output_indices.push_back(cell_vertex_indices[cell_data.get_vertex_index(ti * 3 + 1)]);
					_output_indices.push_back(cell_vertex_indices[cell_data.get_vertex_index(ti * 3 + 2)]);
				} else {
					_output_indices.push_back(cell_vertex_indices[cell_data.get_vertex_index(ti * 3 + 2)]);
					_output_indices.push_back(cell_vertex_indices[cell_data.get_vertex_index(ti * 3 + 1)]);
					_output_indices.push_back(cell_vertex_indices[cell_data.get_vertex_index(ti * 3)]);
				}
			}

		} // for x
	} // for y
}

void VoxelMesherTransvoxel::reset_reuse_cells(Vector3i block_size) {
	_block_size = block_size;
	unsigned int deck_area = block_size.x * block_size.y;
	for (unsigned int i = 0; i < _cache.size(); ++i) {
		std::vector<ReuseCell> &deck = _cache[i];
		deck.resize(deck_area);
		for (size_t j = 0; j < deck.size(); ++j) {
			deck[j].vertices.fill(-1);
		}
	}
}

void VoxelMesherTransvoxel::reset_reuse_cells_2d(Vector3i block_size) {
	for (unsigned int i = 0; i < _cache_2d.size(); ++i) {
		std::vector<ReuseTransitionCell> &row = _cache_2d[i];
		row.resize(block_size.x);
		for (size_t j = 0; j < row.size(); ++j) {
			row[j].vertices.fill(-1);
		}
	}
}

VoxelMesherTransvoxel::ReuseCell &VoxelMesherTransvoxel::get_reuse_cell(Vector3i pos) {
	unsigned int j = pos.z & 1;
	unsigned int i = pos.y * _block_size.y + pos.x;
	CRASH_COND(i >= _cache[j].size());
	return _cache[j][i];
}

VoxelMesherTransvoxel::ReuseTransitionCell &VoxelMesherTransvoxel::get_reuse_cell_2d(int x, int y) {
	unsigned int j = y & 1;
	unsigned int i = x;
	CRASH_COND(i >= _cache_2d[j].size());
	return _cache_2d[j][i];
}

int VoxelMesherTransvoxel::emit_vertex(Vector3 primary, Vector3 normal, uint16_t border_mask, Vector3 secondary) {

	int vi = _output_vertices.size();

	_output_vertices.push_back(primary);
	_output_normals.push_back(normal);
	_output_extra.push_back(Color(secondary.x, secondary.y, secondary.z, border_mask));

	return vi;
}

VoxelMesher *VoxelMesherTransvoxel::clone() {
	return memnew(VoxelMesherTransvoxel);
}

void VoxelMesherTransvoxel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("build_transition_mesh", "voxel_buffer", "direction"), &VoxelMesherTransvoxel::build_transition_mesh);
}
