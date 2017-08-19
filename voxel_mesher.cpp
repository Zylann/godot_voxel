#include "voxel_mesher.h"
#include "voxel_library.h"
#include "cube_tables.h"


template <typename T>
void copy_to(PoolVector<T> &to, const Vector<T> &from) {

	to.resize(from.size());

	typename PoolVector<T>::Write w = to.write();

	for (unsigned int i = 0; i < from.size(); ++i) {
		w[i] = from[i];
	}
}


VoxelMesher::VoxelMesher()
	: _baked_occlusion_darkness(0.75),
	  _bake_occlusion(true) {}

void VoxelMesher::set_library(Ref<VoxelLibrary> library) {
	_library = library;
}

void VoxelMesher::set_material(Ref<Material> material, unsigned int id) {
	ERR_FAIL_COND(id >= MAX_MATERIALS);
	_materials[id] = material;
}

Ref<Material> VoxelMesher::get_material(unsigned int id) const {
	ERR_FAIL_COND_V(id >= MAX_MATERIALS, Ref<Material>());
	return _materials[id];
}

void VoxelMesher::set_occlusion_darkness(float darkness) {
	_baked_occlusion_darkness = darkness;
	if (_baked_occlusion_darkness < 0.0)
		_baked_occlusion_darkness = 0.0;
	else if (_baked_occlusion_darkness >= 1.0)
		_baked_occlusion_darkness = 1.0;
}

void VoxelMesher::set_occlusion_enabled(bool enable) {
	_bake_occlusion = enable;
}

inline Color Color_greyscale(float c) {
	return Color(c, c, c);
}

inline bool is_face_visible(const VoxelLibrary &lib, const Voxel &vt, int other_voxel_id) {
	if (other_voxel_id == 0) // air
		return true;
	if (lib.has_voxel(other_voxel_id)) {
		const Voxel &other_vt = lib.get_voxel_const(other_voxel_id);
		return other_vt.is_transparent() && vt.get_id() != other_voxel_id;
	}
	return true;
}

inline bool is_transparent(const VoxelLibrary &lib, int voxel_id) {
	if (lib.has_voxel(voxel_id))
		return lib.get_voxel_const(voxel_id).is_transparent();
	return true;
}

Ref<ArrayMesh> VoxelMesher::build_ref(Ref<VoxelBuffer> buffer_ref, unsigned int channel, Ref<ArrayMesh> mesh) {
	ERR_FAIL_COND_V(buffer_ref.is_null(), Ref<ArrayMesh>());
	VoxelBuffer &buffer = **buffer_ref;
	mesh = build(buffer, channel, Vector3i(), buffer.get_size(), mesh);
	return mesh;
}

Ref<ArrayMesh> VoxelMesher::build(const VoxelBuffer &buffer, unsigned int channel, Vector3i min, Vector3i max, Ref<ArrayMesh> mesh) {
	ERR_FAIL_COND_V(_library.is_null(), Ref<ArrayMesh>());
	ERR_FAIL_COND_V(channel >= VoxelBuffer::MAX_CHANNELS, Ref<ArrayMesh>());

	const VoxelLibrary &library = **_library;

	for (unsigned int i = 0; i < MAX_MATERIALS; ++i) {
		Arrays &a = _arrays[i];
		a.positions.clear();
		a.normals.clear();
		a.uvs.clear();
		a.colors.clear();
		a.indices.clear();
	}

	float baked_occlusion_darkness;
	if (_bake_occlusion)
		baked_occlusion_darkness = _baked_occlusion_darkness / 3.0;

	// The technique is Culled faces.
	// Could be improved with greedy meshing: https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/
	// However I don't feel it's worth it yet:
	// - Not so much gain for organic worlds with lots of texture variations
	// - Works well with cubes but not with any shape
	// - Slower
	// => Could be implemented in a separate class?

	VOXEL_PROFILE_BEGIN("mesher_face_extraction")

	// Data must be padded, hence the off-by-one
	Vector3i::sort_min_max(min, max);
	const Vector3i pad(1, 1, 1);
	min.clamp_to(pad, max);
	max.clamp_to(min, buffer.get_size() - pad);

	int index_offset = 0;

	// Iterate 3D padded data to extract voxel faces.
	// This is the most intensive job in this class, so all required data should be as fit as possible.
	for (unsigned int z = min.z; z < max.z; ++z) {
		for (unsigned int x = min.x; x < max.x; ++x) {
			for (unsigned int y = min.y; y < max.y; ++y) {

				// TODO In this intensive routine, there is a way to make voxel access fastest by getting a pointer to the channel,
				// and using offset lookup to get neighbors rather than going through get_voxel validations
				int voxel_id = buffer.get_voxel(x, y, z, 0);

				if (voxel_id != 0 && library.has_voxel(voxel_id)) {

					const Voxel &voxel = library.get_voxel_const(voxel_id);

					Arrays &arrays = _arrays[voxel.get_material_id()];

					// Hybrid approach: extract cube faces and decimate those that aren't visible,
					// and still allow voxels to have geometry that is not a cube

					// Sides
					for (unsigned int side = 0; side < Voxel::SIDE_COUNT; ++side) {

						const PoolVector<Vector3> &positions = voxel.get_model_side_positions(side);
						int vertex_count = positions.size();

						if (vertex_count != 0) {

							Vector3i normal = CubeTables::g_side_normals[side];
							unsigned nx = x + normal.x;
							unsigned ny = y + normal.y;
							unsigned nz = z + normal.z;

							int neighbor_voxel_id = buffer.get_voxel(nx, ny, nz, channel);
							// TODO Better face visibility test
							if (is_face_visible(library, voxel, neighbor_voxel_id)) {

								// The face is visible

								int shaded_corner[8] = { 0 };

								if (_bake_occlusion) {

									// Combinatory solution for https://0fps.net/2013/07/03/ambient-occlusion-for-minecraft-like-worlds/

									for (unsigned int j = 0; j < 4; ++j) {
										unsigned int edge = CubeTables::g_side_edges[side][j];
										Vector3i edge_normal = CubeTables::g_edge_inormals[edge];
										unsigned ex = x + edge_normal.x;
										unsigned ey = y + edge_normal.y;
										unsigned ez = z + edge_normal.z;
										if (!is_transparent(library, buffer.get_voxel(ex, ey, ez))) {
											shaded_corner[CubeTables::g_edge_corners[edge][0]] += 1;
											shaded_corner[CubeTables::g_edge_corners[edge][1]] += 1;
										}
									}
									for (unsigned int j = 0; j < 4; ++j) {
										unsigned int corner = CubeTables::g_side_corners[side][j];
										if (shaded_corner[corner] == 2) {
											shaded_corner[corner] = 3;
										} else {
											Vector3i corner_normal = CubeTables::g_corner_inormals[corner];
											unsigned int cx = x + corner_normal.x;
											unsigned int cy = y + corner_normal.y;
											unsigned int cz = z + corner_normal.z;
											if (!is_transparent(library, buffer.get_voxel(cx, cy, cz))) {
												shaded_corner[corner] += 1;
											}
										}
									}
								}

								PoolVector<Vector3>::Read rv = positions.read();
								PoolVector<Vector2>::Read rt = voxel.get_model_side_uv(side).read();

								Vector3 pos(x - 1, y - 1, z - 1);

								for (unsigned int i = 0; i < vertex_count; ++i) {
									Vector3 v = rv[i];

									if (_bake_occlusion) {
										// General purpose occlusion colouring.
										// TODO Optimize for cubes
										// TODO Fix occlusion inconsistency caused by triangles orientation
										float shade = 0;
										for (unsigned int j = 0; j < 4; ++j) {
											unsigned int corner = CubeTables::g_side_corners[side][j];
											if (shaded_corner[corner]) {
												float s = baked_occlusion_darkness * static_cast<float>(shaded_corner[corner]);
												float k = 1.0 - CubeTables::g_corner_position[corner].distance_to(v);
												if (k < 0.0)
													k = 0.0;
												s *= k;
												if (s > shade)
													shade = s;
											}
										}
										float gs = 1.0 - shade;
										arrays.colors.push_back(Color(gs, gs, gs));
									}

									// TODO Investigate wether those vectors can be replaced by a simpler, faster one for PODs
									// TODO Resize beforehands rather than push_back (even if the vector is preallocated)
									arrays.normals.push_back(Vector3(normal.x, normal.y, normal.z));
									arrays.uvs.push_back(rt[i]);
									arrays.positions.push_back(v + pos);
								}

								const PoolVector<int> &side_indices = voxel.get_model_side_indices(side);
								PoolVector<int>::Read ri = side_indices.read();
								unsigned int index_count = side_indices.size();

								for(unsigned int i = 0; i < index_count; ++i) {
									arrays.indices.push_back(index_offset + ri[i]);
								}

								index_offset += vertex_count;
							}
						}
					}

					// Inside
					if (voxel.get_model_positions().size() != 0) {

						const PoolVector<Vector3> &vertices = voxel.get_model_positions();
						int vertex_count = vertices.size();

						PoolVector<Vector3>::Read rv = vertices.read();
						PoolVector<Vector3>::Read rn = voxel.get_model_normals().read();
						PoolVector<Vector2>::Read rt = voxel.get_model_uv().read();

						Vector3 pos(x - 1, y - 1, z - 1);

						for (unsigned int i = 0; i < vertex_count; ++i) {
							arrays.normals.push_back(rn[i]);
							arrays.uvs.push_back(rt[i]);
							arrays.positions.push_back(rv[i] + pos);
						}

						if(_bake_occlusion) {
							// TODO handle ambient occlusion on inner parts
							arrays.colors.push_back(Color(1,1,1));
						}

						const PoolVector<int> &indices = voxel.get_model_indices();
						PoolVector<int>::Read ri = indices.read();
						unsigned int index_count = indices.size();

						for(unsigned int i = 0; i < index_count; ++i) {
							arrays.indices.push_back(index_offset + ri[i]);
						}

						index_offset += vertex_count;
					}
				}
			}
		}
	}

	VOXEL_PROFILE_END("mesher_face_extraction")

	// Commit mesh

	Ref<ArrayMesh> mesh_ref = mesh;
	if (mesh.is_null())
		mesh_ref = Ref<ArrayMesh>(memnew(ArrayMesh));

	VOXEL_PROFILE_BEGIN("mesher_add_surfaces")

//	print_line(String("Made mesh v: ") + String::num(_arrays[0].positions.size())
//			+ String(", i: ") + String::num(_arrays[0].indices.size()));

	int surface = 0;
	for(int i = 0; i < MAX_MATERIALS; ++i) {

		const Arrays &arrays = _arrays[i];
		if(arrays.positions.size() != 0) {

			Array mesh_arrays;
			mesh_arrays.resize(Mesh::ARRAY_MAX);

			{
				PoolVector<Vector3> positions;
				PoolVector<Vector2> uvs;
				PoolVector<Vector3> normals;
				PoolVector<Color> colors;
				PoolVector<int> indices;

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
			}

			mesh_ref->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, mesh_arrays);
			mesh_ref->surface_set_material(surface, _materials[i]);

			++surface;
		}
	}

	VOXEL_PROFILE_END("mesher_add_surfaces")

	return mesh_ref;
}

void VoxelMesher::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_material", "material", "id"), &VoxelMesher::set_material);
	ClassDB::bind_method(D_METHOD("get_material", "id"), &VoxelMesher::get_material);

	ClassDB::bind_method(D_METHOD("set_library", "voxel_library"), &VoxelMesher::set_library);
	ClassDB::bind_method(D_METHOD("get_library"), &VoxelMesher::get_library);

	ClassDB::bind_method(D_METHOD("set_occlusion_enabled", "enable"), &VoxelMesher::set_occlusion_enabled);
	ClassDB::bind_method(D_METHOD("get_occlusion_enabled"), &VoxelMesher::get_occlusion_enabled);

	ClassDB::bind_method(D_METHOD("set_occlusion_darkness", "value"), &VoxelMesher::set_occlusion_darkness);
	ClassDB::bind_method(D_METHOD("get_occlusion_darkness"), &VoxelMesher::get_occlusion_darkness);

	ClassDB::bind_method(D_METHOD("build", "voxel_buffer", "channel", "existing_mesh"), &VoxelMesher::build_ref);

#ifdef VOXEL_PROFILING
	ClassDB::bind_method(D_METHOD("get_profiling_info"), &VoxelMesher::get_profiling_info);
#endif
}
