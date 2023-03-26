#include "voxel_blocky_model.h"
#include "../../util/godot/classes/base_material_3d.h"
#include "../../util/godot/classes/shader_material.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/string.h"
#include "../../util/godot/funcs.h"
#include "../../util/macros.h"
#include "../../util/math/conv.h"
#include "../../util/string_funcs.h"
#include "voxel_blocky_library.h"

// TODO Only required because of MAX_MATERIALS... could be enough inverting that dependency
#include "voxel_mesher_blocky.h"

#include <unordered_map>

namespace zylann::voxel {

VoxelBlockyModel::VoxelBlockyModel() : _color(1.f, 1.f, 1.f) {}

static Cube::Side name_to_side(const String &s) {
	if (s == "left") {
		return Cube::SIDE_LEFT;
	}
	if (s == "right") {
		return Cube::SIDE_RIGHT;
	}
	if (s == "top") {
		return Cube::SIDE_TOP;
	}
	if (s == "bottom") {
		return Cube::SIDE_BOTTOM;
	}
	if (s == "front") {
		return Cube::SIDE_FRONT;
	}
	if (s == "back") {
		return Cube::SIDE_BACK;
	}
	return Cube::SIDE_COUNT; // Invalid
}

bool VoxelBlockyModel::_set(const StringName &p_name, const Variant &p_value) {
	String name = p_name;

	// TODO Eventualy these could be Rect2 for maximum flexibility?
	if (name.begins_with("cube_tiles_")) {
		String s = name.substr(ZN_ARRAY_LENGTH("cube_tiles_") - 1, name.length());
		Cube::Side side = name_to_side(s);
		if (side != Cube::SIDE_COUNT) {
			Vector2 v = p_value;
			set_cube_uv_side(side, Vector2f(v.x, v.y));
			return true;
		}

	} else if (name.begins_with("material_override_")) {
		const int index = name.substr(ZN_ARRAY_LENGTH("material_override_")).to_int();
		set_material_override(index, p_value);
		return true;

	} else if (name.begins_with("collision_enabled_")) {
		const int index = name.substr(ZN_ARRAY_LENGTH("collision_enabled_")).to_int();
		set_mesh_collision_enabled(index, p_value);
		return true;
	}

	return false;
}

bool VoxelBlockyModel::_get(const StringName &p_name, Variant &r_ret) const {
	String name = p_name;

	if (name.begins_with("cube_tiles_")) {
		String s = name.substr(ZN_ARRAY_LENGTH("cube_tiles_") - 1, name.length());
		Cube::Side side = name_to_side(s);
		if (side != Cube::SIDE_COUNT) {
			const Vector2f f = _cube_tiles[side];
			r_ret = Vector2(f.x, f.y);
			return true;
		}

	} else if (name.begins_with("material_override_")) {
		const int index = name.substr(ZN_ARRAY_LENGTH("material_override_")).to_int();
		r_ret = get_material_override(index);
		return true;

	} else if (name.begins_with("collision_enabled_")) {
		const int index = name.substr(ZN_ARRAY_LENGTH("collision_enabled_")).to_int();
		r_ret = is_mesh_collision_enabled(index);
		return true;
	}

	return false;
}

void VoxelBlockyModel::_get_property_list(List<PropertyInfo> *p_list) const {
	if (_geometry_type == GEOMETRY_CUBE) {
		p_list->push_back(PropertyInfo(Variant::NIL, "Cube geometry", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP));

		p_list->push_back(PropertyInfo(Variant::FLOAT, "cube_geometry_padding_y"));

		p_list->push_back(
				PropertyInfo(Variant::NIL, "Cube tiles", PROPERTY_HINT_NONE, "cube_tiles_", PROPERTY_USAGE_GROUP));

		p_list->push_back(PropertyInfo(Variant::VECTOR2, "cube_tiles_left"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "cube_tiles_right"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "cube_tiles_bottom"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "cube_tiles_top"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "cube_tiles_back"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "cube_tiles_front"));
	}

	if (_surface_count > 0) {
		p_list->push_back(PropertyInfo(
				Variant::NIL, "Material overrides", PROPERTY_HINT_NONE, "material_override_", PROPERTY_USAGE_GROUP));

		for (unsigned int i = 0; i < _surface_count; ++i) {
			p_list->push_back(PropertyInfo(Variant::OBJECT, String("material_override_{0}").format(varray(i)),
					PROPERTY_HINT_RESOURCE_TYPE,
					String("{0},{1}").format(
							varray(BaseMaterial3D::get_class_static(), ShaderMaterial::get_class_static()))));
		}

		p_list->push_back(PropertyInfo(
				Variant::NIL, "Mesh collision", PROPERTY_HINT_NONE, "collision_enabled_", PROPERTY_USAGE_GROUP));

		for (unsigned int i = 0; i < _surface_count; ++i) {
			p_list->push_back(PropertyInfo(Variant::BOOL, String("collision_enabled_{0}").format(varray(i))));
		}
	}
}

void VoxelBlockyModel::set_voxel_name(String name) {
	_name = name;
}

void VoxelBlockyModel::set_id(int id) {
	ERR_FAIL_COND(id < 0 || (unsigned int)id >= VoxelBlockyLibrary::MAX_VOXEL_TYPES);
	// Cannot modify ID after creation
	ERR_FAIL_COND_MSG(_id != -1, "ID cannot be modified after being added to a library");
	_id = id;
}

void VoxelBlockyModel::set_color(Color color) {
	_color = color;
}

void VoxelBlockyModel::set_material_override(int index, Ref<Material> material) {
	// TODO Can't check for `_surface_count` instead, because there is no guarantee about the order in which Godot will
	// set properties when loading the resource. The mesh could be set later, so we can't know the number of surfaces.
	ERR_FAIL_INDEX(index, int(_surface_params.size()));
	_surface_params[index].material_override = material;
}

Ref<Material> VoxelBlockyModel::get_material_override(int index) const {
	// TODO Can't check for `_surface_count` instead, because there is no guarantee about the order in which Godot will
	// set properties when loading the resource. The mesh could be set later, so we can't know the number of surfaces.
	ERR_FAIL_INDEX_V(index, int(_surface_params.size()), Ref<Material>());
	return _surface_params[index].material_override;
}

void VoxelBlockyModel::set_mesh_collision_enabled(int surface_index, bool enabled) {
	// TODO Can't check for `_surface_count` instead, because there is no guarantee about the order in which Godot will
	// set properties when loading the resource. The mesh could be set later, so we can't know the number of surfaces.
	ERR_FAIL_INDEX(surface_index, int(_surface_params.size()));
	_surface_params[surface_index].collision_enabled = enabled;
}

bool VoxelBlockyModel::is_mesh_collision_enabled(int surface_index) const {
	// TODO Can't check for `_surface_count` instead, because there is no guarantee about the order in which Godot will
	// set properties when loading the resource. The mesh could be set later, so we can't know the number of surfaces.
	ERR_FAIL_INDEX_V(surface_index, int(_surface_params.size()), false);
	return _surface_params[surface_index].collision_enabled;
}

void VoxelBlockyModel::set_transparent(bool t) {
	if (t) {
		if (_transparency_index == 0) {
			_transparency_index = 1;
		}
	} else {
		_transparency_index = 0;
	}
}

void VoxelBlockyModel::set_transparency_index(int i) {
	_transparency_index = math::clamp(i, 0, 255);
}

void VoxelBlockyModel::set_geometry_type(GeometryType type) {
	if (type == _geometry_type) {
		return;
	}
	_geometry_type = type;

	switch (_geometry_type) {
		case GEOMETRY_NONE:
			_collision_aabbs.clear();
			_surface_count = 0;
			break;

		case GEOMETRY_CUBE:
			_collision_aabbs.clear();
			_collision_aabbs.push_back(AABB(Vector3(0, 0, 0), Vector3(1, 1, 1)));
			_empty = false;
			_surface_count = 1;
			break;

		case GEOMETRY_CUSTOM_MESH:
			// Gotta be user-defined
			if (_custom_mesh.is_valid()) {
				_surface_count = _custom_mesh->get_surface_count();
			} else {
				_surface_count = 0;
			}
			break;

		default:
			ERR_PRINT("Wtf? Unknown geometry type");
			break;
	}
#ifdef TOOLS_ENABLED
	notify_property_list_changed();
#endif
}

VoxelBlockyModel::GeometryType VoxelBlockyModel::get_geometry_type() const {
	return _geometry_type;
}

void VoxelBlockyModel::set_custom_mesh(Ref<Mesh> mesh) {
	_custom_mesh = mesh;
	if (_custom_mesh.is_valid()) {
		set_surface_count(_custom_mesh->get_surface_count());
	} else {
		set_surface_count(0);
	}
}

void VoxelBlockyModel::set_surface_count(unsigned int new_count) {
	if (new_count != _surface_count) {
		_surface_count = new_count;
#ifdef TOOLS_ENABLED
		notify_property_list_changed();
#endif
	}
}

void VoxelBlockyModel::set_random_tickable(bool rt) {
	_random_tickable = rt;
}

void VoxelBlockyModel::set_cube_uv_side(int side, Vector2f tile_pos) {
	_cube_tiles[side] = tile_pos;
}

Ref<Resource> VoxelBlockyModel::duplicate(bool p_subresources) const {
	Ref<VoxelBlockyModel> d_ref;
	d_ref.instantiate();
	VoxelBlockyModel &d = **d_ref;

	d._id = -1;

	d._name = _name;
	d._surface_params = _surface_params;
	d._surface_count = _surface_count;
	d._transparency_index = _transparency_index;
	d._color = _color;
	d._geometry_type = _geometry_type;
	d._cube_tiles = _cube_tiles;
	d._custom_mesh = _custom_mesh;
	d._collision_aabbs = _collision_aabbs;
	d._random_tickable = _random_tickable;
	d._empty = _empty;

	if (p_subresources) {
		if (d._custom_mesh.is_valid()) {
			d._custom_mesh = d._custom_mesh->duplicate(p_subresources);
		}
	}

	return d_ref;
}

void VoxelBlockyModel::set_collision_mask(uint32_t mask) {
	_collision_mask = mask;
}

static void bake_cube_geometry(
		VoxelBlockyModel &config, VoxelBlockyModel::BakedData &baked_data, int p_atlas_size, bool bake_tangents) {
	const float sy = 1.0;

	baked_data.model.surface_count = 1;
	VoxelBlockyModel::BakedData::Surface &surface = baked_data.model.surfaces[0];

	for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {
		std::vector<Vector3f> &positions = surface.side_positions[side];
		positions.resize(4);
		for (unsigned int i = 0; i < 4; ++i) {
			int corner = Cube::g_side_corners[side][i];
			Vector3f p = Cube::g_corner_position[corner];
			if (p.y > 0.9) {
				p.y = sy;
			}
			positions[i] = p;
		}

		std::vector<int> &indices = surface.side_indices[side];
		indices.resize(6);
		for (unsigned int i = 0; i < 6; ++i) {
			indices[i] = Cube::g_side_quad_triangles[side][i];
		}
	}

	const float e = 0.001;
	// Winding is the same as the one chosen in Cube:: vertices
	// I am confused. I read in at least 3 OpenGL tutorials that texture coordinates start at bottom-left (0,0).
	// But even though Godot is said to follow OpenGL's convention, the engine starts at top-left!
	const Vector2f uv[4] = {
		Vector2f(e, 1.f - e),
		Vector2f(1.f - e, 1.f - e),
		Vector2f(1.f - e, e),
		Vector2f(e, e),
	};

	const float atlas_size = (float)p_atlas_size;
	CRASH_COND(atlas_size <= 0);
	const float s = 1.0 / atlas_size;

	for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {
		surface.side_uvs[side].resize(4);
		std::vector<Vector2f> &uvs = surface.side_uvs[side];
		for (unsigned int i = 0; i < 4; ++i) {
			uvs[i] = (config.get_cube_tile(side) + uv[i]) * s;
		}

		if (bake_tangents) {
			std::vector<float> &tangents = surface.side_tangents[side];
			for (unsigned int i = 0; i < 4; ++i) {
				for (unsigned int j = 0; j < 4; ++j) {
					tangents.push_back(Cube::g_side_tangents[side][j]);
				}
			}
		}
	}

	baked_data.empty = false;
}

// Generate tangents based on UVs (won't be as good as properly imported tangents)
static PackedFloat32Array generate_tangents(const PackedVector3Array &positions, const PackedVector3Array &normals,
		const PackedVector2Array &uvs, const PackedInt32Array &indices) {
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

static void bake_mesh_geometry(VoxelBlockyModel &config, VoxelBlockyModel::BakedData &baked_data, bool bake_tangents,
		VoxelBlockyModel::MaterialIndexer &materials) {
	Ref<Mesh> mesh = config.get_custom_mesh();

	if (mesh.is_null()) {
		baked_data.empty = true;
		return;
	}

	VoxelBlockyModel::BakedData::Model &model = baked_data.model;

	if (mesh->get_surface_count() > int(VoxelBlockyModel::BakedData::Model::MAX_SURFACES)) {
		ZN_PRINT_WARNING(format("Mesh has more than {} surfaces, extra surfaces will not be baked.",
				VoxelBlockyModel::BakedData::Model::MAX_SURFACES));
	}

	const unsigned int surface_count =
			math::min(uint32_t(mesh->get_surface_count()), VoxelBlockyModel::BakedData::Model::MAX_SURFACES);

	baked_data.model.surface_count = surface_count;

	for (unsigned int surface_index = 0; surface_index < surface_count; ++surface_index) {
		Array arrays = mesh->surface_get_arrays(surface_index);

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

		const bool tangents_empty = (tangents.size() == 0);

#ifdef TOOLS_ENABLED
		if (tangents_empty && bake_tangents) {
			if (uvs.size() == 0) {
				ZN_PRINT_ERROR(format("Voxel model {} with ID {} is missing tangents and UVs. The model won't be "
									  "baked. You should consider providing a mesh with tangents, or at least UVs and "
									  "normals, or turn off tangents baking in {}.",
						String(config.get_voxel_name()), config.get_id(),
						String(VoxelBlockyLibrary::get_class_static())));
				continue;
			}
			WARN_PRINT(String("Voxel model '{0}' with ID {1} does not have tangents. They will be generated."
							  "You should consider providing a mesh with tangents, or at least UVs and normals, "
							  "or turn off tangents baking in {2}.")
							   .format(varray(config.get_voxel_name(), config.get_id(),
									   VoxelBlockyLibrary::get_class_static())));

			tangents = generate_tangents(positions, normals, uvs, indices);
		}
#endif

		if (uvs.size() == 0) {
			// TODO Properly generate UVs if there arent any
			uvs = PackedVector2Array();
			uvs.resize(positions.size());
		}

		// Separate triangles belonging to faces of the cube

		VoxelBlockyModel::BakedData::Surface &surface = model.surfaces[surface_index];
		Ref<Material> material = mesh->surface_get_material(surface_index);
		// Note, an empty material counts as "The default material".
		surface.material_id = materials.get_or_create_index(material);

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

void VoxelBlockyModel::bake(BakedData &baked_data, int p_atlas_size, bool bake_tangents, MaterialIndexer &materials) {
	baked_data.clear();

	// baked_data.contributes_to_ao is set by the side culling phase
	baked_data.transparency_index = _transparency_index;
	baked_data.color = _color;

	switch (_geometry_type) {
		case GEOMETRY_NONE:
			baked_data.empty = true;
			break;

		case GEOMETRY_CUBE:
			bake_cube_geometry(*this, baked_data, p_atlas_size, bake_tangents);
			break;

		case GEOMETRY_CUSTOM_MESH:
			bake_mesh_geometry(*this, baked_data, bake_tangents, materials);
			break;

		default:
			ERR_PRINT("Wtf? Unknown geometry type");
			break;
	}

	BakedData::Model &model = baked_data.model;

	// Set empty sides mask
	model.empty_sides_mask = 0;
	for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {
		bool empty = true;
		for (unsigned int surface_index = 0; surface_index < model.surface_count; ++surface_index) {
			const BakedData::Surface &surface = model.surfaces[surface_index];
			if (surface.side_indices[side].size() > 0) {
				empty = false;
				break;
			}
		}
		if (empty) {
			model.empty_sides_mask |= (1 << side);
		}
	}

	// Assign material overrides if any
	for (unsigned int surface_index = 0; surface_index < model.surface_count; ++surface_index) {
		if (surface_index < _surface_count) {
			const SurfaceParams &surface_params = _surface_params[surface_index];
			const Ref<Material> material = surface_params.material_override;

			BakedData::Surface &surface = model.surfaces[surface_index];

			const unsigned int material_index = materials.get_or_create_index(material);
			surface.material_id = material_index;

			surface.collision_enabled = surface_params.collision_enabled;
		}
	}

	_empty = baked_data.empty;
}

TypedArray<AABB> VoxelBlockyModel::_b_get_collision_aabbs() const {
	TypedArray<AABB> array;
	array.resize(_collision_aabbs.size());
	for (size_t i = 0; i < _collision_aabbs.size(); ++i) {
		array[i] = _collision_aabbs[i];
	}
	return array;
}

void VoxelBlockyModel::_b_set_collision_aabbs(TypedArray<AABB> array) {
	for (int i = 0; i < array.size(); ++i) {
		const Variant v = array[i];
		// ERR_FAIL_COND(v.get_type() != Variant::AABB);
		// TODO "Add Element" in the Godot Array inspector always adds a null element even if the array is typed!
		if (v.get_type() != Variant::AABB) {
			ZN_PRINT_WARNING(format("Item {} of the array is not an AABB (found {}). It will be replaced.", i,
					Variant::get_type_name(v.get_type())));
			array[i] = AABB(Vector3(), Vector3(1, 1, 1));
		}
	}
	_collision_aabbs.resize(array.size());
	for (int i = 0; i < array.size(); ++i) {
		const AABB aabb = array[i];
		_collision_aabbs[i] = aabb;
	}
}

// void ortho_simplify(Span<const Vector3f> vertices, Span<const int> indices, std::vector<int> &output) {
// TODO Optimization: implement mesh simplification based on axis-aligned triangles.
// It could be very effective on mesh collisions with the blocky mesher.
// }

void VoxelBlockyModel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_voxel_name", "name"), &VoxelBlockyModel::set_voxel_name);
	ClassDB::bind_method(D_METHOD("get_voxel_name"), &VoxelBlockyModel::get_voxel_name);

	ClassDB::bind_method(D_METHOD("set_id", "id"), &VoxelBlockyModel::set_id);
	ClassDB::bind_method(D_METHOD("get_id"), &VoxelBlockyModel::get_id);

	ClassDB::bind_method(D_METHOD("set_color", "color"), &VoxelBlockyModel::set_color);
	ClassDB::bind_method(D_METHOD("get_color"), &VoxelBlockyModel::get_color);

	ClassDB::bind_method(
			D_METHOD("set_material_override", "index", "material"), &VoxelBlockyModel::set_material_override);
	ClassDB::bind_method(D_METHOD("get_material_override", "index"), &VoxelBlockyModel::get_material_override);

	ClassDB::bind_method(D_METHOD("set_transparent", "transparent"), &VoxelBlockyModel::set_transparent);
	ClassDB::bind_method(D_METHOD("is_transparent"), &VoxelBlockyModel::is_transparent);

	ClassDB::bind_method(
			D_METHOD("set_transparency_index", "transparency_index"), &VoxelBlockyModel::set_transparency_index);
	ClassDB::bind_method(D_METHOD("get_transparency_index"), &VoxelBlockyModel::get_transparency_index);

	ClassDB::bind_method(D_METHOD("set_random_tickable", "rt"), &VoxelBlockyModel::set_random_tickable);
	ClassDB::bind_method(D_METHOD("is_random_tickable"), &VoxelBlockyModel::is_random_tickable);

	ClassDB::bind_method(D_METHOD("set_geometry_type", "type"), &VoxelBlockyModel::set_geometry_type);
	ClassDB::bind_method(D_METHOD("get_geometry_type"), &VoxelBlockyModel::get_geometry_type);

	ClassDB::bind_method(D_METHOD("set_custom_mesh", "type"), &VoxelBlockyModel::set_custom_mesh);
	ClassDB::bind_method(D_METHOD("get_custom_mesh"), &VoxelBlockyModel::get_custom_mesh);

	ClassDB::bind_method(D_METHOD("set_mesh_collision_enabled", "surface_index", "enabled"),
			&VoxelBlockyModel::set_mesh_collision_enabled);
	ClassDB::bind_method(
			D_METHOD("is_mesh_collision_enabled", "surface_index"), &VoxelBlockyModel::is_mesh_collision_enabled);

	ClassDB::bind_method(D_METHOD("set_collision_aabbs", "aabbs"), &VoxelBlockyModel::_b_set_collision_aabbs);
	ClassDB::bind_method(D_METHOD("get_collision_aabbs"), &VoxelBlockyModel::_b_get_collision_aabbs);

	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &VoxelBlockyModel::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &VoxelBlockyModel::get_collision_mask);

	ClassDB::bind_method(D_METHOD("is_empty"), &VoxelBlockyModel::is_empty);

	// TODO Update to StringName in Godot 4
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "voxel_name"), "set_voxel_name", "get_voxel_name");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
	// TODO Might become obsolete
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "transparent", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE),
			"set_transparent", "is_transparent");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "transparency_index"), "set_transparency_index", "get_transparency_index");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "random_tickable"), "set_random_tickable", "is_random_tickable");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "geometry_type", PROPERTY_HINT_ENUM, "None,Cube,CustomMesh"),
			"set_geometry_type", "get_geometry_type");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "custom_mesh", PROPERTY_HINT_RESOURCE_TYPE, Mesh::get_class_static()),
			"set_custom_mesh", "get_custom_mesh");

	ADD_GROUP("Box collision", "");

	// TODO What is the syntax `number:` in `hint_string` with `ARRAY`? It's old, hard to search usages in Godot's
	// codebase, and I can't find it anywhere in the documentation
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "collision_aabbs", PROPERTY_HINT_TYPE_STRING,
						 String::num_int64(Variant::AABB) + ":"),
			"set_collision_aabbs", "get_collision_aabbs");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask",
			"get_collision_mask");

	BIND_ENUM_CONSTANT(GEOMETRY_NONE);
	BIND_ENUM_CONSTANT(GEOMETRY_CUBE);
	BIND_ENUM_CONSTANT(GEOMETRY_CUSTOM_MESH);
	BIND_ENUM_CONSTANT(GEOMETRY_MAX);

	BIND_ENUM_CONSTANT(SIDE_NEGATIVE_X);
	BIND_ENUM_CONSTANT(SIDE_POSITIVE_X);
	BIND_ENUM_CONSTANT(SIDE_NEGATIVE_Y);
	BIND_ENUM_CONSTANT(SIDE_POSITIVE_Y);
	BIND_ENUM_CONSTANT(SIDE_NEGATIVE_Z);
	BIND_ENUM_CONSTANT(SIDE_POSITIVE_Z);
	BIND_ENUM_CONSTANT(SIDE_COUNT);
}

} // namespace zylann::voxel
