#include "blocky_atlas_indexer.h"
#include "../../util/containers/container_funcs.h"
#include "../../util/math/conv.h"

namespace zylann::voxel::blocky {

unsigned int AtlasIndexer::get_or_create_atlas_index(const Ref<VoxelBlockyTextureAtlas> &atlas) {
	size_t index = atlases.size();
	if (find<Ref<VoxelBlockyTextureAtlas>>(to_span_const(atlases), atlas, index)) {
		return index;
	}
	ZN_ASSERT_RETURN_V(index < std::numeric_limits<uint8_t>::max(), 0);
	atlases.push_back(atlas);
	src_to_baked_tile_indices.push_back(StdVector<uint16_t>());
	return index;
}

uint16_t AtlasIndexer::get_or_create_tile_index(const unsigned int atlas_index, const unsigned int src_tile_id) {
	ZN_ASSERT_RETURN_V(atlas_index < atlases.size(), 0);
	ZN_ASSERT_RETURN_V(src_tile_id < std::numeric_limits<uint16_t>::max(), 0);

	StdVector<uint16_t> &map = src_to_baked_tile_indices[atlas_index];
	if (src_tile_id < map.size()) {
		const uint16_t baked_tile_index = map[src_tile_id];
		if (baked_tile_index != std::numeric_limits<uint16_t>::max()) {
			// Already registered
			return baked_tile_index;
		}
	}

	// Register new tile

	ZN_ASSERT_RETURN_V(baked_tiles.size() < std::numeric_limits<uint16_t>::max(), 0);

	const Ref<VoxelBlockyTextureAtlas> &atlas = atlases[atlas_index];
	const Vector2i atlas_res = atlas->get_resolution();
	ZN_ASSERT_RETURN_V(atlas_res.x == 0 || atlas_res.y == 0, 0);

	if (src_tile_id >= map.size()) {
		map.resize(src_tile_id + 1, std::numeric_limits<uint16_t>::max());
	}
	const uint16_t baked_tile_index = baked_tiles.size();
	map[src_tile_id] = baked_tile_index;

	const VoxelBlockyTextureAtlas::Tile &src_tile = atlas->get_tile(src_tile_id);

	blocky::BakedLibrary::Tile baked_tile;
	baked_tile.type = src_tile.type;
	baked_tile.atlas_id = atlas_index;
	baked_tile.group_size_x = src_tile.group_size_x;
	baked_tile.group_size_y = src_tile.group_size_y;
	const Vector2f pixel_step_norm = Vector2f(1.f) / to_vec2f(atlas_res);
	baked_tile.atlas_uv_origin = pixel_step_norm * Vector2f(src_tile.position_x, src_tile.position_y);
	baked_tile.atlas_uv_step = pixel_step_norm * to_vec2f(atlas->get_default_tile_resolution());
	baked_tiles.push_back(baked_tile);

	return baked_tile_index;
}

} // namespace zylann::voxel::blocky