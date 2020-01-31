#include "voxel.h"
#include "voxel_library.h"
#include "voxel_mesher_blocky.h" // TODO Only required because of MAX_MATERIALS... could be enough inverting that dependency

#define STRLEN(x) (sizeof(x) / sizeof(x[0]))

Voxel::Voxel() :
		_library(0),
		_id(-1),
		_material_id(0),
		_is_transparent(false),
		_color(1.f, 1.f, 1.f),
		_geometry_type(GEOMETRY_NONE),
		_cube_geometry_padding_y(0) {}

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

	} else if (name == "cube_geometry/padding_y") {

		_cube_geometry_padding_y = p_value;
		set_cube_geometry(_cube_geometry_padding_y);
		return true;
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

	} else if (name == "cube_geometry/padding_y") {

		r_ret = _cube_geometry_padding_y;
		return true;
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

Ref<Voxel> Voxel::set_voxel_name(String name) {
	_name = name;
	return Ref<Voxel>(this);
}

Ref<Voxel> Voxel::set_id(int id) {
	ERR_FAIL_COND_V(id < 0 || id >= 256, Ref<Voxel>(this));
	// Cannot modify ID after creation
	ERR_FAIL_COND_V(_id != -1, Ref<Voxel>(this));
	_id = id;
	return Ref<Voxel>(this);
}

Ref<Voxel> Voxel::set_color(Color color) {
	_color = color;
	return Ref<Voxel>(this);
}

Ref<Voxel> Voxel::set_material_id(unsigned int id) {
	ERR_FAIL_COND_V(id >= VoxelMesherBlocky::MAX_MATERIALS, Ref<Voxel>(this));
	_material_id = id;
	return Ref<Voxel>(this);
}

Ref<Voxel> Voxel::set_transparent(bool t) {
	_is_transparent = t;
	return Ref<Voxel>(this);
}

void Voxel::clear_geometry() {

	_model_positions.resize(0);
	_model_normals.resize(0);
	_model_uvs.resize(0);
	_model_indices.resize(0);

	for (int side = 0; side < Cube::SIDE_COUNT; ++side) {
		_model_side_positions[side].resize(0);
		_model_side_uvs[side].resize(0);
		_model_side_indices[side].resize(0);
	}
}

void Voxel::set_geometry_type(GeometryType type) {

	_geometry_type = type;

	switch (_geometry_type) {

		case GEOMETRY_NONE:
			clear_geometry();
			break;

		case GEOMETRY_CUBE:
			set_cube_geometry(_cube_geometry_padding_y);
			update_cube_uv_sides();
			break;

		case GEOMETRY_CUSTOM_MESH:
			// TODO Re-update geometry?
			break;

		default:
			ERR_PRINT("Wtf? Unknown geometry type");
			break;
	}
}

Voxel::GeometryType Voxel::get_geometry_type() const {
	return _geometry_type;
}

void Voxel::set_library(Ref<VoxelLibrary> lib) {
	if (lib.is_null()) {
		_library = 0;
	} else {
		_library = lib->get_instance_id();
	}
	if(_geometry_type == GEOMETRY_CUBE) {
		// Update model UVs because atlas size is defined by the library
		update_cube_uv_sides();
	}
}

VoxelLibrary *Voxel::get_library() const {
	if (_library == 0) {
		return NULL;
	}
	Object *v = ObjectDB::get_instance(_library);
	if (v) {
		return Object::cast_to<VoxelLibrary>(v);
	}
	return NULL;
}

void Voxel::set_custom_mesh(Ref<Mesh> mesh) {

	if(mesh == _custom_mesh) {
		return;
	}

	clear_geometry();

	if(mesh.is_null()) {
		_custom_mesh = Ref<Mesh>();
		return;
	}

	Array arrays = mesh->surface_get_arrays(0);
	PoolIntArray indices = arrays[Mesh::ARRAY_INDEX];
	PoolVector3Array positions = arrays[Mesh::ARRAY_VERTEX];
	PoolVector3Array normals = arrays[Mesh::ARRAY_NORMAL];
	PoolVector2Array uvs = arrays[Mesh::ARRAY_TEX_UV];

	ERR_FAIL_COND_MSG(indices.size() % 3 != 0, "Mesh is empty or does not contain triangles");
	ERR_FAIL_COND(normals.size() == 0);

	struct L {
		static bool get_side(Vector3 pos, Cube::SideAxis &out_side) {
			if (Math::is_equal_approx(pos.x, 0.0)) {
				out_side = Cube::SIDE_NEGATIVE_X;
				return true;
			}
			if (Math::is_equal_approx(pos.x, 1.0)) {
				out_side = Cube::SIDE_POSITIVE_X;
				return true;
			}
			if (Math::is_equal_approx(pos.y, 0.0)) {
				out_side = Cube::SIDE_NEGATIVE_Y;
				return true;
			}
			if (Math::is_equal_approx(pos.y, 1.0)) {
				out_side = Cube::SIDE_POSITIVE_Y;
				return true;
			}
			if (Math::is_equal_approx(pos.z, 0.0)) {
				out_side = Cube::SIDE_NEGATIVE_Z;
				return true;
			}
			if (Math::is_equal_approx(pos.z, 1.0)) {
				out_side = Cube::SIDE_POSITIVE_Z;
				return true;
			}
			return false;
		}
	};

	if(uvs.size() == 0) {
		// TODO Properly generate UVs if there arent any
		uvs = PoolVector2Array();
		uvs.resize(positions.size());
	}

	_custom_mesh = mesh;

	// Separate triangles belonging to faces of the cube

	{
		PoolIntArray::Read indices_read = indices.read();
		PoolVector3Array::Read positions_read = positions.read();
		PoolVector3Array::Read normals_read = normals.read();
		PoolVector2Array::Read uvs_read = uvs.read();

		FixedArray<HashMap<int, int>, Cube::SIDE_COUNT> added_side_indices;
		HashMap<int, int> added_regular_indices;

		for (int i = 0; i < indices.size(); i += 3) {

			Cube::SideAxis side0;
			Cube::SideAxis side1;
			Cube::SideAxis side2;

			if (L::get_side(positions_read[indices_read[i]], side0) &&
				L::get_side(positions_read[indices_read[i + 1]], side1) &&
				L::get_side(positions_read[indices_read[i + 2]], side2) &&
				side0 == side1 &&
				side1 == side2) {

				// That triangle is on the face

				int next_side_index = _model_side_positions[side0].size();

				for(int j = 0; j < 3; ++j) {
					int src_index = indices_read[i + j];
					const int *existing_dst_index = added_side_indices[side0].getptr(src_index);

					if(existing_dst_index == nullptr) {
						// Add new vertex

						_model_side_indices[side0].push_back(next_side_index);
						_model_side_positions[side0].push_back(positions_read[indices_read[i + j]]);
						_model_side_uvs[side0].push_back(uvs_read[indices_read[i + j]]);

						added_side_indices[side0].set(src_index, next_side_index);
						++next_side_index;

					} else {
						// Vertex was already added, just add index referencing it
						_model_side_indices[side0].push_back(*existing_dst_index);
					}
				}

			} else {
				// That triangle is not on the face

				int next_regular_index = _model_positions.size();

				for (int j = 0; j < 3; ++j) {
					int src_index = indices_read[i + j];
					const int *existing_dst_index = added_regular_indices.getptr(src_index);

					if (existing_dst_index == nullptr) {

						_model_indices.push_back(next_regular_index);
						_model_positions.push_back(positions_read[indices_read[i + j]]);
						_model_normals.push_back(normals_read[indices_read[i + j]]);
						_model_uvs.push_back(uvs_read[indices_read[i + j]]);

						added_regular_indices.set(src_index, next_regular_index);
						++next_regular_index;

					} else {
						_model_indices.push_back(*existing_dst_index);
					}
				}
			}
		}
	}
}

Ref<Voxel> Voxel::set_cube_geometry(float sy) {
	sy = 1.0 + sy;

	for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {

		std::vector<Vector3> &positions = _model_side_positions[side];
		positions.resize(4);
		for (unsigned int i = 0; i < 4; ++i) {
			int corner = Cube::g_side_corners[side][i];
			Vector3 p = Cube::g_corner_position[corner];
			if (p.y > 0.9) {
				p.y = sy;
			}
			positions[i] = p;
		}

		std::vector<int> &indices = _model_side_indices[side];
		indices.resize(6);
		for (unsigned int i = 0; i < 6; ++i) {
			indices[i] = Cube::g_side_quad_triangles[side][i];
		}
	}

	return Ref<Voxel>(this);
}

void Voxel::set_cube_uv_side(int side, Vector2 tile_pos) {
	_cube_tiles[side] = tile_pos;
	// TODO Better have a dirty flag, otherwise UVs will be needlessly updated at least 6 times everytime a Voxel resource is loaded!
	update_cube_uv_sides();
}

void Voxel::update_cube_uv_sides() {
	VoxelLibrary *library = get_library();
	//ERR_FAIL_COND(library == NULL);
	if (library == NULL) {
		// Not an error, the Voxel might have been created before the library, and can't be used without anyways
		print_line("VoxelLibrary not set yet");
		return;
	}

	float e = 0.001;
	// Winding is the same as the one chosen in Cube:: vertices
	// I am confused. I read in at least 3 OpenGL tutorials that texture coordinates start at bottom-left (0,0).
	// But even though Godot is said to follow OpenGL's convention, the engine starts at top-left!
	const Vector2 uv[4] = {
		Vector2(e, 1.f - e),
		Vector2(1.f - e, 1.f - e),
		Vector2(1.f - e, e),
		Vector2(e, e),
	};

	float atlas_size = (float)library->get_atlas_size();
	CRASH_COND(atlas_size <= 0);
	float s = 1.0 / atlas_size;

	for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {
		_model_side_uvs[side].resize(4);
		std::vector<Vector2> &uvs = _model_side_uvs[side];
		for (unsigned int i = 0; i < 4; ++i) {
			uvs[i] = (_cube_tiles[side] + uv[i]) * s;
		}
	}
}

//Ref<Voxel> Voxel::set_xquad_geometry(Vector2 atlas_pos) {
//    // TODO
//    return Ref<Voxel>(this);
//}

void Voxel::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_voxel_name", "name"), &Voxel::set_voxel_name);
	ClassDB::bind_method(D_METHOD("get_voxel_name"), &Voxel::get_voxel_name);

	ClassDB::bind_method(D_METHOD("set_id", "id"), &Voxel::set_id);
	ClassDB::bind_method(D_METHOD("get_id"), &Voxel::get_id);

	ClassDB::bind_method(D_METHOD("set_color", "color"), &Voxel::set_color);
	ClassDB::bind_method(D_METHOD("get_color"), &Voxel::get_color);

	ClassDB::bind_method(D_METHOD("set_transparent", "transparent"), &Voxel::set_transparent);
	ClassDB::bind_method(D_METHOD("is_transparent"), &Voxel::is_transparent);

	ClassDB::bind_method(D_METHOD("set_material_id", "id"), &Voxel::set_material_id);
	ClassDB::bind_method(D_METHOD("get_material_id"), &Voxel::get_material_id);

	ClassDB::bind_method(D_METHOD("set_geometry_type", "type"), &Voxel::set_geometry_type);
	ClassDB::bind_method(D_METHOD("get_geometry_type"), &Voxel::get_geometry_type);

	ClassDB::bind_method(D_METHOD("set_custom_mesh", "type"), &Voxel::set_custom_mesh);
	ClassDB::bind_method(D_METHOD("get_custom_mesh"), &Voxel::get_custom_mesh);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "voxel_name"), "set_voxel_name", "get_voxel_name");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "transparent"), "set_transparent", "is_transparent");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "material_id"), "set_material_id", "get_material_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "geometry_type", PROPERTY_HINT_ENUM, "None,Cube,CustomMesh"), "set_geometry_type", "get_geometry_type");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "custom_mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_custom_mesh", "get_custom_mesh");

	BIND_ENUM_CONSTANT(GEOMETRY_NONE);
	BIND_ENUM_CONSTANT(GEOMETRY_CUBE);
	BIND_ENUM_CONSTANT(GEOMETRY_CUSTOM_MESH);
	BIND_ENUM_CONSTANT(GEOMETRY_MAX);
}
