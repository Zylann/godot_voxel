#include "voxel_blocky_model.h"
#include "../../util/containers/container_funcs.h"
#include "../../util/godot/classes/array_mesh.h"
#include "../../util/godot/classes/base_material_3d.h"
#include "../../util/godot/classes/shader_material.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/string.h"
#include "../../util/math/conv.h"
#include "../../util/string/format.h"
#include "voxel_blocky_library.h"

// TODO Only required because of MAX_MATERIALS... could be enough inverting that dependency
#include "voxel_mesher_blocky.h"

#include "voxel_blocky_model_cube.h"

namespace zylann::voxel {

unsigned int VoxelBlockyModel::MaterialIndexer::get_or_create_index(const Ref<Material> &p_material) {
	for (size_t i = 0; i < materials.size(); ++i) {
		const Ref<Material> &material = materials[i];
		if (material == p_material) {
			return i;
		}
	}
#ifdef TOOLS_ENABLED
	if (materials.size() == VoxelBlockyLibraryBase::MAX_MATERIALS) {
		ZN_PRINT_ERROR(
				format("Maximum material count reached ({}), try reduce your number of materials by re-using "
					   "them or using atlases.",
					   VoxelBlockyLibraryBase::MAX_MATERIALS)
		);
	}
#endif
	const unsigned int ret = materials.size();
	materials.push_back(p_material);
	return ret;
}

VoxelBlockyModel::VoxelBlockyModel() : _color(1.f, 1.f, 1.f) {}

bool VoxelBlockyModel::_set(const StringName &p_name, const Variant &p_value) {
	String property_name = p_name;

	if (property_name.begins_with("material_override_")) {
		const int index = property_name.substr(string_literal_length("material_override_")).to_int();
		set_material_override(index, p_value);
		return true;

	} else if (property_name.begins_with("collision_enabled_")) {
		const int index = property_name.substr(string_literal_length("collision_enabled_")).to_int();
		set_mesh_collision_enabled(index, p_value);
		return true;
	}

	// LEGACY

	if (property_name.begins_with("cube_tiles_")) {
		String s = property_name.substr(string_literal_length("cube_tiles_"), property_name.length());
		Cube::Side side = VoxelBlockyModelCube::name_to_side(s);
		if (side != Cube::SIDE_COUNT) {
			Vector2 v = p_value;
			_legacy_properties.cube_tiles[side] = Vector2f(v.x, v.y);
			return true;
		}
		_legacy_properties.found = true;
		return true;

	} else if (property_name == "geometry_type") {
		_legacy_properties.geometry_type = LegacyProperties::GeometryType(int(p_value));
		_legacy_properties.found = true;
		return true;

	} else if (property_name == "voxel_name") {
		_legacy_properties.name = p_value;
		_legacy_properties.found = true;
		return true;

	} else if (property_name == "custom_mesh") {
		_legacy_properties.custom_mesh = p_value;
		_legacy_properties.found = true;
		return true;
	}

	return false;
}

bool VoxelBlockyModel::_get(const StringName &p_name, Variant &r_ret) const {
	String property_name = p_name;

	if (property_name.begins_with("material_override_")) {
		const int index = property_name.substr(string_literal_length("material_override_")).to_int();
		r_ret = get_material_override(index);
		return true;

	} else if (property_name.begins_with("collision_enabled_")) {
		const int index = property_name.substr(string_literal_length("collision_enabled_")).to_int();
		r_ret = is_mesh_collision_enabled(index);
		return true;
	}

	// LEGACY

	if (property_name.begins_with("cube_tiles_")) {
		String s = property_name.substr(string_literal_length("cube_tiles_"), property_name.length());
		Cube::Side side = VoxelBlockyModelCube::name_to_side(s);
		if (side != Cube::SIDE_COUNT) {
			const Vector2f f = _legacy_properties.cube_tiles[side];
			r_ret = Vector2(f.x, f.y);
			return true;
		}

	} else if (property_name == "geometry_type") {
		r_ret = int(_legacy_properties.geometry_type);
		return true;

	} else if (property_name == "voxel_name") {
		r_ret = _legacy_properties.name;
		return true;

	} else if (property_name == "custom_mesh") {
		r_ret = _legacy_properties.custom_mesh;
		return true;
	}

	return false;
}

void VoxelBlockyModel::_get_property_list(List<PropertyInfo> *p_list) const {
	if (_surface_count > 0) {
		p_list->push_back(PropertyInfo(
				Variant::NIL, "Material overrides", PROPERTY_HINT_NONE, "material_override_", PROPERTY_USAGE_GROUP
		));

		for (unsigned int i = 0; i < _surface_count; ++i) {
			p_list->push_back(PropertyInfo(
					Variant::OBJECT,
					String("material_override_{0}").format(varray(i)),
					PROPERTY_HINT_RESOURCE_TYPE,
					String("{0},{1}").format(
							varray(BaseMaterial3D::get_class_static(), ShaderMaterial::get_class_static())
					)
			));
		}

		p_list->push_back(PropertyInfo(
				Variant::NIL, "Mesh collision", PROPERTY_HINT_NONE, "collision_enabled_", PROPERTY_USAGE_GROUP
		));

		for (unsigned int i = 0; i < _surface_count; ++i) {
			p_list->push_back(PropertyInfo(Variant::BOOL, String("collision_enabled_{0}").format(varray(i))));
		}
	}
}

// void VoxelBlockyModel::set_voxel_name(String name) {
// 	_name = name;
// }

// void VoxelBlockyModel::set_id(int id) {
// 	ERR_FAIL_COND(id < 0 || (unsigned int)id >= VoxelBlockyLibrary::MAX_VOXEL_TYPES);
// 	// Cannot modify ID after creation
// 	ERR_FAIL_COND_MSG(_id != -1, "ID cannot be modified after being added to a library");
// 	_id = id;
// }

void VoxelBlockyModel::set_color(Color color) {
	if (color != _color) {
		_color = color;
		emit_changed();
	}
}

void VoxelBlockyModel::set_material_override(int index, Ref<Material> material) {
	// TODO Can't check for `_surface_count` instead, because there is no guarantee about the order in which Godot will
	// set properties when loading the resource. The mesh could be set later, so we can't know the number of surfaces.
	ERR_FAIL_INDEX(index, int(_surface_params.size()));
	_surface_params[index].material_override = material;
	emit_changed();
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

void VoxelBlockyModel::set_culls_neighbors(bool cn) {
	_culls_neighbors = cn;
}

void VoxelBlockyModel::set_surface_count(unsigned int new_count) {
	if (new_count != _surface_count) {
		_surface_count = new_count;
#ifdef TOOLS_ENABLED
		notify_property_list_changed();
#endif
	}
}

void VoxelBlockyModel::set_collision_mask(uint32_t mask) {
	_collision_mask = mask;
}

void VoxelBlockyModel::bake(BakedData &baked_data, bool bake_tangents, MaterialIndexer &materials) const {
	// TODO That's a bit iffy, design something better?
	// The following logic must run after derived classes, should not be called directly

	// baked_data.contributes_to_ao is set by the side culling phase
	baked_data.transparency_index = _transparency_index;
	baked_data.culls_neighbors = _culls_neighbors;
	baked_data.color = _color;
	baked_data.is_random_tickable = _random_tickable;
	baked_data.box_collision_mask = _collision_mask;
	baked_data.box_collision_aabbs = _collision_aabbs;

	BakedData::Model &model = baked_data.model;

	// Set empty sides mask
	model.empty_sides_mask = 0;
	for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {
		bool empty = true;
		for (unsigned int surface_index = 0; surface_index < model.surface_count; ++surface_index) {
			const BakedData::Surface &surface = model.surfaces[surface_index];
			if (surface.sides[side].indices.size() > 0) {
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
			ZN_PRINT_WARNING(
					format("Item {} of the array is not an AABB (found {}). It will be replaced.",
						   i,
						   Variant::get_type_name(v.get_type()))
			);
			array[i] = AABB(Vector3(), Vector3(1, 1, 1));
		}
	}
	_collision_aabbs.resize(array.size());
	for (int i = 0; i < array.size(); ++i) {
		const AABB aabb = array[i];
		_collision_aabbs[i] = aabb;
	}
	emit_changed();
}

unsigned int VoxelBlockyModel::get_collision_aabb_count() const {
	return _collision_aabbs.size();
}

void VoxelBlockyModel::set_collision_aabb(unsigned int i, AABB aabb) {
	ZN_ASSERT_RETURN(i < _collision_aabbs.size());
	_collision_aabbs[i] = aabb;
}

void VoxelBlockyModel::set_collision_aabbs(Span<const AABB> aabbs) {
	_collision_aabbs.resize(aabbs.size());
	for (unsigned int i = 0; i < aabbs.size(); ++i) {
		_collision_aabbs[i] = aabbs[i];
	}
}

void VoxelBlockyModel::set_random_tickable(bool rt) {
	_random_tickable = rt;
}

bool VoxelBlockyModel::is_random_tickable() const {
	return _random_tickable;
}

bool VoxelBlockyModel::is_empty() const {
	ZN_PRINT_ERROR("Not implemented");
	// Implemented in child classes
	return true;
}

void VoxelBlockyModel::copy_base_properties_from(const VoxelBlockyModel &src) {
	_surface_params = src._surface_params;
	// _surface_count = src._surface_count;
	_transparency_index = src._transparency_index;
	_culls_neighbors = src._culls_neighbors;
	_random_tickable = src._random_tickable;
	_color = src._color;
	_collision_aabbs = src._collision_aabbs;
	_collision_mask = src._collision_mask;
}

Ref<Mesh> VoxelBlockyModel::get_preview_mesh() const {
	ZN_PRINT_ERROR("Not implemented");
	// Implemented in child classes
	return Ref<Mesh>();
}

Ref<Mesh> VoxelBlockyModel::make_mesh_from_baked_data(const BakedData &baked_data, bool tangents_enabled) {
	const VoxelBlockyModel::BakedData::Model &model = baked_data.model;

	Ref<ArrayMesh> mesh;
	mesh.instantiate();

	for (unsigned int surface_index = 0; surface_index < model.surface_count; ++surface_index) {
		const BakedData::Surface &surface = model.surfaces[surface_index];

		// Get vertex and index count in the surface
		unsigned int vertex_count = surface.positions.size();
		unsigned int index_count = surface.indices.size();
		for (const BakedData::SideSurface &side_surface : surface.sides) {
			vertex_count += side_surface.positions.size();
			index_count += side_surface.indices.size();
		}

		// Allocate surface arrays

		PackedVector3Array vertices;
		PackedVector3Array normals;
		PackedFloat32Array tangents;
		PackedColorArray colors;
		PackedVector2Array uvs;

		vertices.resize(vertex_count);
		normals.resize(vertex_count);
		if (tangents_enabled) {
			tangents.resize(vertex_count * 4);
		}
		colors.resize(vertex_count);
		uvs.resize(vertex_count);

		PackedInt32Array indices;
		indices.resize(index_count);

		Span<Vector3> vertices_w(vertices.ptrw(), vertices.size());
		Span<Vector3> normals_w(normals.ptrw(), normals.size());
		Span<float> tangents_w;
		if (tangents_enabled) {
			tangents_w = Span<float>(tangents.ptrw(), tangents.size());
		}
		Span<Color> colors_w(colors.ptrw(), colors.size());
		Span<Vector2> uvs_w(uvs.ptrw(), uvs.size());
		Span<int> indices_w(indices.ptrw(), indices.size());

		// Populate arrays

		unsigned int vi = 0;
		unsigned int ti = 0;
		unsigned int ii = 0;

		for (unsigned int i = 0; i < surface.positions.size(); ++i) {
			vertices_w[vi] = to_vec3(surface.positions[i]);
			normals_w[vi] = to_vec3(surface.normals[i]);
			colors_w[vi] = baked_data.color;
			uvs_w[vi] = to_vec2(surface.uvs[i]);
			++vi;
		}
		if (tangents_enabled) {
			for (unsigned int i = 0; i < surface.tangents.size(); ++i) {
				tangents_w[ti] = surface.tangents[i];
				++ti;
			}
		}
		for (unsigned int i = 0; i < surface.indices.size(); ++i) {
			indices_w[ii] = surface.indices[i];
			++ii;
		}

		for (unsigned int side = 0; side < surface.sides.size(); ++side) {
			const BakedData::SideSurface &side_surface = surface.sides[side];
			Span<const Vector3f> side_positions = to_span(side_surface.positions);
			Span<const Vector2f> side_uvs = to_span(side_surface.uvs);
			Span<const int> side_indices = to_span(side_surface.indices);
			Span<const float> side_tangents = to_span(side_surface.tangents);
			const Vector3 side_normal = to_vec3(Cube::g_side_normals[side]);

			const unsigned int vi0 = vi;

			for (unsigned int i = 0; i < side_positions.size(); ++i) {
				vertices_w[vi] = to_vec3(side_positions[i]);
				normals_w[vi] = side_normal;
				colors_w[vi] = baked_data.color;
				uvs_w[vi] = to_vec2(side_uvs[i]);
				++vi;
			}
			if (tangents_enabled) {
				for (unsigned int i = 0; i < side_tangents.size(); ++i) {
					tangents_w[ti] = side_tangents[i];
					++ti;
				}
			}
			for (unsigned int i = 0; i < side_indices.size(); ++i) {
				indices_w[ii] = vi0 + side_indices[i];
				++ii;
			}
		}

		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = vertices;
		arrays[Mesh::ARRAY_NORMAL] = normals;
		arrays[Mesh::ARRAY_TEX_UV] = uvs;
		arrays[Mesh::ARRAY_COLOR] = colors;
		if (tangents_enabled) {
			arrays[Mesh::ARRAY_TANGENT] = tangents;
		}
		arrays[Mesh::ARRAY_INDEX] = indices;

		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	}

	return mesh;
}

void VoxelBlockyModel::rotate_collision_boxes_90(math::Axis axis, bool clockwise) {
	for (AABB &aabb : _collision_aabbs) {
		// Make it centered, rotation axis is the center of the voxel
		aabb.position -= Vector3(0.5, 0.5, 0.5);

		FixedArray<Vector3f, 2> points;
		points[0] = to_vec3f(aabb.position);
		points[1] = to_vec3f(aabb.position + aabb.size);
		// TODO Move Axis enum outside of vectors?
		math::rotate_90(to_span(points), math::Axis(axis), clockwise);
		const Vector3f min_pos = math::min(points[0], points[1]);
		const Vector3f max_pos = math::max(points[0], points[1]);
		aabb = AABB(to_vec3(min_pos), to_vec3(max_pos - min_pos));

		aabb.position += Vector3(0.5, 0.5, 0.5);
	}
}

void VoxelBlockyModel::rotate_collision_boxes_ortho(math::OrthoBasis ortho_basis) {
	Basis basis(to_vec3(ortho_basis.x), to_vec3(ortho_basis.y), to_vec3(ortho_basis.z));

	for (AABB &aabb : _collision_aabbs) {
		// Make it centered, rotation axis is the center of the voxel
		aabb.position -= Vector3(0.5, 0.5, 0.5);

		const Vector3 p0 = basis.xform(aabb.position);
		const Vector3 p1 = basis.xform(aabb.position + aabb.size);

		const Vector3 min_pos = math::min(p0, p1);
		const Vector3 max_pos = math::max(p0, p1);
		aabb = AABB(min_pos, max_pos - min_pos);

		aabb.position += Vector3(0.5, 0.5, 0.5);
	}
}

void VoxelBlockyModel::rotate_90(math::Axis axis, bool clockwise) {
	ZN_PRINT_ERROR("Not implemented");
	// Implemented in child classes
}

void VoxelBlockyModel::rotate_ortho(math::OrthoBasis ortho_basis) {
	ZN_PRINT_ERROR("Not implemented");
	// Implemented in child classes
}

void VoxelBlockyModel::_b_rotate_90(Vector3i::Axis axis, bool clockwise) {
	ERR_FAIL_INDEX(axis, Vector3i::AXIS_COUNT);
	rotate_90(math::Axis(axis), clockwise);
}

// void ortho_simplify(Span<const Vector3f> vertices, Span<const int> indices, StdVector<int> &output) {
// TODO Optimization: implement mesh simplification based on axis-aligned triangles.
// It could be very effective on mesh collisions with the blocky mesher.
// }

void VoxelBlockyModel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_color", "color"), &VoxelBlockyModel::set_color);
	ClassDB::bind_method(D_METHOD("get_color"), &VoxelBlockyModel::get_color);

	ClassDB::bind_method(
			D_METHOD("set_material_override", "index", "material"), &VoxelBlockyModel::set_material_override
	);
	ClassDB::bind_method(D_METHOD("get_material_override", "index"), &VoxelBlockyModel::get_material_override);

	ClassDB::bind_method(D_METHOD("set_transparent", "transparent"), &VoxelBlockyModel::set_transparent);
	ClassDB::bind_method(D_METHOD("is_transparent"), &VoxelBlockyModel::is_transparent);

	ClassDB::bind_method(
			D_METHOD("set_transparency_index", "transparency_index"), &VoxelBlockyModel::set_transparency_index
	);
	ClassDB::bind_method(D_METHOD("get_transparency_index"), &VoxelBlockyModel::get_transparency_index);

	ClassDB::bind_method(D_METHOD("set_culls_neighbors", "culls_neighbors"), &VoxelBlockyModel::set_culls_neighbors);
	ClassDB::bind_method(D_METHOD("get_culls_neighbors"), &VoxelBlockyModel::get_culls_neighbors);

	ClassDB::bind_method(D_METHOD("is_random_tickable"), &VoxelBlockyModel::is_random_tickable);
	ClassDB::bind_method(D_METHOD("set_random_tickable"), &VoxelBlockyModel::set_random_tickable);

	ClassDB::bind_method(
			D_METHOD("set_mesh_collision_enabled", "surface_index", "enabled"),
			&VoxelBlockyModel::set_mesh_collision_enabled
	);
	ClassDB::bind_method(
			D_METHOD("is_mesh_collision_enabled", "surface_index"), &VoxelBlockyModel::is_mesh_collision_enabled
	);

	ClassDB::bind_method(D_METHOD("set_collision_aabbs", "aabbs"), &VoxelBlockyModel::_b_set_collision_aabbs);
	ClassDB::bind_method(D_METHOD("get_collision_aabbs"), &VoxelBlockyModel::_b_get_collision_aabbs);

	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &VoxelBlockyModel::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &VoxelBlockyModel::get_collision_mask);

	// Bound for editor purposes
	ClassDB::bind_method(D_METHOD("rotate_90", "axis", "clockwise"), &VoxelBlockyModel::_b_rotate_90);

	// TODO Update to StringName in Godot 4
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
	// TODO Might become obsolete
	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "transparent", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE),
			"set_transparent",
			"is_transparent"
	);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "transparency_index"), "set_transparency_index", "get_transparency_index");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "culls_neighbors"), "set_culls_neighbors", "get_culls_neighbors");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "random_tickable"), "set_random_tickable", "is_random_tickable");

	ADD_GROUP("Box collision", "");

	// TODO What is the syntax `number:` in `hint_string` with `ARRAY`? It's old, hard to search usages in Godot's
	// codebase, and I can't find it anywhere in the documentation
	ADD_PROPERTY(
			PropertyInfo(
					Variant::ARRAY, "collision_aabbs", PROPERTY_HINT_TYPE_STRING, String::num_int64(Variant::AABB) + ":"
			),
			"set_collision_aabbs",
			"get_collision_aabbs"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS),
			"set_collision_mask",
			"get_collision_mask"
	);

	BIND_ENUM_CONSTANT(SIDE_NEGATIVE_X);
	BIND_ENUM_CONSTANT(SIDE_POSITIVE_X);
	BIND_ENUM_CONSTANT(SIDE_NEGATIVE_Y);
	BIND_ENUM_CONSTANT(SIDE_POSITIVE_Y);
	BIND_ENUM_CONSTANT(SIDE_NEGATIVE_Z);
	BIND_ENUM_CONSTANT(SIDE_POSITIVE_Z);
	BIND_ENUM_CONSTANT(SIDE_COUNT);
}

} // namespace zylann::voxel
