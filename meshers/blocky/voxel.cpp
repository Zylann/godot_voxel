#include "voxel.h"
#include "../../util/macros.h"
#include "voxel_library.h"
#include "voxel_mesher_blocky.h" // TODO Only required because of MAX_MATERIALS... could be enough inverting that dependency

#define STRLEN(x) (sizeof(x) / sizeof(x[0]))

Voxel::Voxel() :
		_id(-1),
		_material_id(0),
		_is_transparent(false),
		_color(1.f, 1.f, 1.f),
		_geometry_type(GEOMETRY_NONE) {
}

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

bool Voxel::_set(const StringName &p_name, const Variant &p_value) {
	String name = p_name;

	// TODO Eventualy these could be Rect2 for maximum flexibility?
	if (name.begins_with("cube_tiles/")) {
		String s = name.substr(STRLEN("cube_tiles/") - 1, name.length());
		Cube::Side side = name_to_side(s);
		if (side != Cube::SIDE_COUNT) {
			Vector2 v = p_value;
			set_cube_uv_side(side, v);
			return true;
		}
	}

	return false;
}

bool Voxel::_get(const StringName &p_name, Variant &r_ret) const {
	String name = p_name;

	if (name.begins_with("cube_tiles/")) {
		String s = name.substr(STRLEN("cube_tiles/") - 1, name.length());
		Cube::Side side = name_to_side(s);
		if (side != Cube::SIDE_COUNT) {
			r_ret = _cube_tiles[side];
			return true;
		}
	}

	return false;
}

void Voxel::_get_property_list(List<PropertyInfo> *p_list) const {
	if (_geometry_type == GEOMETRY_CUBE) {
		p_list->push_back(PropertyInfo(Variant::REAL, "cube_geometry/padding_y"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "cube_tiles/left"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "cube_tiles/right"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "cube_tiles/bottom"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "cube_tiles/top"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "cube_tiles/back"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, "cube_tiles/front"));
	}
}

void Voxel::set_voxel_name(String name) {
	_name = name;
}

void Voxel::set_id(int id) {
	ERR_FAIL_COND(id < 0 || (unsigned int)id >= VoxelLibrary::MAX_VOXEL_TYPES);
	// Cannot modify ID after creation
	ERR_FAIL_COND_MSG(_id != -1, "ID cannot be modified after being added to a library");
	_id = id;
}

void Voxel::set_color(Color color) {
	_color = color;
}

void Voxel::set_material_id(unsigned int id) {
	ERR_FAIL_COND(id >= VoxelMesherBlocky::MAX_MATERIALS);
	_material_id = id;
}

void Voxel::set_transparent(bool t) {
	_is_transparent = t;
}

void Voxel::set_geometry_type(GeometryType type) {
	_geometry_type = type;

	switch (_geometry_type) {
		case GEOMETRY_NONE:
			_collision_aabbs.clear();
			break;

		case GEOMETRY_CUBE:
			_collision_aabbs.clear();
			_collision_aabbs.push_back(AABB(Vector3(0, 0, 0), Vector3(1, 1, 1)));
			_empty = false;
			break;

		case GEOMETRY_CUSTOM_MESH:
			// Gotta be user-defined
			break;

		default:
			ERR_PRINT("Wtf? Unknown geometry type");
			break;
	}
}

Voxel::GeometryType Voxel::get_geometry_type() const {
	return _geometry_type;
}

void Voxel::set_custom_mesh(Ref<Mesh> mesh) {
	_custom_mesh = mesh;
}

void Voxel::set_cube_geometry() {
}

void Voxel::set_random_tickable(bool rt) {
	_random_tickable = rt;
}

void Voxel::set_cube_uv_side(int side, Vector2 tile_pos) {
	_cube_tiles[side] = tile_pos;
}

Ref<Resource> Voxel::duplicate(bool p_subresources) const {
	Ref<Voxel> d_ref;
	d_ref.instance();
	Voxel &d = **d_ref;

	d._id = -1;

	d._name = _name;
	d._material_id = _material_id;
	d._is_transparent = _is_transparent;
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

void Voxel::set_collision_mask(uint32_t mask) {
	_collision_mask = mask;
}

static void bake_cube_geometry(Voxel &config, Voxel::BakedData &baked_data, int p_atlas_size) {
	const float sy = 1.0;

	for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {
		std::vector<Vector3> &positions = baked_data.model.side_positions[side];
		positions.resize(4);
		for (unsigned int i = 0; i < 4; ++i) {
			int corner = Cube::g_side_corners[side][i];
			Vector3 p = Cube::g_corner_position[corner];
			if (p.y > 0.9) {
				p.y = sy;
			}
			positions[i] = p;
		}

		std::vector<int> &indices = baked_data.model.side_indices[side];
		indices.resize(6);
		for (unsigned int i = 0; i < 6; ++i) {
			indices[i] = Cube::g_side_quad_triangles[side][i];
		}
	}

	const float e = 0.001;
	// Winding is the same as the one chosen in Cube:: vertices
	// I am confused. I read in at least 3 OpenGL tutorials that texture coordinates start at bottom-left (0,0).
	// But even though Godot is said to follow OpenGL's convention, the engine starts at top-left!
	const Vector2 uv[4] = {
		Vector2(e, 1.f - e),
		Vector2(1.f - e, 1.f - e),
		Vector2(1.f - e, e),
		Vector2(e, e),
	};

	const float atlas_size = (float)p_atlas_size;
	CRASH_COND(atlas_size <= 0);
	const float s = 1.0 / atlas_size;

	for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {
		baked_data.model.side_uvs[side].resize(4);
		std::vector<Vector2> &uvs = baked_data.model.side_uvs[side];
		for (unsigned int i = 0; i < 4; ++i) {
			uvs[i] = (config.get_cube_tile(side) + uv[i]) * s;
		}
	}

	baked_data.empty = false;
}

static void bake_mesh_geometry(Voxel &config, Voxel::BakedData &baked_data) {
	Ref<Mesh> mesh = config.get_custom_mesh();

	if (mesh.is_null()) {
		baked_data.empty = true;
		return;
	}

	Array arrays = mesh->surface_get_arrays(0);

	ERR_FAIL_COND(arrays.size() == 0);

	PoolIntArray indices = arrays[Mesh::ARRAY_INDEX];
	ERR_FAIL_COND_MSG(indices.size() % 3 != 0, "Mesh is empty or does not contain triangles");

	PoolVector3Array positions = arrays[Mesh::ARRAY_VERTEX];
	PoolVector3Array normals = arrays[Mesh::ARRAY_NORMAL];
	PoolVector2Array uvs = arrays[Mesh::ARRAY_TEX_UV];

	baked_data.empty = positions.size() == 0;

	ERR_FAIL_COND(normals.size() == 0);

	struct L {
		static uint8_t get_sides(Vector3 pos) {
			uint8_t mask = 0;
			const real_t tolerance = 0.001;
			mask |= Math::is_equal_approx(pos.x, 0.0, tolerance) << Cube::SIDE_NEGATIVE_X;
			mask |= Math::is_equal_approx(pos.x, 1.0, tolerance) << Cube::SIDE_POSITIVE_X;
			mask |= Math::is_equal_approx(pos.y, 0.0, tolerance) << Cube::SIDE_NEGATIVE_Y;
			mask |= Math::is_equal_approx(pos.y, 1.0, tolerance) << Cube::SIDE_POSITIVE_Y;
			mask |= Math::is_equal_approx(pos.z, 0.0, tolerance) << Cube::SIDE_NEGATIVE_Z;
			mask |= Math::is_equal_approx(pos.z, 1.0, tolerance) << Cube::SIDE_POSITIVE_Z;
			return mask;
		}

		static bool get_triangle_side(const Vector3 &a, const Vector3 &b, const Vector3 &c, Cube::SideAxis &out_side) {
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

	if (uvs.size() == 0) {
		// TODO Properly generate UVs if there arent any
		uvs = PoolVector2Array();
		uvs.resize(positions.size());
	}

	// Separate triangles belonging to faces of the cube

	{
		PoolIntArray::Read indices_read = indices.read();
		PoolVector3Array::Read positions_read = positions.read();
		PoolVector3Array::Read normals_read = normals.read();
		PoolVector2Array::Read uvs_read = uvs.read();

		FixedArray<HashMap<int, int>, Cube::SIDE_COUNT> added_side_indices;
		HashMap<int, int> added_regular_indices;
		FixedArray<Vector3, 3> tri_positions;

		Voxel::BakedData::Model &model = baked_data.model;

		for (int i = 0; i < indices.size(); i += 3) {
			Cube::SideAxis side;

			tri_positions[0] = positions_read[indices_read[i]];
			tri_positions[1] = positions_read[indices_read[i + 1]];
			tri_positions[2] = positions_read[indices_read[i + 2]];

			if (L::get_triangle_side(tri_positions[0], tri_positions[1], tri_positions[2], side)) {
				// That triangle is on the face

				int next_side_index = model.side_positions[side].size();

				for (int j = 0; j < 3; ++j) {
					int src_index = indices_read[i + j];
					const int *existing_dst_index = added_side_indices[side].getptr(src_index);

					if (existing_dst_index == nullptr) {
						// Add new vertex

						model.side_indices[side].push_back(next_side_index);
						model.side_positions[side].push_back(tri_positions[j]);
						model.side_uvs[side].push_back(uvs_read[indices_read[i + j]]);

						added_side_indices[side].set(src_index, next_side_index);
						++next_side_index;

					} else {
						// Vertex was already added, just add index referencing it
						model.side_indices[side].push_back(*existing_dst_index);
					}
				}

			} else {
				// That triangle is not on the face

				int next_regular_index = model.positions.size();

				for (int j = 0; j < 3; ++j) {
					int src_index = indices_read[i + j];
					const int *existing_dst_index = added_regular_indices.getptr(src_index);

					if (existing_dst_index == nullptr) {
						model.indices.push_back(next_regular_index);
						model.positions.push_back(tri_positions[j]);
						model.normals.push_back(normals_read[indices_read[i + j]]);
						model.uvs.push_back(uvs_read[indices_read[i + j]]);

						added_regular_indices.set(src_index, next_regular_index);
						++next_regular_index;

					} else {
						model.indices.push_back(*existing_dst_index);
					}
				}
			}
		}
	}
}

void Voxel::bake(BakedData &baked_data, int p_atlas_size) {
	baked_data.clear();

	// baked_data.contributes_to_ao is set by the side culling phase
	baked_data.is_transparent = _is_transparent;
	baked_data.material_id = _material_id;
	baked_data.color = _color;

	switch (_geometry_type) {
		case GEOMETRY_NONE:
			baked_data.empty = true;
			break;

		case GEOMETRY_CUBE:
			bake_cube_geometry(*this, baked_data, p_atlas_size);
			break;

		case GEOMETRY_CUSTOM_MESH:
			bake_mesh_geometry(*this, baked_data);
			break;

		default:
			ERR_PRINT("Wtf? Unknown geometry type");
			break;
	}

	_empty = baked_data.empty;
}

void Voxel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_voxel_name", "name"), &Voxel::set_voxel_name);
	ClassDB::bind_method(D_METHOD("get_voxel_name"), &Voxel::get_voxel_name);

	ClassDB::bind_method(D_METHOD("set_id", "id"), &Voxel::set_id);
	ClassDB::bind_method(D_METHOD("get_id"), &Voxel::get_id);

	ClassDB::bind_method(D_METHOD("set_color", "color"), &Voxel::set_color);
	ClassDB::bind_method(D_METHOD("get_color"), &Voxel::get_color);

	ClassDB::bind_method(D_METHOD("set_transparent", "transparent"), &Voxel::set_transparent);
	ClassDB::bind_method(D_METHOD("is_transparent"), &Voxel::is_transparent);

	ClassDB::bind_method(D_METHOD("set_random_tickable", "rt"), &Voxel::set_random_tickable);
	ClassDB::bind_method(D_METHOD("is_random_tickable"), &Voxel::is_random_tickable);

	ClassDB::bind_method(D_METHOD("set_material_id", "id"), &Voxel::set_material_id);
	ClassDB::bind_method(D_METHOD("get_material_id"), &Voxel::get_material_id);

	ClassDB::bind_method(D_METHOD("set_geometry_type", "type"), &Voxel::set_geometry_type);
	ClassDB::bind_method(D_METHOD("get_geometry_type"), &Voxel::get_geometry_type);

	ClassDB::bind_method(D_METHOD("set_custom_mesh", "type"), &Voxel::set_custom_mesh);
	ClassDB::bind_method(D_METHOD("get_custom_mesh"), &Voxel::get_custom_mesh);

	ClassDB::bind_method(D_METHOD("set_collision_aabbs", "aabbs"), &Voxel::_b_set_collision_aabbs);
	ClassDB::bind_method(D_METHOD("get_collision_aabbs"), &Voxel::_b_get_collision_aabbs);

	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &Voxel::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &Voxel::get_collision_mask);

	ClassDB::bind_method(D_METHOD("is_empty"), &Voxel::is_empty);

	// TODO Update to StringName in Godot 4
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "voxel_name"), "set_voxel_name", "get_voxel_name");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "transparent"), "set_transparent", "is_transparent");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "random_tickable"), "set_random_tickable", "is_random_tickable");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "material_id"), "set_material_id", "get_material_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "geometry_type", PROPERTY_HINT_ENUM, "None,Cube,CustomMesh"), "set_geometry_type", "get_geometry_type");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "custom_mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_custom_mesh", "get_custom_mesh");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "collision_aabbs", PROPERTY_HINT_TYPE_STRING, itos(Variant::AABB) + ":"), "set_collision_aabbs", "get_collision_aabbs");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");

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

Array Voxel::_b_get_collision_aabbs() const {
	Array array;
	array.resize(_collision_aabbs.size());
	for (size_t i = 0; i < _collision_aabbs.size(); ++i) {
		array[i] = _collision_aabbs[i];
	}
	return array;
}

void Voxel::_b_set_collision_aabbs(Array array) {
	for (int i = 0; i < array.size(); ++i) {
		const Variant v = array[i];
		ERR_FAIL_COND(v.get_type() != Variant::AABB);
	}
	_collision_aabbs.resize(array.size());
	for (int i = 0; i < array.size(); ++i) {
		const AABB aabb = array[i];
		_collision_aabbs[i] = aabb;
	}
}
