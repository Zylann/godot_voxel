#ifndef VOXEL_BLOCKY_CONNECTED_TEXTURES_H
#define VOXEL_BLOCKY_CONNECTED_TEXTURES_H

#include "../../util/containers/span.h"
#include "../../util/godot/core/rect2i.h"
#include "../../util/godot/macros.h"

ZN_GODOT_FORWARD_DECLARE(class Image);

namespace zylann::voxel::blocky {

static constexpr unsigned int BLOB9_TILE_COUNT = 47;
// TODO Can't use Vector2i because Godot doesn't define a constexpr constructor
static constexpr unsigned int BLOB9_DEFAULT_LAYOUT_SIZE_X = 12;
static constexpr unsigned int BLOB9_DEFAULT_LAYOUT_SIZE_Y = 4;

enum Compact5Index {
	COMPACT5_POINT = 0,
	COMPACT5_HORIZONTAL = 1,
	COMPACT5_VERTICAL = 2,
	COMPACT5_CROSS = 3,
	COMPACT5_FULL = 4,
	COMPACT5_TILE_COUNT = 5
};

void generate_atlas_from_compact5(
		const Image &input_image,
		const Vector2i tile_res,
		const std::array<Vector2i, Compact5Index::COMPACT5_TILE_COUNT> &input_positions,
		const Vector2i margin,
		Image &output_image,
		const Vector2i output_position
);

std::array<uint8_t, COMPACT5_TILE_COUNT> get_blob9_reference_cases_for_compact5();

uint8_t get_connection_mask_from_case_index(const uint8_t case_index);
uint8_t get_case_index_from_connection_mask(const uint8_t cm);

template <typename TModelID>
uint8_t fetch_connection_mask(
		const Span<const TModelID> model_buffer,
		const uint32_t current_model_index,
		const uint32_t voxel_index,
		// How much to move in `model_buffer` in order to go towards +X or +Y in the tile referential (X right Y down)
		const Vector2i jump
) {
	// 0 1 2   o---x
	// 3 x 4   |
	// 5 6 7   y
	const std::array<uint32_t, 8> neighbor_indices = { voxel_index - jump.y - jump.x, voxel_index - jump.y,
													   voxel_index - jump.y + jump.x, voxel_index - jump.x,
													   voxel_index + jump.x,		  voxel_index + jump.y - jump.x,
													   voxel_index + jump.y,		  voxel_index + jump.y + jump.x };

	uint8_t connection_mask = 0;
	for (unsigned int i = 0; i < neighbor_indices.size(); ++i) {
		const uint32_t ni = neighbor_indices[i];
		const uint32_t nv = model_buffer[ni];

		// TODO If the neighbor face is culled, it should not be considered present
		// We could add 8 side checks here, but that makes things a lot more expensive.
		// It raises the question, may we calculate side culling as a previous pass over the chunk?

		// TODO We might have to check more than just the current model

		if (nv == current_model_index) {
			connection_mask |= (1 << i);
		}
	}

	return connection_mask;
}

} // namespace zylann::voxel::blocky

#endif // VOXEL_BLOCKY_CONNECTED_TEXTURES_H
