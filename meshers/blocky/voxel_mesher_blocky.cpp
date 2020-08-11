#include "voxel_mesher_blocky.h"
#include "../../cube_tables.h"
#include "../../util/array_slice.h"
#include "../../util/utility.h"
#include <core/os/os.h>

namespace {

template <typename T>
void raw_copy_to(PoolVector<T> &to, const Vector<T> &from) {
	to.resize(from.size());
	typename PoolVector<T>::Write w = to.write();
	memcpy(w.ptr(), from.ptr(), from.size() * sizeof(T));
}

const int g_opposite_side[6] = {
	Cube::SIDE_NEGATIVE_X,
	Cube::SIDE_POSITIVE_X,
	Cube::SIDE_POSITIVE_Y,
	Cube::SIDE_NEGATIVE_Y,
	Cube::SIDE_POSITIVE_Z,
	Cube::SIDE_NEGATIVE_Z
};

inline bool is_face_visible(const VoxelLibrary::BakedData &lib, const Voxel::BakedData &vt, uint32_t other_voxel_id, int side) {
	if (other_voxel_id < lib.models.size()) {
		const Voxel::BakedData &other_vt = lib.models[other_voxel_id];
		// TODO Might test against material somehow, but not only
		if (other_vt.empty || (other_vt.is_transparent && !vt.is_transparent)) {
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

inline bool contributes_to_ao(const VoxelLibrary::BakedData &lib, uint32_t voxel_id) {
	if (voxel_id < lib.models.size()) {
		const Voxel::BakedData &t = lib.models[voxel_id];
		return t.contributes_to_ao;
	}
	return true;
}

} // namespace

template <typename Type_T>
static void generate_blocky_mesh(
		FixedArray<VoxelMesherBlocky::Arrays, VoxelMesherBlocky::MAX_MATERIALS> &out_arrays_per_material,
		const ArraySlice<Type_T> type_buffer,
		const Vector3i block_size,
		const VoxelLibrary::BakedData &library,
		bool bake_occlusion, float baked_occlusion_darkness) {

	// Build lookup tables so to speed up voxel access.
	// These are values to add to an address in order to get given neighbor.

	int row_size = block_size.y;
	int deck_size = block_size.x * row_size;

	// Data must be padded, hence the off-by-one
	Vector3i min = Vector3i(VoxelMesherBlocky::PADDING);
	Vector3i max = block_size - Vector3i(VoxelMesherBlocky::PADDING);

	int index_offsets[VoxelMesherBlocky::MAX_MATERIALS] = { 0 };

	FixedArray<int, Cube::SIDE_COUNT> side_neighbor_lut;
	side_neighbor_lut[Cube::SIDE_LEFT] = row_size;
	side_neighbor_lut[Cube::SIDE_RIGHT] = -row_size;
	side_neighbor_lut[Cube::SIDE_BACK] = -deck_size;
	side_neighbor_lut[Cube::SIDE_FRONT] = deck_size;
	side_neighbor_lut[Cube::SIDE_BOTTOM] = -1;
	side_neighbor_lut[Cube::SIDE_TOP] = 1;

	FixedArray<int, Cube::EDGE_COUNT> edge_neighbor_lut;
	edge_neighbor_lut[Cube::EDGE_BOTTOM_BACK] = side_neighbor_lut[Cube::SIDE_BOTTOM] + side_neighbor_lut[Cube::SIDE_BACK];
	edge_neighbor_lut[Cube::EDGE_BOTTOM_FRONT] = side_neighbor_lut[Cube::SIDE_BOTTOM] + side_neighbor_lut[Cube::SIDE_FRONT];
	edge_neighbor_lut[Cube::EDGE_BOTTOM_LEFT] = side_neighbor_lut[Cube::SIDE_BOTTOM] + side_neighbor_lut[Cube::SIDE_LEFT];
	edge_neighbor_lut[Cube::EDGE_BOTTOM_RIGHT] = side_neighbor_lut[Cube::SIDE_BOTTOM] + side_neighbor_lut[Cube::SIDE_RIGHT];
	edge_neighbor_lut[Cube::EDGE_BACK_LEFT] = side_neighbor_lut[Cube::SIDE_BACK] + side_neighbor_lut[Cube::SIDE_LEFT];
	edge_neighbor_lut[Cube::EDGE_BACK_RIGHT] = side_neighbor_lut[Cube::SIDE_BACK] + side_neighbor_lut[Cube::SIDE_RIGHT];
	edge_neighbor_lut[Cube::EDGE_FRONT_LEFT] = side_neighbor_lut[Cube::SIDE_FRONT] + side_neighbor_lut[Cube::SIDE_LEFT];
	edge_neighbor_lut[Cube::EDGE_FRONT_RIGHT] = side_neighbor_lut[Cube::SIDE_FRONT] + side_neighbor_lut[Cube::SIDE_RIGHT];
	edge_neighbor_lut[Cube::EDGE_TOP_BACK] = side_neighbor_lut[Cube::SIDE_TOP] + side_neighbor_lut[Cube::SIDE_BACK];
	edge_neighbor_lut[Cube::EDGE_TOP_FRONT] = side_neighbor_lut[Cube::SIDE_TOP] + side_neighbor_lut[Cube::SIDE_FRONT];
	edge_neighbor_lut[Cube::EDGE_TOP_LEFT] = side_neighbor_lut[Cube::SIDE_TOP] + side_neighbor_lut[Cube::SIDE_LEFT];
	edge_neighbor_lut[Cube::EDGE_TOP_RIGHT] = side_neighbor_lut[Cube::SIDE_TOP] + side_neighbor_lut[Cube::SIDE_RIGHT];

	FixedArray<int, Cube::CORNER_COUNT> corner_neighbor_lut;

	corner_neighbor_lut[Cube::CORNER_BOTTOM_BACK_LEFT] =
			side_neighbor_lut[Cube::SIDE_BOTTOM] +
			side_neighbor_lut[Cube::SIDE_BACK] +
			side_neighbor_lut[Cube::SIDE_LEFT];

	corner_neighbor_lut[Cube::CORNER_BOTTOM_BACK_RIGHT] =
			side_neighbor_lut[Cube::SIDE_BOTTOM] +
			side_neighbor_lut[Cube::SIDE_BACK] +
			side_neighbor_lut[Cube::SIDE_RIGHT];

	corner_neighbor_lut[Cube::CORNER_BOTTOM_FRONT_RIGHT] =
			side_neighbor_lut[Cube::SIDE_BOTTOM] +
			side_neighbor_lut[Cube::SIDE_FRONT] +
			side_neighbor_lut[Cube::SIDE_RIGHT];

	corner_neighbor_lut[Cube::CORNER_BOTTOM_FRONT_LEFT] =
			side_neighbor_lut[Cube::SIDE_BOTTOM] +
			side_neighbor_lut[Cube::SIDE_FRONT] +
			side_neighbor_lut[Cube::SIDE_LEFT];

	corner_neighbor_lut[Cube::CORNER_TOP_BACK_LEFT] =
			side_neighbor_lut[Cube::SIDE_TOP] +
			side_neighbor_lut[Cube::SIDE_BACK] +
			side_neighbor_lut[Cube::SIDE_LEFT];

	corner_neighbor_lut[Cube::CORNER_TOP_BACK_RIGHT] =
			side_neighbor_lut[Cube::SIDE_TOP] +
			side_neighbor_lut[Cube::SIDE_BACK] +
			side_neighbor_lut[Cube::SIDE_RIGHT];

	corner_neighbor_lut[Cube::CORNER_TOP_FRONT_RIGHT] =
			side_neighbor_lut[Cube::SIDE_TOP] +
			side_neighbor_lut[Cube::SIDE_FRONT] +
			side_neighbor_lut[Cube::SIDE_RIGHT];

	corner_neighbor_lut[Cube::CORNER_TOP_FRONT_LEFT] =
			side_neighbor_lut[Cube::SIDE_TOP] +
			side_neighbor_lut[Cube::SIDE_FRONT] +
			side_neighbor_lut[Cube::SIDE_LEFT];

	//uint64_t time_prep = OS::get_singleton()->get_ticks_usec() - time_before;
	//time_before = OS::get_singleton()->get_ticks_usec();

	for (unsigned int z = min.z; z < (unsigned int)max.z; ++z) {
		for (unsigned int x = min.x; x < (unsigned int)max.x; ++x) {
			for (unsigned int y = min.y; y < (unsigned int)max.y; ++y) {
				// min and max are chosen such that you can visit 1 neighbor away from the current voxel without size check

				const int voxel_index = y + x * row_size + z * deck_size;
				const int voxel_id = type_buffer[voxel_index];

				if (voxel_id != 0 && library.has_model(voxel_id)) {
					const Voxel::BakedData &voxel = library.models[voxel_id];

					VoxelMesherBlocky::Arrays &arrays = out_arrays_per_material[voxel.material_id];
					int &index_offset = index_offsets[voxel.material_id];

					// Hybrid approach: extract cube faces and decimate those that aren't visible,
					// and still allow voxels to have geometry that is not a cube

					// Sides
					for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {
						const std::vector<Vector3> &side_positions = voxel.model.side_positions[side];
						const unsigned int vertex_count = side_positions.size();

						if (vertex_count == 0) {
							continue;
						}

						const uint32_t neighbor_voxel_id = type_buffer[voxel_index + side_neighbor_lut[side]];

						if (!is_face_visible(library, voxel, neighbor_voxel_id, side)) {
							continue;
						}

						// The face is visible

						int shaded_corner[8] = { 0 };

						if (bake_occlusion) {
							// Combinatory solution for https://0fps.net/2013/07/03/ambient-occlusion-for-minecraft-like-worlds/
							// (inverted)
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

						const std::vector<Vector2> &side_uvs = voxel.model.side_uvs[side];

						// Subtracting 1 because the data is padded
						Vector3 pos(x - 1, y - 1, z - 1);

						// Append vertices of the faces in one go, don't use push_back

						{
							const int append_index = arrays.positions.size();
							arrays.positions.resize(arrays.positions.size() + vertex_count);
							Vector3 *w = arrays.positions.data() + append_index;
							for (unsigned int i = 0; i < vertex_count; ++i) {
								w[i] = side_positions[i] + pos;
							}
						}

						{
							const int append_index = arrays.uvs.size();
							arrays.uvs.resize(arrays.uvs.size() + vertex_count);
							memcpy(arrays.uvs.data() + append_index, side_uvs.data(), vertex_count * sizeof(Vector2));
						}

						{
							const int append_index = arrays.normals.size();
							arrays.normals.resize(arrays.normals.size() + vertex_count);
							Vector3 *w = arrays.normals.data() + append_index;
							for (unsigned int i = 0; i < vertex_count; ++i) {
								w[i] = Cube::g_side_normals[side].to_vec3();
							}
						}

						{
							const int append_index = arrays.colors.size();
							arrays.colors.resize(arrays.colors.size() + vertex_count);
							Color *w = arrays.colors.data() + append_index;
							const Color modulate_color = voxel.color;

							if (bake_occlusion) {
								for (unsigned int i = 0; i < vertex_count; ++i) {
									Vector3 v = side_positions[i];

									// General purpose occlusion colouring.
									// TODO Optimize for cubes
									// TODO Fix occlusion inconsistency caused by triangles orientation? Not sure if worth it
									float shade = 0;
									for (unsigned int j = 0; j < 4; ++j) {
										unsigned int corner = Cube::g_side_corners[side][j];
										if (shaded_corner[corner]) {
											float s = baked_occlusion_darkness * static_cast<float>(shaded_corner[corner]);
											//float k = 1.f - Cube::g_corner_position[corner].distance_to(v);
											float k = 1.f - Cube::g_corner_position[corner].distance_squared_to(v);
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

						const std::vector<int> &side_indices = voxel.model.side_indices[side];
						const unsigned int index_count = side_indices.size();

						{
							int i = arrays.indices.size();
							arrays.indices.resize(arrays.indices.size() + index_count);
							int *w = arrays.indices.data();
							for (unsigned int j = 0; j < index_count; ++j) {
								w[i++] = index_offset + side_indices[j];
							}
						}

						index_offset += vertex_count;
					}

					// Inside
					if (voxel.model.positions.size() != 0) {
						// TODO Get rid of push_backs

						const std::vector<Vector3> &positions = voxel.model.positions;
						const unsigned int vertex_count = positions.size();
						const Color modulate_color = voxel.color;

						const std::vector<Vector3> &normals = voxel.model.normals;
						const std::vector<Vector2> &uvs = voxel.model.uvs;

						const Vector3 pos(x - 1, y - 1, z - 1);

						for (unsigned int i = 0; i < vertex_count; ++i) {
							arrays.normals.push_back(normals[i]);
							arrays.uvs.push_back(uvs[i]);
							arrays.positions.push_back(positions[i] + pos);
							// TODO handle ambient occlusion on inner parts
							arrays.colors.push_back(modulate_color);
						}

						const std::vector<int> &indices = voxel.model.indices;
						const unsigned int index_count = indices.size();

						for (unsigned int i = 0; i < index_count; ++i) {
							arrays.indices.push_back(index_offset + indices[i]);
						}

						index_offset += vertex_count;
					}
				}
			}
		}
	}
}

VoxelMesherBlocky::VoxelMesherBlocky() :
		_baked_occlusion_darkness(0.8),
		_bake_occlusion(true) {
	set_padding(PADDING, PADDING);
}

void VoxelMesherBlocky::set_library(Ref<VoxelLibrary> library) {
	_library = library;
}

void VoxelMesherBlocky::set_occlusion_darkness(float darkness) {
	_baked_occlusion_darkness = darkness;
	if (_baked_occlusion_darkness < 0.0) {
		_baked_occlusion_darkness = 0.0;
	} else if (_baked_occlusion_darkness >= 1.0) {
		_baked_occlusion_darkness = 1.0;
	}
}

void VoxelMesherBlocky::set_occlusion_enabled(bool enable) {
	_bake_occlusion = enable;
}

void VoxelMesherBlocky::build(VoxelMesher::Output &output, const VoxelMesher::Input &input) {
	const int channel = VoxelBuffer::CHANNEL_TYPE;

	ERR_FAIL_COND(_library.is_null());

	for (unsigned int i = 0; i < _arrays_per_material.size(); ++i) {
		Arrays &a = _arrays_per_material[i];
		a.positions.clear();
		a.normals.clear();
		a.uvs.clear();
		a.colors.clear();
		a.indices.clear();
	}

	float baked_occlusion_darkness = 0;
	if (_bake_occlusion) {
		baked_occlusion_darkness = _baked_occlusion_darkness / 3.0;
	}

	// The technique is Culled faces.
	// Could be improved with greedy meshing: https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/
	// However I don't feel it's worth it yet:
	// - Not so much gain for organic worlds with lots of texture variations
	// - Works well with cubes but not with any shape
	// - Slower
	// => Could be implemented in a separate class?

	const VoxelBuffer &voxels = input.voxels;
#ifdef TOOLS_ENABLED
	if (input.lod != 0) {
		WARN_PRINT("VoxelMesherBlocky received lod != 0, it is not supported");
	}
#endif

	// Iterate 3D padded data to extract voxel faces.
	// This is the most intensive job in this class, so all required data should be as fit as possible.

	// The buffer we receive MUST be dense (i.e not compressed, and channels allocated).
	// That means we can use raw pointers to voxel data inside instead of using the higher-level getters,
	// and then save a lot of time.

	if (voxels.get_channel_compression(channel) == VoxelBuffer::COMPRESSION_UNIFORM) {
		// All voxels have the same type.
		// If it's all air, nothing to do. If it's all cubes, nothing to do either.
		// TODO Handle edge case of uniform block with non-cubic voxels!
		// If the type of voxel still produces geometry in this situation (which is an absurd use case but not an error),
		// decompress into a backing array to still allow the use of the same algorithm.
		return;

	} else if (voxels.get_channel_compression(channel) != VoxelBuffer::COMPRESSION_NONE) {
		// No other form of compression is allowed
		ERR_PRINT("VoxelMesherBlocky received unsupported voxel compression");
		return;
	}

	ArraySlice<uint8_t> raw_channel;
	if (!voxels.get_channel_raw(channel, raw_channel)) {
		/*       _
		//      | \
		//     /\ \\
		//    / /|\\\
		//    | |\ \\\
		//    | \_\ \\|
		//    |    |  )
		//     \   |  |
		//      \    /
		*/
		// Case supposedly handled before...
		ERR_PRINT("Something wrong happened");
		return;
	}

	const Vector3i block_size = voxels.get_size();
	const VoxelBuffer::Depth channel_depth = voxels.get_channel_depth(channel);

	{
		RWLockRead lock(_library->get_baked_data_rw_lock());
		const VoxelLibrary::BakedData &library_baked_data = _library->get_baked_data();

		switch (channel_depth) {
			case VoxelBuffer::DEPTH_8_BIT:
				generate_blocky_mesh(_arrays_per_material, raw_channel,
						block_size, library_baked_data, _bake_occlusion, baked_occlusion_darkness);
				break;

			case VoxelBuffer::DEPTH_16_BIT:
				generate_blocky_mesh(_arrays_per_material, raw_channel.reinterpret_cast_to<uint16_t>(),
						block_size, library_baked_data, _bake_occlusion, baked_occlusion_darkness);
				break;

			default:
				ERR_PRINT("Unsupported voxel depth");
				return;
		}
	}

	// TODO We could return a single byte array and use Mesh::add_surface down the line?

	for (unsigned int i = 0; i < MAX_MATERIALS; ++i) {
		const Arrays &arrays = _arrays_per_material[i];
		if (arrays.positions.size() != 0) {

			Array mesh_arrays;
			mesh_arrays.resize(Mesh::ARRAY_MAX);

			{
				PoolVector<Vector3> positions;
				PoolVector<Vector2> uvs;
				PoolVector<Vector3> normals;
				PoolVector<Color> colors;
				PoolVector<int> indices;

				raw_copy_to(positions, arrays.positions);
				raw_copy_to(uvs, arrays.uvs);
				raw_copy_to(normals, arrays.normals);
				raw_copy_to(colors, arrays.colors);
				raw_copy_to(indices, arrays.indices);

				mesh_arrays[Mesh::ARRAY_VERTEX] = positions;
				mesh_arrays[Mesh::ARRAY_TEX_UV] = uvs;
				mesh_arrays[Mesh::ARRAY_NORMAL] = normals;
				mesh_arrays[Mesh::ARRAY_COLOR] = colors;
				mesh_arrays[Mesh::ARRAY_INDEX] = indices;
			}

			output.surfaces.push_back(mesh_arrays);

		} else {
			// Empty
			output.surfaces.push_back(Array());
		}
	}

	output.primitive_type = Mesh::PRIMITIVE_TRIANGLES;
}

VoxelMesher *VoxelMesherBlocky::clone() {
	VoxelMesherBlocky *c = memnew(VoxelMesherBlocky);
	c->set_library(_library);
	c->set_occlusion_darkness(_baked_occlusion_darkness);
	c->set_occlusion_enabled(_bake_occlusion);
	return c;
}

void VoxelMesherBlocky::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_library", "voxel_library"), &VoxelMesherBlocky::set_library);
	ClassDB::bind_method(D_METHOD("get_library"), &VoxelMesherBlocky::get_library);

	ClassDB::bind_method(D_METHOD("set_occlusion_enabled", "enable"), &VoxelMesherBlocky::set_occlusion_enabled);
	ClassDB::bind_method(D_METHOD("get_occlusion_enabled"), &VoxelMesherBlocky::get_occlusion_enabled);

	ClassDB::bind_method(D_METHOD("set_occlusion_darkness", "value"), &VoxelMesherBlocky::set_occlusion_darkness);
	ClassDB::bind_method(D_METHOD("get_occlusion_darkness"), &VoxelMesherBlocky::get_occlusion_darkness);
}
