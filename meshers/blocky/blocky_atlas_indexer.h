#ifndef VOXEL_BLOCKY_ATLAS_INDEXER_H
#define VOXEL_BLOCKY_ATLAS_INDEXER_H

#include "../../util/containers/std_vector.h"
#include "blocky_baked_library.h"
#include "voxel_blocky_texture_atlas.h"

namespace zylann::voxel::blocky {

struct AtlasIndexer {
	StdVector<Ref<VoxelBlockyTextureAtlas>> atlases;
	StdVector<StdVector<uint16_t>> src_to_baked_tile_indices;
	StdVector<blocky::BakedLibrary::Tile> &baked_tiles;

	AtlasIndexer(StdVector<blocky::BakedLibrary::Tile> &p_baked_tiles) : baked_tiles(p_baked_tiles) {}

	unsigned int get_or_create_atlas_index(const Ref<VoxelBlockyTextureAtlas> &atlas);
	uint16_t get_or_create_tile_index(const unsigned int atlas_index, const unsigned int src_tile_id);
};

} // namespace zylann::voxel::blocky

#endif // VOXEL_BLOCKY_ATLAS_INDEXER_H
