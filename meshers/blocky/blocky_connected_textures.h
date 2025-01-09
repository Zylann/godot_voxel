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

void generate_atlas_from_compact5(
		const Image &input_image,
		const Rect2i input_rect,
		Image &output_image,
		const Vector2i output_position
);

uint8_t get_connection_mask_from_case_index(const uint8_t case_index);
uint8_t get_case_index_from_connection_mask(const uint8_t cm);

template <typename TModelID>
uint8_t fetch_connection_mask(
		const Span<const TModelID> model_buffer,
		const uint32_t current_model_index,
		const uint32_t voxel_index,
		const uint32_t jump_x,
		const uint32_t jump_y
		// const uint32_t jump_z
) {
	const std::array<uint32_t, 8> neighbor_indices = { voxel_index - jump_y - jump_x, voxel_index - jump_y,
													   voxel_index - jump_y + jump_x, voxel_index - jump_x,
													   voxel_index + jump_x,		  voxel_index + jump_y - jump_x,
													   voxel_index + jump_y,		  voxel_index + jump_y + jump_x };

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
