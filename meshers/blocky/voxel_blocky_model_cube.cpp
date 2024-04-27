#include "voxel_blocky_model_cube.h"
#include "../../util/containers/container_funcs.h"
#include "../../util/math/conv.h"

namespace zylann::voxel {

VoxelBlockyModelCube::VoxelBlockyModelCube() {
	_atlas_size_in_tiles = Vector2i(16, 16);
	_surface_count = 1;
	_collision_aabbs.push_back(AABB(Vector3(0, 0, 0), Vector3(1, 1, 1)));
}

Cube::Side VoxelBlockyModelCube::name_to_side(const String &s) {
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

void VoxelBlockyModelCube::set_tile(VoxelBlockyModel::Side side, Vector2i pos) {
	pos.x = math::max(pos.x, 0);
	pos.y = math::max(pos.y, 0);
	if (_tiles[side] != pos) {
		_tiles[side] = pos;
		emit_changed();
	}
}

bool VoxelBlockyModelCube::_set(const StringName &p_name, const Variant &p_value) {
	const String property_name = p_name;

	if (property_name.begins_with("tile_")) {
		String s = property_name.substr(string_literal_length("tile_"), property_name.length());
		Cube::Side side = name_to_side(s);
		if (side != Cube::SIDE_COUNT) {
			Vector2i v = p_value;
			set_tile(VoxelBlockyModel::Side(side), v);
			return true;
		}
	}

	return false;
}

bool VoxelBlockyModelCube::_get(const StringName &p_name, Variant &r_ret) const {
	const String property_name = p_name;

	if (property_name.begins_with("tile_")) {
		String s = property_name.substr(string_literal_length("tile_"), property_name.length());
		Cube::Side side = name_to_side(s);
		if (side != Cube::SIDE_COUNT) {
			r_ret = get_tile(VoxelBlockyModel::Side(side));
			return true;
		}
	}

	return false;
}

void VoxelBlockyModelCube::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::NIL, "Cube tiles", PROPERTY_HINT_NONE, "tile_", PROPERTY_USAGE_GROUP));

	p_list->push_back(PropertyInfo(Variant::VECTOR2I, "tile_left"));
	p_list->push_back(PropertyInfo(Variant::VECTOR2I, "tile_right"));
	p_list->push_back(PropertyInfo(Variant::VECTOR2I, "tile_bottom"));
	p_list->push_back(PropertyInfo(Variant::VECTOR2I, "tile_top"));
	p_list->push_back(PropertyInfo(Variant::VECTOR2I, "tile_back"));
	p_list->push_back(PropertyInfo(Variant::VECTOR2I, "tile_front"));
}

void VoxelBlockyModelCube::set_height(float h) {
	_height = math::clamp(h, 0.01f, 1.f);

	if (get_collision_aabb_count() > 0) {
		// Make collision box match
		set_collision_aabb(0, AABB(Vector3(0, 0, 0), Vector3(1, _height, 1)));
	}

	emit_changed();
}

// Allow to specify AtlasTextures?

void VoxelBlockyModelCube::set_atlas_size_in_tiles(Vector2i s) {
	ZN_ASSERT_RETURN(s.x > 0);
	ZN_ASSERT_RETURN(s.y > 0);
	if (s != _atlas_size_in_tiles) {
		_atlas_size_in_tiles = s;
		emit_changed();
	}
}

Vector2i VoxelBlockyModelCube::get_atlas_size_in_tiles() const {
	return _atlas_size_in_tiles;
}

float VoxelBlockyModelCube::get_height() const {
	return _height;
}

namespace {

void bake_cube_geometry(const VoxelBlockyModelCube &config, VoxelBlockyModel::BakedData &baked_data,
		Vector2i p_atlas_size, bool bake_tangents) {
	const float height = config.get_height();

	baked_data.model.surface_count = 1;
	VoxelBlockyModel::BakedData::Surface &surface = baked_data.model.surfaces[0];

	for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {
		VoxelBlockyModel::BakedData::SideSurface &side_surface = surface.sides[side];
		StdVector<Vector3f> &positions = side_surface.positions;
		positions.resize(4);
		for (unsigned int i = 0; i < 4; ++i) {
			int corner = Cube::g_side_corners[side][i];
			Vector3f p = Cube::g_corner_position[corner];
			if (p.y > 0.9) {
				p.y = height;
			}
			positions[i] = p;
		}

		StdVector<int> &indices = side_surface.indices;
		indices.resize(6);
		for (unsigned int i = 0; i < 6; ++i) {
			indices[i] = Cube::g_side_quad_triangles[side][i];
		}
	}

	const float e = 0.001;
	// Winding is the same as the one chosen in Cube:: vertices
	// I am confused. I read in at least 3 OpenGL tutorials that texture coordinates start at bottom-left (0,0).
	// But even though Godot is said to follow OpenGL's convention, the engine starts at top-left!
	// And now in Godot 4 it's flipped again?
	const Vector2f uv_norm_top_bottom[4] = {
		Vector2f(e, 1.f - e),
		Vector2f(1.f - e, 1.f - e),
		Vector2f(1.f - e, e),
		Vector2f(e, e),
	};
	const float uv_top_y = Math::lerp(1.f - e, e, height);
	const Vector2f uv_norm_side[4] = {
		Vector2f(e, 1.f - e),
		Vector2f(1.f - e, 1.f - e),
		Vector2f(1.f - e, uv_top_y),
		Vector2f(e, uv_top_y),
	};

	const Vector2f atlas_size = to_vec2f(p_atlas_size);
	ZN_ASSERT_RETURN(atlas_size.x > 0);
	ZN_ASSERT_RETURN(atlas_size.y > 0);
	const Vector2f s = Vector2f(1.0f) / atlas_size;

	for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {
		VoxelBlockyModel::BakedData::SideSurface &side_surface = surface.sides[side];
		StdVector<Vector2f> &uvs = side_surface.uvs;
		uvs.resize(4);

		const Vector2f *uv_norm = Cube::g_side_normals[side].y != 0 ? uv_norm_top_bottom : uv_norm_side;

		for (unsigned int i = 0; i < 4; ++i) {
			uvs[i] = (to_vec2f(config.get_tile(VoxelBlockyModel::Side(side))) + uv_norm[i]) * s;
		}

		if (bake_tangents) {
			StdVector<float> &tangents = side_surface.tangents;
			for (unsigned int i = 0; i < 4; ++i) {
				for (unsigned int j = 0; j < 4; ++j) {
					tangents.push_back(Cube::g_side_tangents[side][j]);
				}
			}
		}
	}

	baked_data.empty = false;
}

} // namespace

void VoxelBlockyModelCube::bake(BakedData &baked_data, bool bake_tangents, MaterialIndexer &materials) const {
	baked_data.clear();
	bake_cube_geometry(*this, baked_data, _atlas_size_in_tiles, bake_tangents);
	VoxelBlockyModel::bake(baked_data, bake_tangents, materials);
}

bool VoxelBlockyModelCube::is_empty() const {
	return false;
}

Ref<Mesh> VoxelBlockyModelCube::get_preview_mesh() const {
	const bool bake_tangents = false;

	VoxelBlockyModel::BakedData baked_data;
	baked_data.color = get_color();
	bake_cube_geometry(*this, baked_data, _atlas_size_in_tiles, bake_tangents);

	Ref<Mesh> mesh = make_mesh_from_baked_data(baked_data, bake_tangents);

	for (unsigned int surface_index = 0; surface_index < _surface_count; ++surface_index) {
		Ref<Material> material = get_material_override(surface_index);
		mesh->surface_set_material(surface_index, material);
	}

	return mesh;
}

void VoxelBlockyModelCube::rotate_90(math::Axis axis, bool clockwise) {
	FixedArray<Vector2i, Cube::SIDE_COUNT> rotated_tiles;

	for (unsigned int src_side = 0; src_side < Cube::SIDE_COUNT; ++src_side) {
		const Vector3i dir = math::rotate_90(Cube::g_side_normals[src_side], axis, clockwise);
		Cube::Side dst_side = Cube::dir_to_side(dir);
		rotated_tiles[dst_side] = _tiles[src_side];
	}

	_tiles = rotated_tiles;

	// Collision boxes don't change with this kind of model. Height is always vertical.
	// VoxelBlockyModel::rotate_90(axis, clockwise);

	// Can't do that, it causes the sub-inspector to be entirely rebuilt, which fucks up the state of custom editors in
	// it...
	// notify_property_list_changed();

	emit_changed();
}

void VoxelBlockyModelCube::rotate_ortho(math::OrthoBasis ortho_basis) {
	FixedArray<Vector2i, Cube::SIDE_COUNT> rotated_tiles;

	for (unsigned int src_side = 0; src_side < Cube::SIDE_COUNT; ++src_side) {
		const Vector3i dir = ortho_basis.xform(Cube::g_side_normals[src_side]);
		Cube::Side dst_side = Cube::dir_to_side(dir);
		rotated_tiles[dst_side] = _tiles[src_side];
	}

	_tiles = rotated_tiles;

	// Collision boxes don't change with this kind of model. Height is always vertical.
	// VoxelBlockyModel::rotate_90(axis, clockwise);

	// Can't do that, it causes the sub-inspector to be entirely rebuilt, which fucks up the state of custom editors in
	// it...
	// notify_property_list_changed();

	emit_changed();
}

void VoxelBlockyModelCube::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_height", "h"), &VoxelBlockyModelCube::set_height);
	ClassDB::bind_method(D_METHOD("get_height"), &VoxelBlockyModelCube::get_height);

	ClassDB::bind_method(D_METHOD("set_tile", "side", "position"), &VoxelBlockyModelCube::set_tile);
	ClassDB::bind_method(D_METHOD("get_tile", "side"), &VoxelBlockyModelCube::get_tile);

	ClassDB::bind_method(D_METHOD("set_atlas_size_in_tiles", "ts"), &VoxelBlockyModelCube::set_atlas_size_in_tiles);
	ClassDB::bind_method(D_METHOD("get_atlas_size_in_tiles"), &VoxelBlockyModelCube::get_atlas_size_in_tiles);

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "height", PROPERTY_HINT_RANGE, "0.001,1,0.001"), "set_height", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "atlas_size_in_tiles"), "set_atlas_size_in_tiles",
			"get_atlas_size_in_tiles");
}

} // namespace zylann::voxel
