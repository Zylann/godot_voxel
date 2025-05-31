#ifndef VOXEL_BLOCKY_BAKED_LIBRARY_H
#define VOXEL_BLOCKY_BAKED_LIBRARY_H

#include "../../constants/cube_tables.h"
#include "../../util/containers/dynamic_bitset.h"
#include "../../util/containers/fixed_array.h"
#include "../../util/containers/std_unordered_map.h"
#include "../../util/containers/std_vector.h"
#include "../../util/godot/core/aabb.h"
#include "../../util/math/color.h"
#include "../../util/math/vector2f.h"
#include "../../util/math/vector3f.h"

// This is data directly consumed by the blocky mesher.

namespace zylann::voxel::blocky {

// Limit based on maximum supported by VoxelMesherBlocky.
// Supporting more requires to double the size of voxels (32-bit), but it's a suspicious situation. Minecraft block
// states don't even reach a quarter of that limit. Needing more sounds like it's not the
// right approach.
static constexpr unsigned int MAX_MODELS = 65536;
// Limit related to how we index baked fluids
static constexpr unsigned int MAX_FLUIDS = 256;

// Materials must be kept to a minimum. 256 is already a lot, but that only affects performance. This limit is
// the one beyond which the code stops working. Could still be increased in theory (requires some code changes to
// use uint32_t instead of uint16_t), but there is no practical sense doing so.
static constexpr unsigned int MAX_MATERIALS = 65536;

// Convention to mean "nothing".
// Don't assign a non-empty model at this index.
static const uint16_t AIR_ID = 0;
static const uint8_t NULL_FLUID_INDEX = 255;
static constexpr uint32_t MAX_SURFACES = 2;

enum TileType : uint8_t {
	TILE_SINGLE,
	// Tiles are laid out as a 12x4 sheet with 47 cases of the blob tileset
	// TODO Maybe allow them to be packed in any fitting way instead?
	TILE_BLOB9,
	// Tiles are picked at random among the group of tiles. Optionally, they are randomly rotated.
	TILE_RANDOM,
	// Tiles are extended into a repeating group of tiles.
	TILE_EXTENDED,
	TILE_MAX,
};

// Plain data strictly used by the mesher.
// It becomes distinct because it's going to be used in a multithread environment,
// while the configuration that produced the data can be changed by the user at any time.
// Also, it is lighter than Godot resources.
struct BakedModel {
	struct SideSurface {
		StdVector<Vector3f> positions;
		StdVector<Vector2f> uvs;
		StdVector<int> indices;
		StdVector<float> tangents;
		// Normals aren't stored because they are assumed to be the same for the whole side

		void clear() {
			positions.clear();
			uvs.clear();
			indices.clear();
			tangents.clear();
		}
	};

	struct Surface {
		// Inside part of the model.
		StdVector<Vector3f> positions;
		StdVector<Vector3f> normals;
		StdVector<Vector2f> uvs;
		StdVector<int> indices;
		StdVector<float> tangents;

		uint32_t material_id = 0;
		bool collision_enabled = true;

		void clear() {
			positions.clear();
			normals.clear();
			uvs.clear();
			indices.clear();
			tangents.clear();
		}
	};

	struct Model {
		// A model can have up to 2 materials.
		// If more is needed or profiling tells better, we could change it to a vector?
		FixedArray<Surface, MAX_SURFACES> surfaces;
		// Model sides: they are separated because this way we can occlude them easily.
		FixedArray<FixedArray<SideSurface, MAX_SURFACES>, Cube::SIDE_COUNT> sides_surfaces;
		std::array<uint16_t, Cube::SIDE_COUNT> side_tiles;
		unsigned int surface_count = 0;
		// Cached information to check this case early.
		// Bits are indexed with the Cube::Side enum.
		uint8_t empty_sides_mask = 0;
		uint8_t full_sides_mask = 0;

		// Tells what is the "shape" of each side in order to cull them quickly when in contact with neighbors.
		// Side patterns are still determined based on a combination of all surfaces.
		FixedArray<uint32_t, Cube::SIDE_COUNT> side_pattern_indices;
		// Side culling is all or nothing.
		// If we want to support partial culling with baked models (needed if you do fluids with "staircase"
		// models), we would need another lookup table that given two side patterns, outputs alternate geometry data
		// that is pre-cut. This would require a lot more data and precomputations though, and the cases in
		// which this is needed could make use of different approaches such as procedural generation of the
		// geometry.

		// [side][neighbor_shape_id] => pre-cut SideSurfaces
		// Surface to attempt using when a side passes the visibility test and cutout is enabled.
		// If the SideSurface from this container is empty or not found, fallback on full surface
		FixedArray<StdUnorderedMap<uint32_t, FixedArray<SideSurface, MAX_SURFACES>>, Cube::SIDE_COUNT>
				cutout_side_surfaces;
		// TODO ^ Make it UniquePtr? That array takes space for what is essentially a niche feature

		void clear() {
			for (uint16_t &t : side_tiles) {
				t = std::numeric_limits<uint16_t>::max();
			}
			for (Surface &surface : surfaces) {
				surface.clear();
			}
			for (FixedArray<SideSurface, MAX_SURFACES> &side_surfaces : sides_surfaces) {
				for (SideSurface &side_surface : side_surfaces) {
					side_surface.clear();
				}
			}
		}
	};

	Model model;
	Color color;
	uint8_t transparency_index;
	bool culls_neighbors;
	bool contributes_to_ao;
	bool empty = true;
	bool is_random_tickable;
	bool is_transparent;
	bool cutout_sides_enabled = false;
	uint8_t fluid_index = NULL_FLUID_INDEX;
	uint8_t fluid_level;
	bool lod_skirts;

	uint32_t box_collision_mask;
	StdVector<AABB> box_collision_aabbs;

	inline void clear() {
		model.clear();
		empty = true;
	}
};

struct BakedFluid {
	static constexpr float TOP_HEIGHT = 0.9375f;
	static constexpr float BOTTOM_HEIGHT = 0.0625f;

	struct Surface {
		StdVector<Vector3f> positions;
		StdVector<int> indices;
		StdVector<float> tangents;
		// Normals aren't stored because they are assumed to be the same for the whole side
		// There are no UVs because they are generated for animation

		void clear() {
			positions.clear();
			indices.clear();
			tangents.clear();
		}
	};

	FixedArray<Surface, Cube::SIDE_COUNT> side_surfaces;

	// StdVector<uint16_t> level_model_indices;
	uint32_t material_id = 0;
	uint8_t max_level = 1;
	bool dip_when_flowing_down = false;
	// uint32_t box_collision_mask = 0;
};

struct BakedLibrary {
	// 2D array: { X : pattern A, Y : pattern B } => Does A occlude B
	// Where index is X + Y * pattern count
	DynamicBitset side_pattern_culling;
	unsigned int side_pattern_count = 0;
	// Lots of data can get moved but it's only on load.
	StdVector<BakedModel> models;
	StdVector<BakedFluid> fluids;
	// uint8_t default_tile_resolution = 16;

	unsigned int indexed_materials_count = 0;

	struct Tile {
		TileType type;
		uint8_t atlas_id;
		uint8_t group_size_x;
		uint8_t group_size_y;
		// For blob tiles, when true, if the neighbor voxel exists but its face is covered, it will still be considered
		// connecting.
		bool connect_to_covered = false;
		Vector2f atlas_uv_origin;
		// Size of 1 tile in the atlas in texture normalized coordinates
		Vector2f atlas_uv_step;
	};
	StdVector<Tile> tiles;

	inline bool has_model(uint32_t i) const {
		return i < models.size();
	}

	inline bool get_side_pattern_occlusion(unsigned int pattern_a, unsigned int pattern_b) const {
#ifdef DEBUG_ENABLED
		CRASH_COND(pattern_a >= side_pattern_count);
		CRASH_COND(pattern_b >= side_pattern_count);
#endif
		return side_pattern_culling.get(pattern_a + pattern_b * side_pattern_count);
	}
};

inline bool is_face_visible_regardless_of_shape(const BakedModel &vt, const BakedModel &other_vt) {
	// TODO Maybe we could get rid of `empty` here and instead set `culls_neighbors` to false during baking
	return other_vt.empty || (other_vt.transparency_index > vt.transparency_index) || !other_vt.culls_neighbors;
}

// Does not account for other factors
inline bool is_face_visible_according_to_shape(
		const BakedLibrary &lib,
		const BakedModel &vt,
		const BakedModel &other_vt,
		const int side
) {
	const unsigned int ai = vt.model.side_pattern_indices[side];
	const unsigned int bi = other_vt.model.side_pattern_indices[Cube::g_opposite_side[side]];
	// Patterns are not the same, and B does not occlude A
	return (ai != bi) && !lib.get_side_pattern_occlusion(bi, ai);
}

// Assuming `current_model` has geometry on this side
inline bool is_face_visible(
		const BakedLibrary &lib,
		const BakedModel &current_model,
		const BakedModel &neighbor_model,
		const Cube::Side side
) {
	if (is_face_visible_regardless_of_shape(current_model, neighbor_model)) {
		return true;
	}
	return is_face_visible_according_to_shape(lib, current_model, neighbor_model, side);
}

// Assuming `current_model` has geometry on this side
inline bool is_face_visible(
		const BakedLibrary &lib,
		const BakedModel &current_model,
		const uint32_t neighbor_model_id,
		const Cube::Side side
) {
	if (neighbor_model_id >= lib.models.size()) {
		return true;
	}
	const BakedModel &neighbor_model = lib.models[neighbor_model_id];
	return is_face_visible(lib, current_model, neighbor_model, side);
}

} // namespace zylann::voxel::blocky

#endif // VOXEL_BLOCKY_BAKED_LIBRARY_H
