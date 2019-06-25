
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

const Vector3i g_corner_dirs[8] = {
	Vector3i(0, 0, 0),
	Vector3i(1, 0, 0),
	Vector3i(0, 1, 0),
	Vector3i(1, 1, 0),
	Vector3i(0, 0, 1),
	Vector3i(1, 0, 1),
	Vector3i(0, 1, 1),
	Vector3i(1, 1, 1)
};

inline Vector3i dir_to_prev_vec(uint8_t dir) {
	//return g_corner_dirs[mask] - Vector3(1,1,1);
	return Vector3i(
			-(dir & 1),
			-((dir >> 1) & 1),
			-((dir >> 2) & 1));
}

template <typename T>
void copy_to(PoolVector<T> &to, Vector<T> &from) {

	to.resize(from.size());

	typename PoolVector<T>::Write w = to.write();

	for (unsigned int i = 0; i < (unsigned int)from.size(); ++i) {
		w[i] = from[i];
	}
}

} // namespace

VoxelMesherTransvoxel::ReuseCell::ReuseCell() {
	case_index = 0;
	for (unsigned int i = 0; i < 4; ++i) {
		vertices[i] = -1;
	}
}

int VoxelMesherTransvoxel::get_minimum_padding() const {
	return MINIMUM_PADDING;
}

void VoxelMesherTransvoxel::build(VoxelMesher::Output &output, const VoxelBuffer &voxels, int padding) {

	ERR_FAIL_COND(padding < MINIMUM_PADDING);

	int channel = VoxelBuffer::CHANNEL_ISOLEVEL;

	// Initialize dynamic memory:
	// These vectors are re-used.
	// We don't know in advance how much geometry we are going to produce.
	// Once capacity is big enough, no more memory should be allocated
	m_output_vertices.clear();
	//m_output_vertices_secondary.clear();
	m_output_normals.clear();
	m_output_indices.clear();

	build_internal(voxels, channel);
	//	OS::get_singleton()->print("vertices: %i, normals: %i, indices: %i\n",
	//							   m_output_vertices.size(),
	//							   m_output_normals.size(),
	//							   m_output_indices.size());

	if (m_output_vertices.size() == 0) {
		// The mesh can be empty
		return;
	}

	PoolVector<Vector3> vertices;
	PoolVector<Vector3> normals;
	PoolVector<int> indices;

	copy_to(vertices, m_output_vertices);
	copy_to(normals, m_output_normals);
	copy_to(indices, m_output_indices);

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	if (m_output_normals.size() != 0) {
		arrays[Mesh::ARRAY_NORMAL] = normals;
	}
	arrays[Mesh::ARRAY_INDEX] = indices;

	output.surfaces.push_back(arrays);
	output.primitive_type = Mesh::PRIMITIVE_TRIANGLES;
}

void VoxelMesherTransvoxel::build_internal(const VoxelBuffer &voxels, unsigned int channel) {

	// Each 2x2 voxel group is a "cell"

	if (voxels.is_uniform(channel)) {
		// Nothing to extract, because constant isolevels never cross the threshold and describe no surface
		return;
	}

	const Vector3i block_size = voxels.get_size();
	// TODO No lod yet, but it's planned
	//const int lod_index = 0;
	//const int lod_scale = 1 << lod_index;

	// Prepare vertex reuse cache
	m_block_size = block_size;
	unsigned int deck_area = block_size.x * block_size.y;
	for (int i = 0; i < 2; ++i) {
		if ((unsigned int)m_cache[i].size() != deck_area) {
			m_cache[i].clear(); // Clear any previous data
			m_cache[i].resize(deck_area);
		}
	}

	// Iterate all cells with padding (expected to be neighbors)
	Vector3i pos;
	for (pos.z = PAD.z; pos.z < block_size.z - 2; ++pos.z) {
		for (pos.y = PAD.y; pos.y < block_size.y - 2; ++pos.y) {
			for (pos.x = PAD.x; pos.x < block_size.x - 2; ++pos.x) {

				// Get the value of cells.
				// Negative values are "solid" and positive are "air".
				// Due to raw cells being unsigned 8-bit, they get converted to signed.
				int8_t cell_samples[8] = {
					tos(voxels.get_voxel(pos.x, pos.y, pos.z, channel)),
					tos(voxels.get_voxel(pos.x + 1, pos.y, pos.z, channel)),
					tos(voxels.get_voxel(pos.x, pos.y + 1, pos.z, channel)),
					tos(voxels.get_voxel(pos.x + 1, pos.y + 1, pos.z, channel)),
					tos(voxels.get_voxel(pos.x, pos.y, pos.z + 1, channel)),
					tos(voxels.get_voxel(pos.x + 1, pos.y, pos.z + 1, channel)),
					tos(voxels.get_voxel(pos.x, pos.y + 1, pos.z + 1, channel)),
					tos(voxels.get_voxel(pos.x + 1, pos.y + 1, pos.z + 1, channel))
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

				// TODO We might not always need all of them
				// Compute normals
				Vector3 corner_normals[8];
				for (unsigned int i = 0; i < 8; ++i) {

					Vector3i p = pos + g_corner_dirs[i];

					float nx = tof(tos(voxels.get_voxel(p - Vector3i(1, 0, 0), channel))) - tof(tos(voxels.get_voxel(p + Vector3i(1, 0, 0), channel)));
					float ny = tof(tos(voxels.get_voxel(p - Vector3i(0, 1, 0), channel))) - tof(tos(voxels.get_voxel(p + Vector3i(0, 1, 0), channel)));
					float nz = tof(tos(voxels.get_voxel(p - Vector3i(0, 0, 1), channel))) - tof(tos(voxels.get_voxel(p + Vector3i(0, 0, 1), channel)));

					corner_normals[i] = Vector3(nx, ny, nz);
					corner_normals[i].normalize();
				}

				// For cells occurring along the minimal boundaries of a block,
				// the preceding cells needed for vertex reuse may not exist.
				// In these cases, we allow new vertex creation on additional edges of a cell.
				// While iterating through the cells in a block, a 3-bit mask is maintained whose bits indicate
				// whether corresponding bits in a direction code are valid
				uint8_t direction_validity_mask =
						(pos.x > 1 ? 1 : 0) | ((pos.y > 1 ? 1 : 0) << 1) | ((pos.z > 1 ? 1 : 0) << 2);

				uint8_t regular_cell_class_index = Transvoxel::regularCellClass[case_code];
				Transvoxel::RegularCellData regular_cell_class = Transvoxel::regularCellData[regular_cell_class_index];
				uint8_t triangle_count = regular_cell_class.geometryCounts & 0x0f;
				uint8_t vertex_count = (regular_cell_class.geometryCounts & 0xf0) >> 4;

				int cell_mesh_indices[12];

				// For each vertex in the case
				for (unsigned int i = 0; i < vertex_count; ++i) {

					// The case index maps to a list of 16-bit codes providing information about the edges on which the vertices lie.
					// The low byte of each 16-bit code contains the corner indexes of the edge’s endpoints in one nibble each,
					// and the high byte contains the mapping code shown in Figure 3.8(b)
					unsigned short rvd = Transvoxel::regularVertexData[case_code][i];
					unsigned short edge_code_low = rvd & 0xff;
					unsigned short edge_code_high = (rvd >> 8) & 0xff;

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
					float t1 = static_cast<float>(0x0100 - t) / 256.f;

					Vector3i p0 = pos + g_corner_dirs[v0];
					Vector3i p1 = pos + g_corner_dirs[v1];

					if (t & 0xff) {
						//OS::get_singleton()->print("A");
						// Vertex lies in the interior of the edge.

						// Each edge of a cell is assigned an 8-bit code, as shown in Figure 3.8(b),
						// that provides a mapping to a preceding cell and the coincident edge on that preceding cell
						// for which new vertex creation  was  allowed.
						// The high nibble of this code indicates which direction to go in order to reach the correct preceding cell.
						// The bit values 1, 2, and 4 in this nibble indicate that we must subtract one
						// from the x, y, and/or z coordinate, respectively.
						uint8_t reuse_dir = (edge_code_high >> 4) & 0xf;
						uint8_t reuse_vertex_index = edge_code_high & 0xf;

						bool can_reuse = (reuse_dir & direction_validity_mask) == reuse_dir;

						if (can_reuse) {
							Vector3i cache_pos = pos + dir_to_prev_vec(reuse_dir);
							ReuseCell &prev_cell = get_reuse_cell(cache_pos);

							if (prev_cell.case_index == 0 || prev_cell.case_index == 255) {
								// TODO I don't think this can happen for non-corner vertices.
								cell_mesh_indices[i] = -1;
							} else {
								// Will reuse a previous vertice
								cell_mesh_indices[i] = prev_cell.vertices[reuse_vertex_index];
							}
						}

						if (!can_reuse || cell_mesh_indices[i] == -1) {
							// Going to create a new vertice

							cell_mesh_indices[i] = m_output_vertices.size();

							Vector3 pi = p0.to_vec3() * t0 + p1.to_vec3() * t1;

							Vector3 primary = pi; //pos.to_vec3() + pi;
							Vector3 normal = corner_normals[v0] * t0 + corner_normals[v1] * t1;

							emit_vertex(primary, normal);

							if (reuse_dir & 8) {
								// Store the generated vertex so that other cells can reuse it.
								ReuseCell &rc = get_reuse_cell(pos);
								rc.vertices[reuse_vertex_index] = cell_mesh_indices[i];
							}
						}

					} else if (t == 0 && v1 == 7) {

						//OS::get_singleton()->print("B");
						// This cell owns the vertex, so it should be created.

						cell_mesh_indices[i] = m_output_vertices.size();

						Vector3 pi = p0.to_vec3() * t0 + p1.to_vec3() * t1;
						Vector3 primary = pi; //pos.to_vec3() + pi;
						Vector3 normal = corner_normals[v0] * t0 + corner_normals[v1] * t1;

						emit_vertex(primary, normal);

						ReuseCell &rc = get_reuse_cell(pos);
						rc.vertices[0] = cell_mesh_indices[i];

					} else {
						// Always try to reuse previous vertices in these cases

						//OS::get_singleton()->print("C");
						// A 3-bit direction code leading to the proper cell can easily be obtained by
						// inverting the 3-bit corner index (bitwise, by exclusive ORing with the number 7).
						// The corner index depends on the value of t, t = 0 means that we're at the higher
						// numbered endpoint.
						uint8_t reuse_dir = (t == 0 ? v1 ^ 7 : v0 ^ 7);
						bool can_reuse = (reuse_dir & direction_validity_mask) == reuse_dir;

						// Note: the only difference with similar code above is that we take vertice 0 in the `else`
						if (can_reuse) {
							Vector3i cache_pos = pos + dir_to_prev_vec(reuse_dir);
							ReuseCell prev_cell = get_reuse_cell(cache_pos);

							// The previous cell might not have any geometry, and we
							// might therefore have to create a new vertex anyway.
							if (prev_cell.case_index == 0 || prev_cell.case_index == 255) {
								cell_mesh_indices[i] = -1;
							} else {
								cell_mesh_indices[i] = prev_cell.vertices[0];
							}
						}

						if (!can_reuse || cell_mesh_indices[i] < 0) {
							cell_mesh_indices[i] = m_output_vertices.size();

							Vector3 pi = p0.to_vec3() * t0 + p1.to_vec3() * t1;
							Vector3 primary = pi; //pos.to_vec3() + pi;
							Vector3 normal = corner_normals[v0] * t0 + corner_normals[v1] * t1;

							emit_vertex(primary, normal);
						}
					}

				} // for each cell vertice

				//OS::get_singleton()->print("_");

				for (int t = 0; t < triangle_count; ++t) {
					for (int i = 0; i < 3; ++i) {
						int index = cell_mesh_indices[regular_cell_class.vertexIndex[t * 3 + i]];
						m_output_indices.push_back(index);
					}
				}

			} // x
		} // y
	} // z

	//OS::get_singleton()->print("\n");
}

VoxelMesherTransvoxel::ReuseCell &VoxelMesherTransvoxel::get_reuse_cell(Vector3i pos) {
	int j = pos.z & 1;
	int i = pos.y * m_block_size.y + pos.x;
	return m_cache[j].write[i];
}

void VoxelMesherTransvoxel::emit_vertex(Vector3 primary, Vector3 normal) {
	m_output_vertices.push_back(primary - PAD.to_vec3());
	m_output_normals.push_back(normal);
}

VoxelMesher *VoxelMesherTransvoxel::clone() {
	return memnew(VoxelMesherTransvoxel);
}

void VoxelMesherTransvoxel::_bind_methods() {
}
