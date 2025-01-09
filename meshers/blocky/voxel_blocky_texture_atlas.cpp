#include "voxel_blocky_texture_atlas.h"
#include "../../util/godot/classes/atlas_texture.h"
#include "../../util/godot/core/string.h"
#include "../../util/math/vector2i.h"

namespace zylann::voxel {

// std::string_view get_csv_item_by_index(const std::string_view csv, const unsigned int item_index) {
// 	unsigned int current_item_index = 0;
// 	unsigned int begin_pos = 0;
//
// 	for (unsigned int i = 0; i < csv.size(); ++i) {
// 		const char c = csv[i];
// 		if (c == ' ') {
// 			if (current_item_index == item_index) {
// 				return csv.substr(begin_pos, i - begin_pos);
// 			}
// 		} else if (c == ',') {
// 			if (current_item_index == item_index) {
// 				return csv.substr(begin_pos, i - begin_pos);
// 			}
// 			++i;
// 			++current_item_index;
// 			begin_pos = i;
// 		}
// 	}
//
// 	ZN_PRINT_ERROR("Item not found");
// 	return std::string_view();
// }

const char *VoxelBlockyTextureAtlas::TILE_TYPE_HINT_STRING = "Single,Blob9,Random,Extended";

Vector2i VoxelBlockyTextureAtlas::get_default_tile_resolution() const {
	return _default_tile_resolution;
}

void VoxelBlockyTextureAtlas::set_default_tile_resolution(const Vector2i res) {
	const Vector2i checked_res = math::max(res, Vector2i(1, 1));
	if (checked_res == _default_tile_resolution) {
		return;
	}
	_default_tile_resolution = checked_res;
}

void VoxelBlockyTextureAtlas::emit_changed(const ChangeType change) {
	_last_change = change;
	Resource::emit_changed();
}

Vector2i VoxelBlockyTextureAtlas::get_resolution() const {
	return _atlas_resolution;
}

void VoxelBlockyTextureAtlas::set_texture(Ref<Texture2D> texture) {
	if (_texture == texture) {
		return;
	}
	_texture = texture;
	if (texture.is_valid()) {
		_atlas_resolution = texture->get_size();
	}
	emit_changed(CHANGE_TEXTURE);
}

Ref<Texture2D> VoxelBlockyTextureAtlas::get_texture() {
	return _texture;
}

bool VoxelBlockyTextureAtlas::is_valid_tile_id(int tile_id) const {
	return tile_id >= 0 && tile_id < _tiles.size() && !_tiles[tile_id].is_tombstone();
}

void VoxelBlockyTextureAtlas::set_tile_type(int tile_id, VoxelBlockyTextureAtlas::TileType type) {
	ZN_ASSERT_RETURN(is_valid_tile_id(tile_id));
	ZN_ASSERT_RETURN(type >= 0 && type < TILE_TYPE_MAX);
	Tile &tile = _tiles[tile_id];
	const blocky::TileType checked_tile_type = static_cast<blocky::TileType>(type);
	if (checked_tile_type == tile.type) {
		return;
	}
	tile.type = tile.type;
	emit_changed(CHANGE_TILE_MODIFIED);
}

VoxelBlockyTextureAtlas::TileType VoxelBlockyTextureAtlas::get_tile_type(int tile_id) const {
	ZN_ASSERT_RETURN_V(is_valid_tile_id(tile_id), TILE_TYPE_MAX);
	const Tile &tile = _tiles[tile_id];
	return static_cast<TileType>(tile.type);
}

void VoxelBlockyTextureAtlas::set_tile_name(int tile_id, String tile_name) {
	ZN_ASSERT_RETURN(is_valid_tile_id(tile_id));
	Tile &tile = _tiles[tile_id];
	const StdString tile_name_std = zylann::godot::to_std_string(tile_name);
	if (tile_name_std == tile.name) {
		return;
	}
	tile.name = tile_name_std;
	emit_changed(CHANGE_TILE_MODIFIED);
}

String VoxelBlockyTextureAtlas::get_tile_name(int tile_id) const {
	ZN_ASSERT_RETURN_V(is_valid_tile_id(tile_id), String());
	const Tile &tile = _tiles[tile_id];
	return tile.name.c_str();
}

void VoxelBlockyTextureAtlas::set_tile_position(int tile_id, Vector2i position) {
	ZN_ASSERT_RETURN(is_valid_tile_id(tile_id));
	ZN_ASSERT_RETURN(get_supported_tile_position_range().has_point(position));
	Tile &tile = _tiles[tile_id];
	const uint16_t x = position.x;
	const uint16_t y = position.y;
	if (x == tile.position_x && y == tile.position_y) {
		return;
	}
	tile.position_x = x;
	tile.position_y = y;
	emit_changed(CHANGE_TILE_MODIFIED);
}

Vector2i VoxelBlockyTextureAtlas::get_tile_position(int tile_id) const {
	ZN_ASSERT_RETURN_V(is_valid_tile_id(tile_id), Vector2i());
	const Tile &tile = _tiles[tile_id];
	return Vector2i(tile.position_x, tile.position_y);
}

void VoxelBlockyTextureAtlas::set_tile_group_size(int tile_id, Vector2i size) {
	ZN_ASSERT_RETURN(is_valid_tile_id(tile_id));
	Tile &tile = _tiles[tile_id];
	ZN_ASSERT_RETURN(get_supported_tile_group_size_range().has_point(size));
	const uint8_t x = size.x;
	const uint8_t y = size.y;
	if (x == tile.group_size_x && y == tile.group_size_y) {
		return;
	}
	tile.group_size_x = x;
	tile.group_size_y = y;
	emit_changed(CHANGE_TILE_MODIFIED);
}

Vector2i VoxelBlockyTextureAtlas::get_tile_group_size(int tile_id) const {
	ZN_ASSERT_RETURN_V(is_valid_tile_id(tile_id), Vector2i());
	const Tile &tile = _tiles[tile_id];
	return Vector2i(tile.group_size_x, tile.group_size_y);
}

void VoxelBlockyTextureAtlas::set_tile_random_rotation(int tile_id, bool enabled) {
	ZN_ASSERT_RETURN(is_valid_tile_id(tile_id));
	Tile &tile = _tiles[tile_id];
	if (enabled == tile.random_rotation) {
		return;
	}
	tile.random_rotation = enabled;
	emit_changed(CHANGE_TILE_MODIFIED);
}

bool VoxelBlockyTextureAtlas::get_tile_random_rotation(int tile_id) const {
	ZN_ASSERT_RETURN_V(is_valid_tile_id(tile_id), false);
	const Tile &tile = _tiles[tile_id];
	return tile.random_rotation;
}

Span<const VoxelBlockyTextureAtlas::Tile> VoxelBlockyTextureAtlas::get_tiles() const {
	return to_span_const(_tiles);
}

VoxelBlockyTextureAtlas::Tile &VoxelBlockyTextureAtlas::get_tile(const uint32_t id) {
	return _tiles[id];
}

uint32_t VoxelBlockyTextureAtlas::add_tile(const Tile &tile) {
	const uint32_t i = allocate_tile();
	_tiles[i] = tile;
	emit_changed(CHANGE_TILE_ADDED);
	return i;
}

void VoxelBlockyTextureAtlas::remove_tile(const uint32_t id) {
	ZN_ASSERT_RETURN(is_valid_tile_id(id));
	_tiles[id].clear();
	emit_changed(CHANGE_TILE_REMOVED);
}

uint32_t VoxelBlockyTextureAtlas::allocate_tile() {
	for (unsigned int i = 0; i < _tiles.size(); ++i) {
		const Tile &tile = _tiles[i];
		if (tile.is_tombstone()) {
			return i;
		}
	}
	const uint32_t i = _tiles.size();
	_tiles.push_back(Tile());
	return i;
}

Array VoxelBlockyTextureAtlas::_b_get_tiles_data() const {
	Array tiles_data;
	tiles_data.resize(1 + _tiles.size());

	// Version tag
	tiles_data.append(1);

	for (unsigned int tile_index = 0; tile_index < _tiles.size(); ++tile_index) {
		const Tile &tile = _tiles[tile_index];

		Array tile_data;
		tile_data.resize(5);
		tile_data[0] = tile.type;
		tile_data[1] = Vector2i(tile.position_x, tile.position_y);
		tile_data[2] = Vector2i(tile.group_size_x, tile.group_size_y);
		tile_data[3] = String(tile.name.c_str());
		tile_data[4] = tile.random_rotation;

		tiles_data[tile_index] = tile_data;
	}

	return tiles_data;
}

void VoxelBlockyTextureAtlas::_b_set_tiles_data(Array tiles_data) {
	ERR_FAIL_COND(tiles_data.size() < 1);

	const int version = tiles_data[0];
	ERR_FAIL_COND(version != 1);

	_tiles.resize(tiles_data.size() - 1);
	for (unsigned int tile_index = 0; tile_index < _tiles.size(); ++tile_index) {
		const Array tile_data = tiles_data[tile_index];
		ZN_ASSERT_CONTINUE(tile_data.size() == 5);

		Tile &tile = _tiles[tile_index];

		const int type = tile_data[0];
		ZN_ASSERT_CONTINUE(type >= 0 && type < blocky::TILE_MAX);
		tile.type = static_cast<blocky::TileType>(type);

		const Vector2i position = tile_data[1];
		ZN_ASSERT_CONTINUE(position.x >= 0 && position.y >= 0);
		tile.position_x = position.x;
		tile.position_y = position.y;

		const Vector2i group_size = tile_data[2];
		ZN_ASSERT_CONTINUE(group_size.x >= 1 && group_size.y >= 1);
		tile.group_size_x = group_size.x;
		tile.group_size_y = group_size.y;

		const String tile_name = tile_data[3];
		tile.name = godot::to_std_string(tile_name);

		tile.random_rotation = tile_data[4];
	}
}

String VoxelBlockyTextureAtlas::get_tile_name_or_default(const uint32_t id) const {
	const Tile &tile = _tiles[id];
	String title;
	if (tile.name.size() == 0) {
		title = String("<unnamed{0}>").format(varray(id));
	} else {
		title = tile.name.c_str();
	}
	return title;
}

VoxelBlockyTextureAtlas::ChangeType VoxelBlockyTextureAtlas::get_last_change() const {
	return _last_change;
}

Ref<AtlasTexture> VoxelBlockyTextureAtlas::get_tile_preview_texture(const uint32_t id) const {
	ZN_ASSERT_RETURN_V(is_valid_tile_id(id), Ref<AtlasTexture>());
	const Tile &tile = _tiles[id];
	const Rect2 rect(tile.position_x, tile.position_y, _default_tile_resolution.x, _default_tile_resolution.y);
	Ref<AtlasTexture> texture;
	texture.instantiate();
	texture->set_atlas(_texture);
	texture->set_region(rect);
	return texture;
}

#ifdef TOOLS_ENABLED

int VoxelBlockyTextureAtlas::get_tile_id_at_pixel_position(const Vector2i pos) {
	for (uint32_t tile_index = 0; tile_index < _tiles.size(); ++tile_index) {
		const VoxelBlockyTextureAtlas::Tile &tile = _tiles[tile_index];
		if (tile.is_tombstone()) {
			continue;
		}
		const Vector2i ts = _default_tile_resolution;
		const Rect2i rect(
				Vector2i(tile.position_x, tile.position_y), Vector2i(tile.group_size_x, tile.group_size_y) * ts
		);
		if (rect.has_point(pos)) {
			return tile_index;
		}
	}
	return -1;
}

void VoxelBlockyTextureAtlas::get_configuration_warnings(PackedStringArray &out_warnings) const {
	// TODO Check if there are tiles out of bounds
	// TODO Check if tiles have the same name
}

#endif

Rect2i VoxelBlockyTextureAtlas::get_supported_tile_position_range() {
	return Rect2i(Vector2i(0, 0), Vector2i(std::numeric_limits<uint16_t>::max(), std::numeric_limits<uint16_t>::max()));
}

Rect2i VoxelBlockyTextureAtlas::get_supported_tile_group_size_range() {
	return Rect2i(Vector2i(1, 1), Vector2i(std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()));
}

void VoxelBlockyTextureAtlas::_bind_methods() {
	using Self = VoxelBlockyTextureAtlas;

	ClassDB::bind_method(D_METHOD("set_texture", "texture"), &Self::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &Self::get_texture);

	ClassDB::bind_method(D_METHOD("set_default_tile_resolution", "resolution"), &Self::set_default_tile_resolution);
	ClassDB::bind_method(D_METHOD("get_default_tile_resolution"), &Self::get_default_tile_resolution);

	ClassDB::bind_method(D_METHOD("set_tile_name", "tile_id", "name"), &Self::set_tile_name);
	ClassDB::bind_method(D_METHOD("get_tile_name"), &Self::get_tile_name);

	ClassDB::bind_method(D_METHOD("set_tile_type", "tile_id", "type"), &Self::set_tile_type);
	ClassDB::bind_method(D_METHOD("get_tile_type"), &Self::get_tile_type);

	ClassDB::bind_method(D_METHOD("set_tile_position", "tile_id", "pos"), &Self::set_tile_position);
	ClassDB::bind_method(D_METHOD("get_tile_position"), &Self::get_tile_position);

	ClassDB::bind_method(D_METHOD("set_tile_group_size", "tile_id", "size"), &Self::set_tile_group_size);
	ClassDB::bind_method(D_METHOD("get_tile_group_size"), &Self::get_tile_group_size);

	ClassDB::bind_method(D_METHOD("set_tile_random_rotation", "tile_id", "enabled"), &Self::set_tile_random_rotation);
	ClassDB::bind_method(D_METHOD("get_tile_random_rotation"), &Self::get_tile_random_rotation);

	ClassDB::bind_method(D_METHOD("_get_tiles_data"), &Self::_b_get_tiles_data);
	ClassDB::bind_method(D_METHOD("_set_tiles_data", "data"), &Self::_b_set_tiles_data);

	ADD_PROPERTY(
			PropertyInfo(
					Variant::OBJECT,
					"texture",
					PROPERTY_HINT_RESOURCE_TYPE,
					Texture2D::get_class_static(),
					PROPERTY_USAGE_DEFAULT
			),
			"set_texture",
			"get_texture"
	);

	ADD_PROPERTY(
			PropertyInfo(Variant::VECTOR2I, "default_tile_resolution"),
			"set_default_tile_resolution",
			"get_default_tile_resolution"
	);

	ADD_PROPERTY(
			PropertyInfo(Variant::ARRAY, "_tiles_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE),
			"set_texture",
			"get_texture"
	);

	BIND_ENUM_CONSTANT(TILE_TYPE_SINGLE);
	BIND_ENUM_CONSTANT(TILE_TYPE_BLOB9);
	BIND_ENUM_CONSTANT(TILE_TYPE_RANDOM);
	BIND_ENUM_CONSTANT(TILE_TYPE_EXTENDED);
	BIND_ENUM_CONSTANT(TILE_TYPE_MAX);
}

} // namespace zylann::voxel
