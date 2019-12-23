#include "voxel_mesher_transvoxel.h"
#include "transvoxel_tables.cpp"
#include <core/os/os.h>

// This is a very partial implementation, it is barely equivalent to marching cubes.
// It doesn't include transition cells.

namespace {

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

Vector3 get_border_offset(Vector3 pos, int lod, Vector3i block_size, Vector3i min_pos) {

	// When transition meshes are inserted between blocks of different LOD, we need to make space for them.
	// Secondary vertex positions can be calculated by linearly transforming positions inside boundary cells
	// so that the full-size cell is scaled to a smaller size that allows space for between one and three transition cells,
	// as necessary, depending on the location with respect to the edges and corners of the entire block.
	// This can be accomplished by computing offsets (Δx, Δy, Δz) for the coordinates (x, y, z) in any boundary cell.

	Vector3 delta;

	if (lod == 0) {
		return delta;
	}

	static float p2mk_precalculated[] = {
		1.0,
		0.5,
		0.25,
		0.125,
		0.0625,
		0.03125,
		0.015625,
		0.0078125,
		0.00390625,
		0.001953125,
		0.0009765625,
		0.00048828125,
		0.000244140625,
		0.0001220703125
	};
	static const int max_lod = 14;
	CRASH_COND(lod >= max_lod);

	const float p2k = 1 << lod; // 2 ^ lod
	const float p2mk = p2mk_precalculated[lod]; // 2 ^ (-lod)
	// The paper uses 2 ^ (-lod) because it needs to "undo" the LOD scale of the (x,y,z) coordinates.
	// but in our implementation, this is useless, because we are working in local scale.
	// So a full-resolution cell will always have size 1, and a half-resolution cell will always have size 2.
	// It also means LOD itself is relative, so it will only take values 0 and 1.
	// Nevertheless, I still implement this as per the paper, but without caring too much about max LOD limitation.

	const float wk = lod < 2 ? 0.5f : (1 << (lod - 2)); // 2 ^ (lod - 2)

	for (unsigned int i = 0; i < Vector3i::AXIS_COUNT; ++i) {

		const float p = pos[i] - min_pos[i];
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

// TODO Use this to prevent some cases of null normals
inline Vector3 get_gradient_normal(uint8_t left, uint8_t right, uint8_t bottom, uint8_t top, uint8_t back, uint8_t front, uint8_t middle) {

	float gx, gy, gz;

	if (left == right && bottom == top && back == front) {
		// Sided gradient, but can't be zero
		float m = tof(tos(middle));
		gx = tof(tos(left)) - m;
		gy = tof(tos(bottom)) - m;
		gz = tof(tos(back)) - m;

	} else {
		// Symetric gradient
		gx = tof(tos(left)) - tof(tos(right));
		gy = tof(tos(bottom)) - tof(tos(top));
		gz = tof(tos(back)) - tof(tos(front));
	}

	return Vector3(gx, gy, gz).normalized();
}

} // namespace

int VoxelMesherTransvoxel::get_minimum_padding() const {
	return MINIMUM_PADDING;
}

void VoxelMesherTransvoxel::clear_output() {
	_output_indices.clear();
	_output_normals.clear();
	_output_vertices.clear();
	//m_output_vertices_secondary.clear();
}

void VoxelMesherTransvoxel::fill_surface_arrays(Array &arrays) {

	PoolVector<Vector3> vertices;
	PoolVector<Vector3> normals;
	PoolVector<int> indices;

	raw_copy_to(vertices, _output_vertices);
	raw_copy_to(normals, _output_normals);
	raw_copy_to(indices, _output_indices);

	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	if (_output_normals.size() != 0) {
		arrays[Mesh::ARRAY_NORMAL] = normals;
	}
	arrays[Mesh::ARRAY_INDEX] = indices;
}

void VoxelMesherTransvoxel::build(VoxelMesher::Output &output, const VoxelBuffer &voxels, int padding) {

	ERR_FAIL_COND(padding < MINIMUM_PADDING);

	int channel = VoxelBuffer::CHANNEL_ISOLEVEL;

	// Initialize dynamic memory:
	// These vectors are re-used.
	// We don't know in advance how much geometry we are going to produce.
	// Once capacity is big enough, no more memory should be allocated
	clear_output();

	build_internal(voxels, channel);
	//	OS::get_singleton()->print("vertices: %i, normals: %i, indices: %i\n",
	//							   m_output_vertices.size(),
	//							   m_output_normals.size(),
	//							   m_output_indices.size());

	if (_output_vertices.size() == 0) {
		// The mesh can be empty
		return;
	}

	Array arrays;
	fill_surface_arrays(arrays);

	output.surfaces.push_back(arrays);
	output.primitive_type = Mesh::PRIMITIVE_TRIANGLES;
}

// TODO For testing at the moment
Ref<ArrayMesh> VoxelMesherTransvoxel::build_transition_mesh(Ref<VoxelBuffer> voxels) {

	clear_output();

	ERR_FAIL_COND_V(voxels.is_null(), Ref<ArrayMesh>());

	build_transition(**voxels, VoxelBuffer::CHANNEL_ISOLEVEL);

	Ref<ArrayMesh> mesh;

	if (_output_vertices.size() == 0) {
		return mesh;
	}

	Array arrays;
	fill_surface_arrays(arrays);
	mesh.instance();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

void VoxelMesherTransvoxel::build_internal(const VoxelBuffer &voxels, unsigned int channel) {

	struct L {
		inline static Vector3i dir_to_prev_vec(uint8_t dir) {
			//return g_corner_dirs[mask] - Vector3(1,1,1);
			return Vector3i(
					-(dir & 1),
					-((dir >> 1) & 1),
					-((dir >> 2) & 1));
		}
	};

	//
	//    6-------7
	//   /|      /|
	//  / |     / |  Corners
	// 4-------5  |
	// |  2----|--3
	// | /     | /   z y
	// |/      |/    |/
	// 0-------1     o--x
	//

	static const Vector3i g_corner_dirs[8] = {
		Vector3i(0, 0, 0),
		Vector3i(1, 0, 0),
		Vector3i(0, 1, 0),
		Vector3i(1, 1, 0),
		Vector3i(0, 0, 1),
		Vector3i(1, 0, 1),
		Vector3i(0, 1, 1),
		Vector3i(1, 1, 1)
	};

	// Each 2x2 voxel group is a "cell"

	if (voxels.is_uniform(channel)) {
		// Nothing to extract, because constant isolevels never cross the threshold and describe no surface
		return;
	}

	const Vector3i block_size = voxels.get_size();

	// Prepare vertex reuse cache
	reset_reuse_cells(block_size);

	Vector3i min_pos = PAD;

	// Iterate all cells with padding (expected to be neighbors)
	Vector3i pos;
	for (pos.z = min_pos.z; pos.z < block_size.z - 2; ++pos.z) {
		for (pos.y = min_pos.y; pos.y < block_size.y - 2; ++pos.y) {
			for (pos.x = min_pos.x; pos.x < block_size.x - 2; ++pos.x) {

				// Get the value of cells.
				// Negative values are "solid" and positive are "air".
				// Due to raw cells being unsigned 8-bit, they get converted to signed.
				int8_t cell_samples[8] = {
					tos(get_voxel(voxels, pos.x, pos.y, pos.z, channel)),
					tos(get_voxel(voxels, pos.x + 1, pos.y, pos.z, channel)),
					tos(get_voxel(voxels, pos.x, pos.y + 1, pos.z, channel)),
					tos(get_voxel(voxels, pos.x + 1, pos.y + 1, pos.z, channel)),
					tos(get_voxel(voxels, pos.x, pos.y, pos.z + 1, channel)),
					tos(get_voxel(voxels, pos.x + 1, pos.y, pos.z + 1, channel)),
					tos(get_voxel(voxels, pos.x, pos.y + 1, pos.z + 1, channel)),
					tos(get_voxel(voxels, pos.x + 1, pos.y + 1, pos.z + 1, channel))
				};

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

				{
					ReuseCell &rc = get_reuse_cell(pos);
					rc.case_index = case_code;
				}

				if (case_code == 0 || case_code == 255) {
					// If the case_code is 0 or 255, there is no triangulation to do
					continue;
				}

				CRASH_COND(case_code > 255);

				// TODO We might not always need all of them
				// Compute normals
				Vector3 corner_normals[8];
				for (unsigned int i = 0; i < 8; ++i) {

					Vector3i p = pos + g_corner_dirs[i];

					float nx = tof(tos(get_voxel(voxels, p - Vector3i(1, 0, 0), channel))) - tof(tos(get_voxel(voxels, p + Vector3i(1, 0, 0), channel)));
					float ny = tof(tos(get_voxel(voxels, p - Vector3i(0, 1, 0), channel))) - tof(tos(get_voxel(voxels, p + Vector3i(0, 1, 0), channel)));
					float nz = tof(tos(get_voxel(voxels, p - Vector3i(0, 0, 1), channel))) - tof(tos(get_voxel(voxels, p + Vector3i(0, 0, 1), channel)));

					corner_normals[i] = Vector3(nx, ny, nz);
					corner_normals[i].normalize();
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

				uint8_t regular_cell_class_index = Transvoxel::regularCellClass[case_code];
				Transvoxel::RegularCellData regular_cell_class = Transvoxel::regularCellData[regular_cell_class_index];
				uint8_t triangle_count = regular_cell_class.geometryCounts & 0x0f;
				uint8_t vertex_count = (regular_cell_class.geometryCounts & 0xf0) >> 4;

				int cell_vertex_indices[12];

				// For each vertex in the case
				for (unsigned int i = 0; i < vertex_count; ++i) {

					// The case index maps to a list of 16-bit codes providing information about the edges on which the vertices lie.
					// The low byte of each 16-bit code contains the corner indexes of the edge’s endpoints in one nibble each,
					// and the high byte contains the mapping code shown in Figure 3.8(b)
					unsigned short rvd = Transvoxel::regularVertexData[case_code][i];
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

					Vector3i p0 = pos + g_corner_dirs[v0];
					Vector3i p1 = pos + g_corner_dirs[v1];

					if (t & 0xff) {
						// Vertex lies in the interior of the edge.

						// Each edge of a cell is assigned an 8-bit code, as shown in Figure 3.8(b),
						// that provides a mapping to a preceding cell and the coincident edge on that preceding cell
						// for which new vertex creation  was  allowed.
						// The high nibble of this code indicates which direction to go in order to reach the correct preceding cell.
						// The bit values 1, 2, and 4 in this nibble indicate that we must subtract one
						// from the x, y, and/or z coordinate, respectively.
						uint8_t reuse_dir = (edge_code_high >> 4) & 0xf;
						uint8_t reuse_vertex_index = edge_code_high & 0xf;

						bool present = (reuse_dir & direction_validity_mask) == reuse_dir;

						if (present) {
							Vector3i cache_pos = pos + L::dir_to_prev_vec(reuse_dir);
							ReuseCell &prev_cell = get_reuse_cell(cache_pos);

							if (prev_cell.case_index == 0 || prev_cell.case_index == 255) {
								// TODO I don't think this can happen for non-corner vertices.
								cell_vertex_indices[i] = -1;
							} else {
								// Will reuse a previous vertice
								cell_vertex_indices[i] = prev_cell.vertices[reuse_vertex_index];
							}
						}

						if (!present || cell_vertex_indices[i] == -1) {
							// Going to create a new vertice

							// TODO Implement surface shifting interpolation (see other places we interpolate too).
							// See issue https://github.com/Zylann/godot_voxel/issues/60
							// Seen in the paper, it fixes "steps" between LODs on flat surfaces.
							// I did not do it here, because our data model is such that when we have low-resolution voxels,
							// we cannot just have a look at the high-res ones, because they are not in memory.
							// However, it might be possible on low-res blocks bordering high-res ones due to neighboring rules,
							// or with a different storage strategy.
							Vector3 pi = p0.to_vec3() * t0 + p1.to_vec3() * t1;

							Vector3 primary = pi; //pos.to_vec3() + pi;
							Vector3 normal = corner_normals[v0] * t0 + corner_normals[v1] * t1;
							// TODO Implement secondary position.
							// It is needed in order to make space for transition meshes inside the vertex shader

							cell_vertex_indices[i] = emit_vertex(primary, normal);

							if (reuse_dir & 8) {
								// Store the generated vertex so that other cells can reuse it.
								ReuseCell &rc = get_reuse_cell(pos);
								rc.vertices[reuse_vertex_index] = cell_vertex_indices[i];
							}
						}

					} else if (t == 0 && v1 == 7) {

						// This cell owns the vertex, so it should be created.

						Vector3 pi = p0.to_vec3() * t0 + p1.to_vec3() * t1;
						Vector3 primary = pi; //pos.to_vec3() + pi;
						Vector3 normal = corner_normals[v0] * t0 + corner_normals[v1] * t1;

						cell_vertex_indices[i] = emit_vertex(primary, normal);

						ReuseCell &rc = get_reuse_cell(pos);
						rc.vertices[0] = cell_vertex_indices[i];

					} else {
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

							// The previous cell might not have any geometry, and we
							// might therefore have to create a new vertex anyway.
							if (prev_cell.case_index == 0 || prev_cell.case_index == 255) {
								cell_vertex_indices[i] = -1;
							} else {
								cell_vertex_indices[i] = prev_cell.vertices[0];
							}
						}

						if (!present || cell_vertex_indices[i] < 0) {

							Vector3 pi = p0.to_vec3() * t0 + p1.to_vec3() * t1;
							Vector3 primary = pi; //pos.to_vec3() + pi;
							Vector3 normal = corner_normals[v0] * t0 + corner_normals[v1] * t1;

							cell_vertex_indices[i] = emit_vertex(primary, normal);
						}
					}

				} // for each cell vertice

				for (int t = 0; t < triangle_count; ++t) {
					for (int i = 0; i < 3; ++i) {
						int index = cell_vertex_indices[regular_cell_class.vertexIndex[t * 3 + i]];
						_output_indices.push_back(index);
					}
				}

			} // x
		} // y
	} // z

	// TODO For the second part, take a look at:
	// - https://github.com/Phyronnaz/VoxelPlugin/blob/master/Source/Voxel/Private/VoxelRender/Polygonizers/VoxelMCPolygonizer.cpp
	// - https://github.com/BinaryConstruct/Transvoxel-XNA/blob/master/Transvoxel/SurfaceExtractor/ported/TransvoxelExtractor.cs
}

void VoxelMesherTransvoxel::build_transitions(const TransitionVoxels &p_voxels, unsigned int channel) {

	//	const unsigned int side_to_axis[6] = {
	//		Vector3i::AXIS_X,
	//		Vector3i::AXIS_X,
	//		Vector3i::AXIS_Y,
	//		Vector3i::AXIS_Y,
	//		Vector3i::AXIS_Z,
	//		Vector3i::AXIS_Z
	//	};

	//  o---o---o---o---o-------o
	//  |   |   |   |   |       |
	//  o---o---o---o---o       |
	//  |   |   | n | n |       |
	//  o---o---o---o---o-------o
	//  |   | n |       |       |
	//  o---o---o   X   |       |
	//  |   | n |       |       |
	//  o---o---o-------o-------o
	//  |       |       |       |
	//  |       |       |       |
	//  |       |       |       |
	//  o-------o-------o-------o

	// Check which transition meshes we want
	for (unsigned int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {

		const VoxelBuffer *n = p_voxels.full_resolution_neighbor_voxels[dir];

		if (n != nullptr) {
			build_transition(*n, channel);
		}
	}
}

void VoxelMesherTransvoxel::build_transition(const VoxelBuffer &p_voxels, unsigned int channel) {

	//    y
	//    |
	//    |       OpenGL axis convention
	//    o---x
	//   /
	//  z

	struct L {

		// Convert from face-space into block-space coordinates considering which face we are working on.
		static inline Vector3i face_to_block(int x, int y, int z, int dir, const Vector3i &bs) {

			// TODO Include padding?
			switch (dir) {

				case Cube::SIDE_LEFT: // -x
					return Vector3i(-z, y, bs.z - x);

				case Cube::SIDE_RIGHT: // +x
					return Vector3i(bs.x + z, y, x);

				case Cube::SIDE_BOTTOM: // -y
					return Vector3i(x, -z, bs.z - y);

				case Cube::SIDE_TOP: // +y
					return Vector3i(x, bs.y + z, y);

				case Cube::SIDE_BACK: // +z
					return Vector3i(bs.x - x, y, bs.z + z);

				case Cube::SIDE_FRONT: // -z
					return Vector3i(x, y, -z);

				default:
					CRASH_COND(true);
					return Vector3i();
			}
		}
	};

	if (p_voxels.is_uniform(channel)) {
		// Nothing to extract, because constant isolevels never cross the threshold and describe no surface
		return;
	}

	const Vector3i block_size = p_voxels.get_size();
	//const Vector3i half_block_size = block_size / 2;

	ERR_FAIL_COND(block_size.x < 3);
	ERR_FAIL_COND(block_size.y < 3);
	ERR_FAIL_COND(block_size.z < 3);

	reset_reuse_cells_2d(block_size);

	// This part works in "face space", which is mainly 2D along the X and Y axes.
	// In this space, -Z points towards the full resolution cells, while +Z points towards half-resolution cells.
	// Conversion is used to map this space to block space using a direction enum.

	//Vector3i min_hpos = PAD;
	Vector3i min_fpos = PAD;

	// Padding takes into account the fact our kernel is 3x3 on full-res side (max voxels being shared with next),
	// And that we may look one voxel further to compute normals
	for (int fy = min_fpos.y; fy < block_size.y - 3; fy += 2) {
		for (int fx = min_fpos.x; fx < block_size.x - 3; fx += 2) {

			const int fz = min_fpos.z;

			const VoxelBuffer &fvoxels = p_voxels;

			int8_t cell_samples[13];

			//  6---7---8
			//  |   |   |
			//  3---4---5
			//  |   |   |
			//  0---1---2

			// TODO Double-check positions and transforms. I understand what's needed (contrary to what's next ._.) but it's boring to do, so I botched it

			// Full-resolution samples 0..8
			cell_samples[0] = tos(get_voxel(fvoxels, fx, fy, fz, channel));
			cell_samples[1] = tos(get_voxel(fvoxels, fx + 1, fy, fz, channel));
			cell_samples[2] = tos(get_voxel(fvoxels, fx + 2, fy, fz, channel));
			cell_samples[3] = tos(get_voxel(fvoxels, fx, fy + 1, fz, channel));
			cell_samples[4] = tos(get_voxel(fvoxels, fx + 1, fy + 1, fz, channel));
			cell_samples[5] = tos(get_voxel(fvoxels, fx + 2, fy + 1, fz, channel));
			cell_samples[6] = tos(get_voxel(fvoxels, fx, fy + 2, fz, channel));
			cell_samples[7] = tos(get_voxel(fvoxels, fx + 1, fy + 2, fz, channel));
			cell_samples[8] = tos(get_voxel(fvoxels, fx + 2, fy + 2, fz, channel));

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

			// TODO Eventually we should divide these positions by 2 in the final mesh
			Vector3i cell_positions[13];
			cell_positions[0] = Vector3i(fx, fy, fz);
			cell_positions[1] = Vector3i(fx + 1, fy, fz);
			cell_positions[2] = Vector3i(fx + 2, fy, fz);
			cell_positions[3] = Vector3i(fx, fy + 1, fz);
			cell_positions[4] = Vector3i(fx + 1, fy + 1, fz);
			cell_positions[5] = Vector3i(fx + 2, fy + 1, fz);
			cell_positions[6] = Vector3i(fx, fy + 2, fz);
			cell_positions[7] = Vector3i(fx + 1, fy + 2, fz);
			cell_positions[8] = Vector3i(fx + 2, fy + 2, fz);
			cell_positions[0x9] = Vector3i(fx, fy, fz + 1);
			cell_positions[0xA] = Vector3i(fx + 2, fy, fz + 1);
			cell_positions[0xB] = Vector3i(fx, fy + 2, fz + 1);
			cell_positions[0xC] = Vector3i(fx + 2, fy + 2, fz + 1);

			// TODO We may not need all of them!
			Vector3 cell_normals[13];
			for (unsigned int i = 0; i < 9; ++i) {

				Vector3i p = cell_positions[i];

				float nx = tof(tos(get_voxel(fvoxels, p - Vector3i(1, 0, 0), channel))) - tof(tos(get_voxel(fvoxels, p + Vector3i(1, 0, 0), channel)));
				float ny = tof(tos(get_voxel(fvoxels, p - Vector3i(0, 1, 0), channel))) - tof(tos(get_voxel(fvoxels, p + Vector3i(0, 1, 0), channel)));
				float nz = tof(tos(get_voxel(fvoxels, p - Vector3i(0, 0, 1), channel))) - tof(tos(get_voxel(fvoxels, p + Vector3i(0, 0, 1), channel)));

				cell_normals[i] = Vector3(nx, ny, nz);
				cell_normals[i].normalize();
			}
			cell_normals[0x9] = cell_normals[0];
			cell_normals[0xA] = cell_normals[2];
			cell_normals[0xB] = cell_normals[6];
			cell_normals[0xC] = cell_normals[8];

			uint16_t case_code = sign(cell_samples[0]);
			case_code |= (sign(cell_samples[1]) << 1);
			case_code |= (sign(cell_samples[2]) << 2);
			case_code |= (sign(cell_samples[5]) << 3);
			case_code |= (sign(cell_samples[8]) << 4);
			case_code |= (sign(cell_samples[7]) << 5);
			case_code |= (sign(cell_samples[6]) << 6);
			case_code |= (sign(cell_samples[3]) << 7);
			case_code |= (sign(cell_samples[4]) << 8);

			if (case_code == 0 || case_code == 511) {
				// The cell contains no triangles.
				continue;
			}

			CRASH_COND(case_code > 511);

			get_reuse_cell_2d(fx, fy).case_index = case_code;

			const uint8_t cell_class = Transvoxel::transitionCellClass[case_code];
			const uint16_t *vertex_data = Transvoxel::transitionVertexData[case_code];

			CRASH_COND((cell_class & 0x7f) > 55);

			const Transvoxel::TransitionCellData cell_data = Transvoxel::transitionCellData[cell_class & 0x7f];
			const bool flip_triangles = ((cell_class >> 7) != 0);

			unsigned int vertex_count = cell_data.GetVertexCount();
			CRASH_COND(vertex_count > 12);
			int cell_vertex_indices[12] = { -1 };

			uint8_t direction_validity_mask = (fx > min_fpos.x ? 1 : 0) | ((fy > min_fpos.y ? 1 : 0) << 1);

			for (unsigned int i = 0; i < vertex_count; ++i) {

				uint16_t edge_code = vertex_data[i];
				uint8_t index_vertex_a = (edge_code >> 4) & 0xf;
				uint8_t index_vertex_b = (edge_code & 0xf);
				CRASH_COND(index_vertex_a > 12);
				CRASH_COND(index_vertex_b > 12);

				int8_t sample_a = cell_samples[index_vertex_a]; // d0 and d1 in the paper
				int8_t sample_b = cell_samples[index_vertex_b];
				// TODO Zero-division is not mentionned in the paper??
				ERR_FAIL_COND(sample_a == sample_b);
				ERR_FAIL_COND(sample_a == 0 && sample_b == 0);

				// Get interpolation position
				// We use an 8-bit fraction, allowing the new vertex to be located at one of 257 possible
				// positions  along  the  edge  when  both  endpoints  are included.
				int t = (sample_b << 8) / (sample_b - sample_a);

				float t0 = static_cast<float>(t) / 256.f;
				float t1 = static_cast<float>(0x100 - t) / 256.f;

				if (t & 0xff) {
					// Vertex lies in the interior of the edge.
					// (i.e t is either 0 or 257, meaning it's either directly on vertex a or vertex b)

					uint8_t vertex_index_to_reuse_or_create = (edge_code >> 8) & 0xf;
					CRASH_COND(vertex_index_to_reuse_or_create > 9);

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

						if (prev.case_index == 0 || prev.case_index == 511) {
							// Previous cell does not contain any geometry.
							cell_vertex_indices[i] = -1;
						} else {
							// Reuse the vertex index from the previous cell.
							cell_vertex_indices[i] = prev.vertices[vertex_index_to_reuse_or_create];
						}
					}

					if (!present || cell_vertex_indices[i] == -1) {
						// Going to create a new vertex

						const Vector3 p0 = cell_positions[index_vertex_a].to_vec3();
						const Vector3 p1 = cell_positions[index_vertex_b].to_vec3();

						const Vector3 n0 = cell_normals[index_vertex_a];
						const Vector3 n1 = cell_normals[index_vertex_b];

						Vector3 primary = p0 * t0 + p1 * t1;
						Vector3 normal = n0 * t0 + n1 * t1;
						// TODO Compute secondary and delta

						cell_vertex_indices[i] = emit_vertex(primary, normal);

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

					uint8_t index_vertex = (t == 0 ? index_vertex_a : index_vertex_b);
					uint8_t corner_data = Transvoxel::transitionCornerData[index_vertex];

					uint8_t vertex_index_to_reuse_or_create = (corner_data & 0xf);
					CRASH_COND(vertex_index_to_reuse_or_create > 9);

					uint8_t reuse_direction = ((corner_data >> 4) & 0xf);

					bool present = (reuse_direction & direction_validity_mask) == reuse_direction;

					if (present) {
						// The previous cell is available. Retrieve the cached cell
						// from which to retrieve the reused vertex index from.

						const ReuseTransitionCell &prev = get_reuse_cell_2d(fx - (reuse_direction & 1), fy - ((reuse_direction >> 1) & 1));

						if (prev.case_index == 0 || prev.case_index == 511) {
							// Previous cell had no geometry
							cell_vertex_indices[i] = -1;

						} else {
							// Reuse the vertex index from the previous cell.
							cell_vertex_indices[i] = prev.vertices[vertex_index_to_reuse_or_create];
						}
					}

					if (!present || cell_vertex_indices[i] == -1) {
						// Going to create a new vertex

						Vector3 primary = cell_positions[index_vertex].to_vec3();
						Vector3 normal = cell_normals[index_vertex];
						// TODO Compute secondary and delta

						cell_vertex_indices[i] = emit_vertex(primary, normal);

						// We are on a corner so the vertex will always be re-usable later
						ReuseTransitionCell &r = get_reuse_cell_2d(fx, fy);
						r.vertices[vertex_index_to_reuse_or_create] = cell_vertex_indices[i];
					}
				}

			} // for vertex

			unsigned int triangle_count = cell_data.GetTriangleCount();

			for (unsigned int ti = 0; ti < triangle_count; ++ti) {
				CRASH_COND(ti * 3 + 2 >= 36);

				if (flip_triangles) {

					_output_indices.push_back(cell_vertex_indices[cell_data.vertexIndex[ti * 3 + 2]]);
					_output_indices.push_back(cell_vertex_indices[cell_data.vertexIndex[ti * 3 + 1]]);
					_output_indices.push_back(cell_vertex_indices[cell_data.vertexIndex[ti * 3]]);

				} else {

					_output_indices.push_back(cell_vertex_indices[cell_data.vertexIndex[ti * 3]]);
					_output_indices.push_back(cell_vertex_indices[cell_data.vertexIndex[ti * 3 + 1]]);
					_output_indices.push_back(cell_vertex_indices[cell_data.vertexIndex[ti * 3 + 2]]);
				}
			}

		} // for x
	} // for y
}

void VoxelMesherTransvoxel::reset_reuse_cells(Vector3i block_size) {
	_block_size = block_size;
	unsigned int deck_area = block_size.x * block_size.y;
	for (int i = 0; i < 2; ++i) {
		_cache[i].clear();
		_cache[i].resize(deck_area);
	}
}

void VoxelMesherTransvoxel::reset_reuse_cells_2d(Vector3i block_size) {
	for (int i = 0; i < 2; ++i) {
		_cache_2d[i].clear();
		_cache_2d[i].resize(block_size.x);
	}
}

VoxelMesherTransvoxel::ReuseCell &VoxelMesherTransvoxel::get_reuse_cell(Vector3i pos) {
	unsigned int j = pos.z & 1;
	unsigned int i = pos.y * _block_size.y + pos.x;
	CRASH_COND(j >= 2);
	CRASH_COND(i >= _cache[j].size());
	return _cache[j][i];
}

VoxelMesherTransvoxel::ReuseTransitionCell &VoxelMesherTransvoxel::get_reuse_cell_2d(int x, int y) {
	unsigned int j = y & 1;
	unsigned int i = x;
	CRASH_COND(j >= 2);
	CRASH_COND(i >= _cache_2d[j].size());
	return _cache_2d[j][i];
}

int VoxelMesherTransvoxel::emit_vertex(Vector3 primary, Vector3 normal) {
	// Could have been offset by 1, but using 2 instead because the VoxelMesher API expects a symetric padding at the moment
	int vi = _output_vertices.size();
	_output_vertices.push_back(primary - Vector3(MINIMUM_PADDING, MINIMUM_PADDING, MINIMUM_PADDING));
	_output_normals.push_back(normal);
	return vi;
}

VoxelMesher *VoxelMesherTransvoxel::clone() {
	return memnew(VoxelMesherTransvoxel);
}

void VoxelMesherTransvoxel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("build_transition_mesh", "voxel_buffer"), &VoxelMesherTransvoxel::build_transition_mesh);
}
