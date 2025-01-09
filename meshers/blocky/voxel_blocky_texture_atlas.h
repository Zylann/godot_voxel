#ifndef VOXEL_BLOCKY_TILE_ATLAS_H
#define VOXEL_BLOCKY_TILE_ATLAS_H

#include "../../util/godot/classes/resource.h"
#include "../../util/godot/classes/texture_2d.h"
#include "../../util/godot/macros.h"
#include "../../util/math/vector2i.h"
#include "../../util/string/std_string.h"
#include "blocky_baked_library.h"

ZN_GODOT_FORWARD_DECLARE(class AtlasTexture);

namespace zylann::voxel {

// Note, this is almost unrelated to voxels and could be separated. But the fact its low-level API integrates with
// blocky voxels I'm not sure if I should move it out

// Describes a texture atlas
class VoxelBlockyTextureAtlas : public Resource {
	GDCLASS(VoxelBlockyTextureAtlas, Resource)
public:
	struct Tile {
		StdString name;
		// uint16_t resolution_x = 0;
		// uint16_t resolution_y = 0;
		uint16_t position_x = 0;
		uint16_t position_y = 0;
		uint8_t group_size_x = 0;
		uint8_t group_size_y = 0;
		blocky::TileType type = blocky::TILE_SINGLE;
		bool random_rotation = false;

		inline bool is_tombstone() const {
			return group_size_x == 0;
		}

		inline void clear() {
			name.clear();
			group_size_x = 0;
		}
	};

	enum TileType {
		TILE_TYPE_SINGLE = blocky::TILE_SINGLE,
		TILE_TYPE_BLOB9 = blocky::TILE_BLOB9,
		TILE_TYPE_RANDOM = blocky::TILE_RANDOM,
		TILE_TYPE_EXTENDED = blocky::TILE_EXTENDED,
		TILE_TYPE_MAX = blocky::TILE_MAX,
	};

	static const char *TILE_TYPE_HINT_STRING;

	Vector2i get_default_tile_resolution() const;
	void set_default_tile_resolution(const Vector2i res);

	// void set_resolution(const Vector2i res);
	Vector2i get_resolution() const;

	void set_texture(Ref<Texture2D> texture);
	Ref<Texture2D> get_texture();

	bool is_valid_tile_id(int tile_id) const;

	void set_tile_type(int tile_id, TileType type);
	TileType get_tile_type(int tile_id) const;

	void set_tile_name(int tile_id, String tile_name);
	String get_tile_name(int tile_id) const;

	void set_tile_position(int tile_id, Vector2i position);
	Vector2i get_tile_position(int tile_id) const;

	void set_tile_group_size(int tile_id, Vector2i size);
	Vector2i get_tile_group_size(int tile_id) const;

	void set_tile_random_rotation(int tile_id, bool enabled);
	bool get_tile_random_rotation(int tile_id) const;

	// Internal

	Span<const Tile> get_tiles() const;

	Tile &get_tile(const uint32_t id);

	uint32_t add_tile(const Tile &tile);
	void remove_tile(const uint32_t id);

#ifdef TOOLS_ENABLED
	int get_tile_id_at_pixel_position(const Vector2i pos);

	void get_configuration_warnings(PackedStringArray &out_warnings) const;
#endif

	String get_tile_name_or_default(const uint32_t id) const;

	enum ChangeType {
		CHANGE_TILE_ADDED,
		CHANGE_TILE_REMOVED,
		CHANGE_TILE_MODIFIED,
		CHANGE_TEXTURE,
	};

	ChangeType get_last_change() const;

	Ref<AtlasTexture> get_tile_preview_texture(const uint32_t id) const;

	static Rect2i get_supported_tile_position_range();
	static Rect2i get_supported_tile_group_size_range();

private:
	uint32_t allocate_tile();
	void emit_changed(const ChangeType change);

	Array _b_get_tiles_data() const;
	void _b_set_tiles_data(Array tiles_data);

	static void _bind_methods();

	Vector2i _default_tile_resolution = Vector2i(16, 16);
	Vector2i _atlas_resolution = Vector2i(256, 256);
	StdVector<Tile> _tiles;
	// Not required, but useful to preview tiles
	Ref<Texture2D> _texture;

	ChangeType _last_change = CHANGE_TILE_ADDED;
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelBlockyTextureAtlas::TileType);

#endif // VOXEL_BLOCKY_TILE_ATLAS_H
