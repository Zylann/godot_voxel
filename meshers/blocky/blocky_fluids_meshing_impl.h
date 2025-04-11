// Only include this in the mesher to define it once.
// It is only in a separate header because I wanted to split things out.

// #ifndef VOXEL_BLOCKY_FLUIDS_MESHING_IMPL_H
// #define VOXEL_BLOCKY_FLUIDS_MESHING_IMPL_H

#include "../../util/containers/container_funcs.h"
#include "blocky_fluids.h"
#include "voxel_blocky_fluid.h"

namespace zylann::voxel::blocky {

void copy(const BakedFluid::Surface &src, Vector2f src_uv, BakedModel::SideSurface &dst) {
	copy(src.positions, dst.positions);

	dst.uvs.resize(src.positions.size());
	fill(dst.uvs, src_uv);

	copy(src.indices, dst.indices);
	// TODO Aren't tangents always the same on sides too? Like normals?
	copy(src.tangents, dst.tangents);
}

void copy_positions_normals_tangents(
		const BakedFluid::Surface &src,
		const Vector3f normal,
		const uint32_t p_material_id,
		const bool p_collision_enabled,
		BakedModel::Surface &dst
) {
	copy(src.positions, dst.positions);

	dst.normals.resize(src.positions.size());
	fill(dst.normals, normal);
	// std::fill(dst.normals.begin(), dst.normals.end(), normal);

	// copy(src.uvs, dst.uvs);
	copy(src.indices, dst.indices);
	copy(src.tangents, dst.tangents);

	dst.material_id = p_material_id;
	dst.collision_enabled = p_collision_enabled;
}

static const VoxelBlockyFluid::FlowState g_min_corners_mask_to_flowstate[16] = {
	// 0123
	// ----
	// 0000
	VoxelBlockyFluid::FLOW_IDLE, // Impossible
	// 0001
	VoxelBlockyFluid::FLOW_DIAGONAL_POSITIVE_X_POSITIVE_Z,
	// 0010
	VoxelBlockyFluid::FLOW_DIAGONAL_NEGATIVE_X_POSITIVE_Z,
	// 0011
	VoxelBlockyFluid::FLOW_STRAIGHT_POSITIVE_Z,
	// 0100
	VoxelBlockyFluid::FLOW_DIAGONAL_NEGATIVE_X_NEGATIVE_Z,
	// 0101
	VoxelBlockyFluid::FLOW_IDLE, // Ambiguous
	// 0110
	VoxelBlockyFluid::FLOW_STRAIGHT_NEGATIVE_X,
	// 0111
	VoxelBlockyFluid::FLOW_DIAGONAL_NEGATIVE_X_POSITIVE_Z,
	// 1000
	VoxelBlockyFluid::FLOW_DIAGONAL_POSITIVE_X_NEGATIVE_Z,
	// 1001
	VoxelBlockyFluid::FLOW_STRAIGHT_POSITIVE_X,
	// 1010
	VoxelBlockyFluid::FLOW_IDLE, // Ambiguous
	// 1011
	VoxelBlockyFluid::FLOW_DIAGONAL_POSITIVE_X_POSITIVE_Z,
	// 1100
	VoxelBlockyFluid::FLOW_STRAIGHT_NEGATIVE_Z,
	// 1101
	VoxelBlockyFluid::FLOW_DIAGONAL_POSITIVE_X_NEGATIVE_Z,
	// 1110
	VoxelBlockyFluid::FLOW_DIAGONAL_NEGATIVE_X_NEGATIVE_Z,
	// 1111
	VoxelBlockyFluid::FLOW_IDLE,
};

VoxelBlockyFluid::FlowState get_fluid_flow_state_from_corner_levels(
		//    3-------2
		//   /|      /|        z
		//  / |     / |       /
		// 0-------1     x---o
		// |       |
		const FixedArray<uint8_t, 4> corner_levels
) {
	const uint8_t min_level = math::min(corner_levels[0], corner_levels[1], corner_levels[2], corner_levels[3]);
	const uint8_t mask = //
			(static_cast<uint8_t>(corner_levels[0] == min_level) << 3) | //
			(static_cast<uint8_t>(corner_levels[1] == min_level) << 2) | //
			(static_cast<uint8_t>(corner_levels[2] == min_level) << 1) | //
			(static_cast<uint8_t>(corner_levels[3] == min_level) << 0);
	return g_min_corners_mask_to_flowstate[mask];
}

FixedArray<uint8_t, 4> get_corner_levels_from_fluid_levels(
		//  8 7 6     z
		//  5 4 3     |
		//  2 1 0  x--o
		const FixedArray<uint8_t, 9> fluid_levels
) {
	//    3-------2
	//   /|      /|        z
	//  / |     / |       /
	// 0-------1     x---o
	// |       |
	FixedArray<uint8_t, 4> corner_levels;

	corner_levels[0] = math::max(fluid_levels[1], fluid_levels[2], fluid_levels[4], fluid_levels[5]);
	corner_levels[1] = math::max(fluid_levels[0], fluid_levels[1], fluid_levels[3], fluid_levels[4]);
	corner_levels[2] = math::max(fluid_levels[3], fluid_levels[4], fluid_levels[6], fluid_levels[7]);
	corner_levels[3] = math::max(fluid_levels[4], fluid_levels[5], fluid_levels[7], fluid_levels[8]);

	return corner_levels;
}

FixedArray<float, 4> get_corner_heights_from_corner_levels(
		const FixedArray<uint8_t, 4> corner_levels,
		const BakedFluid &fluid
) {
	struct LevelToHeight {
		const float max_level_inv;

		inline float operator()(uint8_t level) const {
			return math::lerp(
					BakedFluid::BOTTOM_HEIGHT, BakedFluid::TOP_HEIGHT, static_cast<float>(level) * max_level_inv
			);
		}
	};

	// TODO Disallow fluids with only one level
	const LevelToHeight level_to_height{ 1.f / static_cast<float>(fluid.max_level) };

	FixedArray<float, 4> heights;
	heights[0] = level_to_height(corner_levels[0]);
	heights[1] = level_to_height(corner_levels[1]);
	heights[2] = level_to_height(corner_levels[2]);
	heights[3] = level_to_height(corner_levels[3]);

	return heights;
}

FixedArray<FixedArray<BakedModel::SideSurface, MAX_SURFACES>, Cube::SIDE_COUNT> &get_tls_fluid_sides_surfaces() {
	static thread_local FixedArray<FixedArray<BakedModel::SideSurface, MAX_SURFACES>, Cube::SIDE_COUNT> tls_fluid_sides;
	return tls_fluid_sides;
}

BakedModel::Surface &get_tls_fluid_top() {
	static thread_local BakedModel::Surface tls_fluid_top;
	return tls_fluid_top;
}

inline void transpose_quad_triangles(Span<int32_t> indices) {
	// Assumes triangles are like this:
	// 3---2
	// |   |  {0, 2, 1, 0, 3, 2} --> { 0, 3, 1, 1, 3, 2 }
	// 0---1
	indices[1] = indices[4];
	indices[3] = indices[2];
}

template <typename TModelID>
bool generate_fluid_model(
		const BakedModel &voxel,
		const Span<const TModelID> type_buffer,
		const int voxel_index,
		const int y_jump_size,
		const int x_jump_size,
		const int z_jump_size,
		const uint32_t visible_sides_mask,
		const BakedLibrary &library,
		Span<const BakedModel::Surface> &out_model_surfaces,
		const FixedArray<FixedArray<BakedModel::SideSurface, MAX_SURFACES>, Cube::SIDE_COUNT> *&out_model_sides_surfaces
) {
	const uint32_t top_voxel_id = type_buffer[voxel_index + y_jump_size];

	bool fluid_top_covered = false;

	if (library.has_model(top_voxel_id)) {
		const BakedModel &top_model = library.models[top_voxel_id];
		if (top_model.fluid_index == voxel.fluid_index) {
			// The top side is covered.
			if (visible_sides_mask == 0) {
				// Fast-path in cases all sides are culled (typically inside large water bodies such as
				// oceans)
				return false;
			}
			fluid_top_covered = true;
		}
	}

	const BakedFluid &fluid = library.fluids[voxel.fluid_index];

	// Fluids have only one material
	static constexpr unsigned int surface_index = 0;

	// We re-use the same memory per thread for each meshed fluid voxel
	// TODO Candidate for TempAllocator
	FixedArray<FixedArray<BakedModel::SideSurface, MAX_SURFACES>, Cube::SIDE_COUNT> &fluid_sides =
			get_tls_fluid_sides_surfaces();
	BakedModel::Surface &fluid_top_surface = get_tls_fluid_top();

	// TODO Optimize: maybe don't copy if not covered and reference instead?

	// UVs will be assigned differently than typical voxels. The shader is assumed to interpret them in order to render
	// a flowing animation. Vertex coordinates may be used as UVs instead.
	// UV.X = which axis the side is on (although it could already be deduced from normals?)
	// UV.Y = flow state (tells both direction or whether the fluid is idle)

	// Lateral sides
	// They always flow to the same direction

	copy(fluid.side_surfaces[Cube::SIDE_NEGATIVE_X],
		 Vector2f(math::AXIS_X, VoxelBlockyFluid::FLOW_STRAIGHT_POSITIVE_Z),
		 fluid_sides[Cube::SIDE_NEGATIVE_X][surface_index]);

	copy(fluid.side_surfaces[Cube::SIDE_POSITIVE_X],
		 Vector2f(math::AXIS_X, VoxelBlockyFluid::FLOW_STRAIGHT_POSITIVE_Z),
		 fluid_sides[Cube::SIDE_POSITIVE_X][surface_index]);

	copy(fluid.side_surfaces[Cube::SIDE_NEGATIVE_Z],
		 Vector2f(math::AXIS_Z, VoxelBlockyFluid::FLOW_STRAIGHT_POSITIVE_Z),
		 fluid_sides[Cube::SIDE_NEGATIVE_Z][surface_index]);

	copy(fluid.side_surfaces[Cube::SIDE_POSITIVE_Z],
		 Vector2f(math::AXIS_Z, VoxelBlockyFluid::FLOW_STRAIGHT_POSITIVE_Z),
		 fluid_sides[Cube::SIDE_POSITIVE_Z][surface_index]);

	// Bottom side
	// It is always idle

	copy(fluid.side_surfaces[Cube::SIDE_NEGATIVE_Y],
		 Vector2f(math::AXIS_Y, VoxelBlockyFluid::FLOW_IDLE),
		 fluid_sides[Cube::SIDE_NEGATIVE_Y][surface_index]);

	if (fluid_top_covered) {
		// No top side
		fluid_sides[Cube::SIDE_POSITIVE_Y][surface_index].clear();
		fluid_top_surface.clear();

	} else {
		copy_positions_normals_tangents(
				fluid.side_surfaces[Cube::SIDE_POSITIVE_Y],
				Vector3f(0.f, 1.f, 0.f),
				fluid.material_id,
				// TODO Option for collision on the fluid? Not sure if desired
				false,
				fluid_top_surface
		);

		// We'll potentially have to adjust corners of the model based on neighbor levels
		//  8 7 6     z
		//  5 4 3     |
		//  2 1 0  x--o
		FixedArray<uint8_t, 9> fluid_levels;
		uint32_t covered_neighbors = 0;
		const bool dip_when_flowing_down = fluid.dip_when_flowing_down;

		// TODO Optimize: could sample 4 neighbors first and if the max isn't the same as current level,
		// sample 4 diagonals too?
		unsigned int i = 0;
		for (int dz = -1; dz <= 1; ++dz) {
			for (int dx = -1; dx <= 1; ++dx) {
				const uint32_t nloc = voxel_index + dx * x_jump_size + dz * z_jump_size;
				const uint32_t nid = type_buffer[nloc];

				if (library.has_model(nid)) {
					const BakedModel &nm = library.models[nid];
					if (nm.fluid_index == voxel.fluid_index) {
						fluid_levels[i] = nm.fluid_level;

						// We don't test the current voxel, we know it's not covered
						if (i != 4) {
							const uint32_t anloc = nloc + y_jump_size;
							const uint32_t anid = type_buffer[anloc];
							if (anid != AIR_ID && library.has_model(anid)) {
								const BakedModel &anm = library.models[anid];
								if (anm.fluid_index == voxel.fluid_index) {
									covered_neighbors |= (1 << i);
								}
							}
						}

						if (dip_when_flowing_down) {
							// When a non-covered fluid voxel is above an area in which it can flow down, fake its level
							// to be 0 (even if it isn't really) in order to create a steep slope.
							// Do this except on max level fluids, which can "sustain" themselves. If we don't do this,
							// lakes and oceans would end up looking lower than they should (assuming their surface is
							// covered in max level fluid).
							if (nm.fluid_level != fluid.max_level && (covered_neighbors & (1 << i)) == 0) {
								const uint32_t bnloc = nloc - y_jump_size;
								const uint32_t bnid = type_buffer[bnloc];
								if (bnid == AIR_ID) {
									fluid_levels[i] = 0;
								} else if (library.has_model(bnid)) {
									const BakedModel &bnm = library.models[bnid];
									if (bnm.fluid_index == voxel.fluid_index) {
										fluid_levels[i] = 0;
									}
								}
							}
						}

					} else {
						fluid_levels[i] = 0;
					}
				} else {
					fluid_levels[i] = 0;
				}

				++i;
			}
		}

		// Adjust top corner heights to form slopes.

		const FixedArray<uint8_t, 4> corner_levels = get_corner_levels_from_fluid_levels(fluid_levels);
		const VoxelBlockyFluid::FlowState flow_state = get_fluid_flow_state_from_corner_levels(corner_levels);
		//    3-------2
		//   /|      /|        z
		//  / |     / |       /
		// 0-------1     x---o
		// |       |
		FixedArray<float, 4> corner_heights = get_corner_heights_from_corner_levels(corner_levels, fluid);

		//  8 7 6     z
		//  5 4 3     |
		//  2 1 0  x--o
		// Covered neighbors need to be considered at full height
		if ((covered_neighbors & 0b000'001'011) != 0) {
			corner_heights[1] = 1.f;
		}
		if ((covered_neighbors & 0b000'100'110) != 0) {
			corner_heights[0] = 1.f;
		}
		if ((covered_neighbors & 0b011'001'000) != 0) {
			corner_heights[2] = 1.f;
		}
		if ((covered_neighbors & 0b110'100'000) != 0) {
			corner_heights[3] = 1.f;
		}

		fluid_top_surface.uvs.resize(4);
		fill(fluid_top_surface.uvs, Vector2f(math::AXIS_Y, flow_state));

		// TODO Option to alter normals too so they are more "correct"? Not always needed tho?

		// For lateral sides, we assume top vertices are always the last 2, in
		// clockwise order relative to the top face
		{
			BakedModel::SideSurface &side_surface = fluid_sides[Cube::SIDE_NEGATIVE_X][surface_index];
			side_surface.positions[2].y = corner_heights[2];
			side_surface.positions[3].y = corner_heights[1];
		}
		{
			BakedModel::SideSurface &side_surface = fluid_sides[Cube::SIDE_POSITIVE_X][surface_index];
			side_surface.positions[2].y = corner_heights[0];
			side_surface.positions[3].y = corner_heights[3];
		}
		{
			BakedModel::SideSurface &side_surface = fluid_sides[Cube::SIDE_NEGATIVE_Z][surface_index];
			side_surface.positions[2].y = corner_heights[1];
			side_surface.positions[3].y = corner_heights[0];
		}
		{
			BakedModel::SideSurface &side_surface = fluid_sides[Cube::SIDE_POSITIVE_Z][surface_index];
			side_surface.positions[2].y = corner_heights[3];
			side_surface.positions[3].y = corner_heights[2];
		}
		// For the top side, we assume vertices are counter-clockwise, and the first is at (+x, -z)
		{
			fluid_top_surface.positions[0].y = corner_heights[0];
			fluid_top_surface.positions[1].y = corner_heights[1];
			fluid_top_surface.positions[2].y = corner_heights[2];
			fluid_top_surface.positions[3].y = corner_heights[3];
		}

		// We want the diagonal of the top quad's triangles to remain aligned with the flow
		if (flow_state == VoxelBlockyFluid::FLOW_DIAGONAL_POSITIVE_X_POSITIVE_Z ||
			flow_state == VoxelBlockyFluid::FLOW_DIAGONAL_NEGATIVE_X_NEGATIVE_Z) {
			transpose_quad_triangles(to_span(fluid_top_surface.indices));
		}
	}

	if (fluid_top_covered) {
		// Expected to be empty, but also provides material ID. Not great tho
		out_model_surfaces = to_span(voxel.model.surfaces);
	} else {
		out_model_surfaces = to_single_element_span(fluid_top_surface);
	}

	out_model_sides_surfaces = &fluid_sides;

	return true;
}

void generate_preview_fluid_model(
		const BakedModel &model,
		const uint16_t model_id,
		const BakedLibrary &library,
		Span<const BakedModel::Surface> &out_model_surfaces,
		const FixedArray<FixedArray<BakedModel::SideSurface, MAX_SURFACES>, Cube::SIDE_COUNT> *&out_model_sides_surfaces
) {
	ZN_ASSERT(model.fluid_index != NULL_FLUID_INDEX);
	FixedArray<uint16_t, 3 * 3 * 3> id_buffer;
	fill(id_buffer, AIR_ID);
	const int center_loc = Vector3iUtil::get_zxy_index(Vector3i(1, 1, 1), Vector3i(3, 3, 3));
	id_buffer[center_loc] = model_id;
	generate_fluid_model<uint16_t>(
			model,
			to_span(id_buffer),
			center_loc,
			1,
			3,
			3 * 3,
			0b111111,
			library,
			out_model_surfaces,
			out_model_sides_surfaces
	);
}

} // namespace zylann::voxel::blocky

// #endif // VOXEL_BLOCKY_FLUIDS_MESHING_IMPL_H
