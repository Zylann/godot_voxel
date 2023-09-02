#include "voxel_blocky_model_mesh.h"
#include "../../util/godot/classes/array_mesh.h"
#include "../../util/godot/classes/object.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/string.h"
#include "../../util/math/conv.h"
#include "../../util/math/ortho_basis.h"
#include "../../util/string_funcs.h"
#include "voxel_blocky_library.h"

#include <unordered_map>

namespace zylann::voxel {

void VoxelBlockyModelMesh::set_mesh(Ref<Mesh> mesh) {
	_mesh = mesh;
	if (_mesh.is_valid()) {
		set_surface_count(_mesh->get_surface_count());
	} else {
		set_surface_count(0);
	}
	emit_changed();
}

#ifdef TOOLS_ENABLED
// Generate tangents based on UVs (won't be as good as properly imported tangents)
static PackedFloat32Array generate_tangents_from_uvs(const PackedVector3Array &positions,
		const PackedVector3Array &normals, const PackedVector2Array &uvs, const PackedInt32Array &indices) {
	PackedFloat32Array tangents;
	tangents.resize(positions.size() * 4);

	FixedArray<Vector3f, 3> tri_positions;

	for (int i = 0; i < indices.size(); i += 3) {
		tri_positions[0] = to_vec3f(positions[indices[i]]);
		tri_positions[1] = to_vec3f(positions[indices[i + 1]]);
		tri_positions[2] = to_vec3f(positions[indices[i + 2]]);

		FixedArray<float, 4> tangent;

		const Vector2f delta_uv1 = to_vec2f(uvs[indices[i + 1]] - uvs[indices[i]]);
		const Vector2f delta_uv2 = to_vec2f(uvs[indices[i + 2]] - uvs[indices[i]]);
		const Vector3f delta_pos1 = tri_positions[1] - tri_positions[0];
		const Vector3f delta_pos2 = tri_positions[2] - tri_positions[0];
		const float r = 1.0f / (delta_uv1[0] * delta_uv2[1] - delta_uv1[1] * delta_uv2[0]);
		const Vector3f t = (delta_pos1 * delta_uv2[1] - delta_pos2 * delta_uv1[1]) * r;
		const Vector3f bt = (delta_pos2 * delta_uv1[0] - delta_pos1 * delta_uv2[0]) * r;
		tangent[0] = t[0];
		tangent[1] = t[1];
		tangent[2] = t[2];
		tangent[3] = (math::dot(bt, math::cross(to_vec3f(normals[indices[i]]), t))) < 0 ? -1.0f : 1.0f;

		tangents.append(tangent[0]);
		tangents.append(tangent[1]);
		tangents.append(tangent[2]);
		tangents.append(tangent[3]);
	}

	return tangents;
}
#endif

static void add(Span<Vector3> vectors, Vector3 rhs) {
	for (Vector3 &v : vectors) {
		v += rhs;
	}
}

static void mul(Span<Vector3> vectors, const Basis &basis) {
	for (Vector3 &v : vectors) {
		v = basis.xform(v);
	}
}

inline void add(PackedVector3Array &vectors, Vector3 rhs) {
	add(Span<Vector3>(vectors.ptrw(), vectors.size()), rhs);
}

static void rotate_mesh_arrays(
		PackedVector3Array &vertices, PackedVector3Array &normals, PackedFloat32Array tangents, const Basis &basis) {
	Span<Vector3> vertices_w(vertices.ptrw(), vertices.size());
	Span<Vector3> normals_w(normals.ptrw(), normals.size());
	Span<float> tangents_w(tangents.ptrw(), tangents.size());

	mul(vertices_w, basis);

	if (tangents.size() == 0) {
		mul(normals_w, basis);

	} else {
		const unsigned int tangent_count = tangents_w.size() / 4;
		ZN_ASSERT_RETURN(int(tangent_count) == normals.size());

		for (unsigned int ti = 0; ti < tangent_count; ++ti) {
			const unsigned int i0 = ti * 4;

			Vector3 normal = normals_w[ti];
			Vector3 tangent(tangents_w[i0], tangents_w[i0 + 1], tangents_w[i0 + 2]);
			Vector3 bitangent(normal.cross(tangent) * tangents_w[i0 + 3]);

			normal = basis.xform(normal);
			tangent = basis.xform(tangent);
			bitangent = basis.xform(bitangent);

			const float bitangent_s = math::sign_nonzero(bitangent.dot(normal.cross(tangent)));

			normals_w[ti] = normal;

			tangents_w[i0] = tangent.x;
			tangents_w[i0 + 1] = tangent.y;
			tangents_w[i0 + 2] = tangent.z;
			tangents_w[i0 + 3] = bitangent_s;
		}
	}
}

static void rotate_mesh_arrays_ortho(PackedVector3Array &vertices, PackedVector3Array &normals,
		PackedFloat32Array tangents, unsigned int ortho_basis_index) {
	const math::OrthoBasis ortho_basis = math::get_ortho_basis_from_index(ortho_basis_index);
	const Basis basis(to_vec3(ortho_basis.x), to_vec3(ortho_basis.y), to_vec3(ortho_basis.z));
	rotate_mesh_arrays(vertices, normals, tangents, basis);
}

static void bake_mesh_geometry(Span<const Array> surfaces, Span<const Ref<Material>> materials,
		VoxelBlockyModel::BakedData &baked_data, bool bake_tangents,
		VoxelBlockyModel::MaterialIndexer &material_indexer, unsigned int ortho_rotation) {
	baked_data.model.surface_count = surfaces.size();

	for (unsigned int surface_index = 0; surface_index < surfaces.size(); ++surface_index) {
		const Array &arrays = surfaces[surface_index];

		ERR_CONTINUE(arrays.size() == 0);

		PackedInt32Array indices = arrays[Mesh::ARRAY_INDEX];
		ERR_CONTINUE_MSG(indices.size() % 3 != 0, "Mesh surface is empty or does not contain triangles");

		PackedVector3Array positions = arrays[Mesh::ARRAY_VERTEX];
		PackedVector3Array normals = arrays[Mesh::ARRAY_NORMAL];
		PackedVector2Array uvs = arrays[Mesh::ARRAY_TEX_UV];
		PackedFloat32Array tangents = arrays[Mesh::ARRAY_TANGENT];

		baked_data.empty = positions.size() == 0;

		ZN_ASSERT_CONTINUE(normals.size() != 0);

		ZN_ASSERT_CONTINUE(positions.size() == normals.size());
		// ZN_ASSERT_CONTINUE(positions.size() == uvs.size());
		// ZN_ASSERT_CONTINUE(positions.size() == tangents.size() * 4);

		if (ortho_rotation != math::ORTHOGONAL_BASIS_IDENTITY_INDEX) {
			// Move mesh to origin for easier rotation, since the baked mesh spans 0..1 instead of -0.5..0.5
			// Note: the source mesh won't be modified due to CoW
			add(positions, Vector3(-0.5, -0.5, -0.5));
			rotate_mesh_arrays_ortho(positions, normals, tangents, ortho_rotation);
			add(positions, Vector3(0.5, 0.5, 0.5));
		}

		struct L {
			static uint8_t get_sides(Vector3f pos) {
				uint8_t mask = 0;
				const float tolerance = 0.001;
				mask |= Math::is_equal_approx(pos.x, 0.f, tolerance) << Cube::SIDE_NEGATIVE_X;
				mask |= Math::is_equal_approx(pos.x, 1.f, tolerance) << Cube::SIDE_POSITIVE_X;
				mask |= Math::is_equal_approx(pos.y, 0.f, tolerance) << Cube::SIDE_NEGATIVE_Y;
				mask |= Math::is_equal_approx(pos.y, 1.f, tolerance) << Cube::SIDE_POSITIVE_Y;
				mask |= Math::is_equal_approx(pos.z, 0.f, tolerance) << Cube::SIDE_NEGATIVE_Z;
				mask |= Math::is_equal_approx(pos.z, 1.f, tolerance) << Cube::SIDE_POSITIVE_Z;
				return mask;
			}

			static bool get_triangle_side(
					const Vector3f &a, const Vector3f &b, const Vector3f &c, Cube::SideAxis &out_side) {
				const uint8_t m = get_sides(a) & get_sides(b) & get_sides(c);
				if (m == 0) {
					// At least one of the points doesn't belong to a face
					return false;
				}
				for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {
					if (m == (1 << side)) {
						// All points belong to the same face
						out_side = (Cube::SideAxis)side;
						return true;
					}
				}
				// The triangle isn't in one face
				return false;
			}
		};

#ifdef TOOLS_ENABLED
		const bool tangents_empty = (tangents.size() == 0);
		if (tangents_empty && bake_tangents) {
			if (uvs.size() == 0) {
				// TODO Provide context where the model is used, they can't always be named
				ZN_PRINT_ERROR(format("Voxel model is missing tangents and UVs. The model won't be "
									  "baked. You should consider providing a mesh with tangents, or at least UVs and "
									  "normals, or turn off tangents baking in {}.",
						get_class_name_str<VoxelBlockyLibrary>()));
				continue;
			}
			ZN_PRINT_WARNING(format("Voxel model does not have tangents. They will be generated."
									"You should consider providing a mesh with tangents, or at least UVs and normals, "
									"or turn off tangents baking in {}.",
					get_class_name_str<VoxelBlockyLibrary>()));

			tangents = generate_tangents_from_uvs(positions, normals, uvs, indices);
		}
#endif

		if (uvs.size() == 0) {
			// TODO Properly generate UVs if there arent any
			uvs = PackedVector2Array();
			uvs.resize(positions.size());
		}

		// Separate triangles belonging to faces of the cube

		VoxelBlockyModel::BakedData::Model &model = baked_data.model;

		VoxelBlockyModel::BakedData::Surface &surface = model.surfaces[surface_index];
		Ref<Material> material = materials[surface_index];
		// Note, an empty material counts as "The default material".
		surface.material_id = material_indexer.get_or_create_index(material);

		FixedArray<std::unordered_map<int, int>, Cube::SIDE_COUNT> added_side_indices;
		std::unordered_map<int, int> added_regular_indices;
		FixedArray<Vector3f, 3> tri_positions;

		for (int i = 0; i < indices.size(); i += 3) {
			tri_positions[0] = to_vec3f(positions[indices[i]]);
			tri_positions[1] = to_vec3f(positions[indices[i + 1]]);
			tri_positions[2] = to_vec3f(positions[indices[i + 2]]);

			Cube::SideAxis side;
			if (L::get_triangle_side(tri_positions[0], tri_positions[1], tri_positions[2], side)) {
				// That triangle is on the face

				int next_side_index = surface.side_positions[side].size();

				for (int j = 0; j < 3; ++j) {
					const int src_index = indices[i + j];
					std::unordered_map<int, int> &added_indices = added_side_indices[side];
					const auto existing_dst_index_it = added_indices.find(src_index);

					if (existing_dst_index_it == added_indices.end()) {
						// Add new vertex

						surface.side_indices[side].push_back(next_side_index);
						surface.side_positions[side].push_back(tri_positions[j]);
						surface.side_uvs[side].push_back(to_vec2f(uvs[indices[i + j]]));

						if (bake_tangents) {
							// i is the first vertex of each triangle which increments by steps of 3.
							// There are 4 floats per tangent.
							int ti = indices[i + j] * 4;
							surface.side_tangents[side].push_back(tangents[ti]);
							surface.side_tangents[side].push_back(tangents[ti + 1]);
							surface.side_tangents[side].push_back(tangents[ti + 2]);
							surface.side_tangents[side].push_back(tangents[ti + 3]);
						}

						added_indices.insert({ src_index, next_side_index });
						++next_side_index;

					} else {
						// Vertex was already added, just add index referencing it
						surface.side_indices[side].push_back(existing_dst_index_it->second);
					}
				}

			} else {
				// That triangle is not on the face

				int next_regular_index = surface.positions.size();

				for (int j = 0; j < 3; ++j) {
					const int src_index = indices[i + j];
					const auto existing_dst_index_it = added_regular_indices.find(src_index);

					if (existing_dst_index_it == added_regular_indices.end()) {
						surface.indices.push_back(next_regular_index);
						surface.positions.push_back(tri_positions[j]);
						surface.normals.push_back(to_vec3f(normals[indices[i + j]]));
						surface.uvs.push_back(to_vec2f(uvs[indices[i + j]]));

						if (bake_tangents) {
							// i is the first vertex of each triangle which increments by steps of 3.
							// There are 4 floats per tangent.
							int ti = indices[i + j] * 4;
							surface.tangents.push_back(tangents[ti]);
							surface.tangents.push_back(tangents[ti + 1]);
							surface.tangents.push_back(tangents[ti + 2]);
							surface.tangents.push_back(tangents[ti + 3]);
						}

						added_regular_indices.insert({ src_index, next_regular_index });
						++next_regular_index;

					} else {
						surface.indices.push_back(existing_dst_index_it->second);
					}
				}
			}
		}
	}
}

static void bake_mesh_geometry(const VoxelBlockyModelMesh &config, VoxelBlockyModel::BakedData &baked_data,
		bool bake_tangents, VoxelBlockyModel::MaterialIndexer &material_indexer) {
	Ref<Mesh> mesh = config.get_mesh();

	if (mesh.is_null()) {
		baked_data.empty = true;
		return;
	}

	// TODO Merge surfaces if they are found to have the same material (but still print a warning if their material is
	// different or is null)
	if (mesh->get_surface_count() > int(VoxelBlockyModel::BakedData::Model::MAX_SURFACES)) {
		ZN_PRINT_WARNING(format("Mesh has more than {} surfaces, extra surfaces will not be baked.",
				VoxelBlockyModel::BakedData::Model::MAX_SURFACES));
	}

	const unsigned int surface_count =
			math::min(uint32_t(mesh->get_surface_count()), VoxelBlockyModel::BakedData::Model::MAX_SURFACES);

	std::vector<Ref<Material>> materials;
	std::vector<Array> surfaces;

	for (unsigned int i = 0; i < surface_count; ++i) {
		surfaces.push_back(mesh->surface_get_arrays(i));
		materials.push_back(mesh->surface_get_material(i));
	}

	bake_mesh_geometry(to_span(surfaces), to_span(materials), baked_data, bake_tangents, material_indexer,
			config.get_mesh_ortho_rotation_index());
}

void VoxelBlockyModelMesh::bake(BakedData &baked_data, bool bake_tangents, MaterialIndexer &materials) const {
	baked_data.clear();
	bake_mesh_geometry(*this, baked_data, bake_tangents, materials);
	VoxelBlockyModel::bake(baked_data, bake_tangents, materials);
}

bool VoxelBlockyModelMesh::is_empty() const {
	if (_mesh.is_null()) {
		return true;
	}
	Ref<ArrayMesh> array_mesh = _mesh;
	if (array_mesh.is_valid()) {
		return is_mesh_empty(**array_mesh);
	}
	return false;
}

void VoxelBlockyModelMesh::set_mesh_ortho_rotation_index(int i) {
	ZN_ASSERT_RETURN(i >= 0 && i < math::ORTHOGONAL_BASIS_COUNT);
	if (i != int(_mesh_ortho_rotation)) {
		_mesh_ortho_rotation = i;
	}
}

int VoxelBlockyModelMesh::get_mesh_ortho_rotation_index() const {
	return _mesh_ortho_rotation;
}

Ref<Mesh> VoxelBlockyModelMesh::get_preview_mesh() const {
	const float bake_tangents = false;
	VoxelBlockyModel::BakedData baked_data;
	baked_data.color = get_color();
	std::vector<Ref<Material>> materials;
	MaterialIndexer material_indexer{ materials };
	bake_mesh_geometry(*this, baked_data, bake_tangents, material_indexer);

	Ref<Mesh> mesh = make_mesh_from_baked_data(baked_data, bake_tangents);

	for (unsigned int surface_index = 0; surface_index < baked_data.model.surface_count; ++surface_index) {
		const BakedData::Surface &surface = baked_data.model.surfaces[surface_index];
		Ref<Material> material = materials[surface.material_id];
		Ref<Material> material_override = get_material_override(surface_index);
		if (material_override.is_valid()) {
			material = material_override;
		}
		mesh->surface_set_material(surface_index, material);
	}

	return mesh;
}

void VoxelBlockyModelMesh::rotate_90(math::Axis axis, bool clockwise) {
	math::OrthoBasis ortho_basis = math::get_ortho_basis_from_index(_mesh_ortho_rotation);
	ortho_basis.rotate_90(axis, clockwise);
	_mesh_ortho_rotation = math::get_index_from_ortho_basis(ortho_basis);

	rotate_collision_boxes_90(axis, clockwise);

	emit_changed();
}

void VoxelBlockyModelMesh::rotate_ortho(math::OrthoBasis p_ortho_basis) {
	math::OrthoBasis ortho_basis = math::get_ortho_basis_from_index(_mesh_ortho_rotation);
	ortho_basis = p_ortho_basis * ortho_basis;
	_mesh_ortho_rotation = math::get_index_from_ortho_basis(ortho_basis);

	rotate_collision_boxes_ortho(p_ortho_basis);

	emit_changed();
}

void VoxelBlockyModelMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &VoxelBlockyModelMesh::set_mesh);
	ClassDB::bind_method(D_METHOD("get_mesh"), &VoxelBlockyModelMesh::get_mesh);

	ClassDB::bind_method(
			D_METHOD("set_mesh_ortho_rotation_index", "i"), &VoxelBlockyModelMesh::set_mesh_ortho_rotation_index);
	ClassDB::bind_method(
			D_METHOD("get_mesh_ortho_rotation_index"), &VoxelBlockyModelMesh::get_mesh_ortho_rotation_index);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, Mesh::get_class_static()),
			"set_mesh", "get_mesh");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_ortho_rotation_index", PROPERTY_HINT_RANGE, "0,24"),
			"set_mesh_ortho_rotation_index", "get_mesh_ortho_rotation_index");
}

} // namespace zylann::voxel
