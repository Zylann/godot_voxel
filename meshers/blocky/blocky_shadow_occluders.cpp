#include "blocky_shadow_occluders.h"
#include "../../util/math/conv.h"
#include "voxel_mesher_blocky.h"

namespace zylann::voxel::blocky {

void generate_occluders_geometry(
		OccluderArrays &out_arrays,
		const Vector3f maxf,
		bool positive_x,
		bool positive_y,
		bool positive_z,
		bool negative_x,
		bool negative_y,
		bool negative_z
) {
	const unsigned int quad_count = //
			static_cast<unsigned int>(positive_x) + //
			static_cast<unsigned int>(positive_y) + //
			static_cast<unsigned int>(positive_z) + //
			static_cast<unsigned int>(negative_x) + //
			static_cast<unsigned int>(negative_y) + //
			static_cast<unsigned int>(negative_z);

	if (quad_count == 0) {
		return;
	}

	out_arrays.vertices.resize(out_arrays.vertices.size() + 4 * quad_count);
	out_arrays.indices.resize(out_arrays.indices.size() + 6 * quad_count);

	unsigned int vi0 = 0;
	unsigned int ii0 = 0;

	if (positive_x) {
		// 3---2  y
		// |   |  |
		// 0---1  x--z

		Span<Vector3f> vertices = to_span_from_position_and_size(out_arrays.vertices, 0, 4);
		Span<int32_t> indices = to_span_from_position_and_size(out_arrays.indices, 0, 6);

		vertices[0] = Vector3f(maxf.x, 0.f, 0.f);
		vertices[1] = Vector3f(maxf.x, 0.f, maxf.z);
		vertices[2] = Vector3f(maxf.x, maxf.y, maxf.z);
		vertices[3] = Vector3f(maxf.x, maxf.y, 0.f);

		indices[0] = 0;
		indices[1] = 1;
		indices[2] = 2;
		indices[3] = 0;
		indices[4] = 2;
		indices[5] = 3;

		vi0 += 4;
		ii0 += 6;
	}

	if (positive_y) {
		// 3---2  z
		// |   |  |
		// 0---1  y--x

		Span<Vector3f> vertices = to_span_from_position_and_size(out_arrays.vertices, vi0, 4);
		Span<int32_t> indices = to_span_from_position_and_size(out_arrays.indices, ii0, 6);

		vertices[0] = Vector3f(0.f, maxf.y, 0.f);
		vertices[1] = Vector3f(maxf.x, maxf.y, 0.f);
		vertices[2] = Vector3f(maxf.x, maxf.y, maxf.z);
		vertices[3] = Vector3f(0.f, maxf.y, maxf.z);

		indices[0] = vi0 + 0;
		indices[1] = vi0 + 1;
		indices[2] = vi0 + 2;
		indices[3] = vi0 + 0;
		indices[4] = vi0 + 2;
		indices[5] = vi0 + 3;

		vi0 += 4;
		ii0 += 6;
	}

	if (positive_z) {
		// 3---2  y
		// |   |  |
		// 0---1  z--x

		Span<Vector3f> vertices = to_span_from_position_and_size(out_arrays.vertices, vi0, 4);
		Span<int32_t> indices = to_span_from_position_and_size(out_arrays.indices, ii0, 6);

		vertices[0] = Vector3f(0.f, 0.f, maxf.z);
		vertices[1] = Vector3f(maxf.x, 0.f, maxf.z);
		vertices[2] = Vector3f(maxf.x, maxf.y, maxf.z);
		vertices[3] = Vector3f(0.f, maxf.y, maxf.z);

		indices[0] = vi0 + 0;
		indices[1] = vi0 + 2;
		indices[2] = vi0 + 1;
		indices[3] = vi0 + 0;
		indices[4] = vi0 + 3;
		indices[5] = vi0 + 2;

		vi0 += 4;
		ii0 += 6;
	}

	if (negative_x) {
		// 3---2  y
		// |   |  |
		// 0---1  x--z

		Span<Vector3f> vertices = to_span_from_position_and_size(out_arrays.vertices, vi0, 4);
		Span<int32_t> indices = to_span_from_position_and_size(out_arrays.indices, ii0, 6);

		vertices[0] = Vector3f(0.f, 0.f, 0.f);
		vertices[1] = Vector3f(0.f, 0.f, maxf.z);
		vertices[2] = Vector3f(0.f, maxf.y, maxf.z);
		vertices[3] = Vector3f(0.f, maxf.y, 0.f);

		indices[0] = vi0 + 0;
		indices[1] = vi0 + 2;
		indices[2] = vi0 + 1;
		indices[3] = vi0 + 0;
		indices[4] = vi0 + 3;
		indices[5] = vi0 + 2;

		vi0 += 4;
		ii0 += 6;
	}

	if (negative_y) {
		// 3---2  z
		// |   |  |
		// 0---1  y--x

		Span<Vector3f> vertices = to_span_from_position_and_size(out_arrays.vertices, vi0, 4);
		Span<int32_t> indices = to_span_from_position_and_size(out_arrays.indices, ii0, 6);

		vertices[0] = Vector3f(0.f, 0.f, 0.f);
		vertices[1] = Vector3f(maxf.x, 0.f, 0.f);
		vertices[2] = Vector3f(maxf.x, 0.f, maxf.z);
		vertices[3] = Vector3f(0.f, 0.f, maxf.z);

		indices[0] = vi0 + 0;
		indices[1] = vi0 + 2;
		indices[2] = vi0 + 1;
		indices[3] = vi0 + 0;
		indices[4] = vi0 + 3;
		indices[5] = vi0 + 2;

		vi0 += 4;
		ii0 += 6;
	}

	if (negative_z) {
		// 3---2  y
		// |   |  |
		// 0---1  z--x

		Span<Vector3f> vertices = to_span_from_position_and_size(out_arrays.vertices, vi0, 4);
		Span<int32_t> indices = to_span_from_position_and_size(out_arrays.indices, ii0, 6);

		vertices[0] = Vector3f(0.f, 0.f, 0.f);
		vertices[1] = Vector3f(maxf.x, 0.f, 0.f);
		vertices[2] = Vector3f(maxf.x, maxf.y, 0.f);
		vertices[3] = Vector3f(0.f, maxf.y, 0.f);

		indices[0] = vi0 + 0;
		indices[1] = vi0 + 1;
		indices[2] = vi0 + 2;
		indices[3] = vi0 + 0;
		indices[4] = vi0 + 2;
		indices[5] = vi0 + 3;
	}
}

template <typename TModelID>
void classify_chunk_occlusion_from_voxels(
		Span<const TModelID> id_buffer,
		const BakedLibrary &baked_data,
		const Vector3i block_size,
		const Vector3i min,
		const Vector3i max,
		const uint8_t enabled_mask,
		bool &out_positive_x,
		bool &out_positive_y,
		bool &out_positive_z,
		bool &out_negative_x,
		bool &out_negative_y,
		bool &out_negative_z
) {
	struct L {
		static inline bool is_fully_occluded(
				const TModelID v0,
				const TModelID v1,
				const BakedLibrary &baked_data,
				const uint8_t side0_mask,
				const uint8_t side1_mask
		) {
			if (v0 >= baked_data.models.size() || v1 >= baked_data.models.size()) {
				return false;
			}
			const BakedModel &b0 = baked_data.models[v0];
			const BakedModel &b1 = baked_data.models[v1];
			if (b0.empty || b1.empty) {
				return false;
			}
			if (b0.transparency_index > 0 || b1.transparency_index > 0) {
				// Either side is transparent
				return false;
			}
			if ((b0.model.full_sides_mask & side0_mask) == 0) {
				return false;
			}
			if ((b1.model.full_sides_mask & side1_mask) == 0) {
				return false;
			}
			return true;
		}

		static bool is_chunk_side_occluded_x(
				const Vector3i min,
				const Vector3i max,
				Span<const TModelID> id_buffer,
				const Vector3i block_size,
				const int sign,
				const BakedLibrary &baked_data
		) {
			const Cube::SideAxis side0 = sign < 0 ? Cube::SIDE_NEGATIVE_X : Cube::SIDE_POSITIVE_X;
			const Cube::SideAxis side1 = sign < 0 ? Cube::SIDE_POSITIVE_X : Cube::SIDE_NEGATIVE_X;
			const uint8_t side0_mask = (1 << side0);
			const uint8_t side1_mask = (1 << side1);

			Vector3i pos(sign < 0 ? min.x : max.x - 1, 0, 0);
			for (pos.z = min.z; pos.z < max.z; ++pos.z) {
				for (pos.y = min.y; pos.y < max.y; ++pos.y) {
					const unsigned int loc0 = Vector3iUtil::get_zxy_index(pos, block_size);
					const unsigned int loc1 = Vector3iUtil::get_zxy_index(pos + Vector3i(sign, 0, 0), block_size);

					const TModelID v0 = id_buffer[loc0];
					const TModelID v1 = id_buffer[loc1];

					if (!is_fully_occluded(v0, v1, baked_data, side0_mask, side1_mask)) {
						return false;
					}
				}
			}
			return true;
		}

		static bool is_chunk_side_occluded_y(
				const Vector3i min,
				const Vector3i max,
				Span<const TModelID> id_buffer,
				const Vector3i block_size,
				const int sign,
				const BakedLibrary &baked_data
		) {
			const Cube::SideAxis side0 = sign < 0 ? Cube::SIDE_NEGATIVE_Y : Cube::SIDE_POSITIVE_Y;
			const Cube::SideAxis side1 = sign < 0 ? Cube::SIDE_POSITIVE_Y : Cube::SIDE_NEGATIVE_Y;
			const uint8_t side0_mask = (1 << side0);
			const uint8_t side1_mask = (1 << side1);

			Vector3i pos(0, sign < 0 ? min.y : max.y - 1, 0);
			for (pos.z = min.z; pos.z < max.z; ++pos.z) {
				for (pos.x = min.x; pos.x < max.x; ++pos.x) {
					const unsigned int loc0 = Vector3iUtil::get_zxy_index(pos, block_size);
					const unsigned int loc1 = Vector3iUtil::get_zxy_index(pos + Vector3i(0, sign, 0), block_size);

					const TModelID v0 = id_buffer[loc0];
					const TModelID v1 = id_buffer[loc1];

					if (!is_fully_occluded(v0, v1, baked_data, side0_mask, side1_mask)) {
						return false;
					}
				}
			}
			return true;
		}

		static bool is_chunk_side_occluded_z(
				const Vector3i min,
				const Vector3i max,
				Span<const TModelID> id_buffer,
				const Vector3i block_size,
				const int sign,
				const BakedLibrary &baked_data
		) {
			const Cube::SideAxis side0 = sign < 0 ? Cube::SIDE_NEGATIVE_Z : Cube::SIDE_POSITIVE_Z;
			const Cube::SideAxis side1 = sign < 0 ? Cube::SIDE_POSITIVE_Z : Cube::SIDE_NEGATIVE_Z;
			const uint8_t side0_mask = (1 << side0);
			const uint8_t side1_mask = (1 << side1);

			Vector3i pos(0, 0, sign < 0 ? min.z : max.z - 1);
			for (pos.x = min.x; pos.x < max.x; ++pos.x) {
				for (pos.y = min.y; pos.y < max.y; ++pos.y) {
					const unsigned int loc0 = Vector3iUtil::get_zxy_index(pos, block_size);
					const unsigned int loc1 = Vector3iUtil::get_zxy_index(pos + Vector3i(0, 0, sign), block_size);

					const TModelID v0 = id_buffer[loc0];
					const TModelID v1 = id_buffer[loc1];

					if (!is_fully_occluded(v0, v1, baked_data, side0_mask, side1_mask)) {
						return false;
					}
				}
			}
			return true;
		}
	};

	const bool positive_x_enabled = (enabled_mask & (1 << VoxelMesherBlocky::SIDE_POSITIVE_X)) != 0;
	const bool positive_y_enabled = (enabled_mask & (1 << VoxelMesherBlocky::SIDE_POSITIVE_Y)) != 0;
	const bool positive_z_enabled = (enabled_mask & (1 << VoxelMesherBlocky::SIDE_POSITIVE_Z)) != 0;

	const bool negative_x_enabled = (enabled_mask & (1 << VoxelMesherBlocky::SIDE_NEGATIVE_X)) != 0;
	const bool negative_y_enabled = (enabled_mask & (1 << VoxelMesherBlocky::SIDE_NEGATIVE_Y)) != 0;
	const bool negative_z_enabled = (enabled_mask & (1 << VoxelMesherBlocky::SIDE_NEGATIVE_Z)) != 0;

	out_positive_x = positive_x_enabled && L::is_chunk_side_occluded_x(min, max, id_buffer, block_size, 1, baked_data);
	out_positive_y = positive_y_enabled && L::is_chunk_side_occluded_y(min, max, id_buffer, block_size, 1, baked_data);
	out_positive_z = positive_z_enabled && L::is_chunk_side_occluded_z(min, max, id_buffer, block_size, 1, baked_data);

	out_negative_x = negative_x_enabled && L::is_chunk_side_occluded_x(min, max, id_buffer, block_size, -1, baked_data);
	out_negative_y = negative_y_enabled && L::is_chunk_side_occluded_y(min, max, id_buffer, block_size, -1, baked_data);
	out_negative_z = negative_z_enabled && L::is_chunk_side_occluded_z(min, max, id_buffer, block_size, -1, baked_data);
}

void generate_shadow_occluders(
		OccluderArrays &out_arrays,
		const Span<const uint8_t> id_buffer_raw,
		const VoxelBuffer::Depth depth,
		const Vector3i block_size,
		const BakedLibrary &baked_data,
		const uint8_t enabled_mask
) {
	ZN_PROFILE_SCOPE();

	// Data must be padded, hence the off-by-one
	const Vector3i min = Vector3iUtil::create(VoxelMesherBlocky::PADDING);
	const Vector3i max = block_size - Vector3iUtil::create(VoxelMesherBlocky::PADDING);

	// Not doing only positive sides, because it allows to self-contain the calculation in the chunk.
	// Doing only positive sides requires to also do this when the main mesh is actually empty, which means we would end
	// up with lots of useless occluders across all space instead of just near chunks that actually have meshes. That
	// would in turn require to lookup neighbor chunks, which introduces dependencies that complicate multi-threading
	// and sorting.

	// Rendering idea: instead of baking this into every mesh, batch all occluders into a single mesh? Then rebuild it
	// at most once per frame if there was any change? Means we have to store occlusion info somewhere that is fast
	// to access.

	bool positive_x;
	bool positive_y;
	bool positive_z;
	bool negative_x;
	bool negative_y;
	bool negative_z;

	switch (depth) {
		case VoxelBuffer::DEPTH_8_BIT:
			classify_chunk_occlusion_from_voxels(
					id_buffer_raw,
					baked_data,
					block_size,
					min,
					max,
					enabled_mask,
					positive_x,
					positive_y,
					positive_z,
					negative_x,
					negative_y,
					negative_z
			);
			break;

		case VoxelBuffer::DEPTH_16_BIT:
			classify_chunk_occlusion_from_voxels(
					id_buffer_raw.reinterpret_cast_to<const uint16_t>(),
					baked_data,
					block_size,
					min,
					max,
					enabled_mask,
					positive_x,
					positive_y,
					positive_z,
					negative_x,
					negative_y,
					negative_z
			);
			break;

		default:
			ERR_PRINT("Unsupported voxel depth");
			return;
	}

	generate_occluders_geometry(
			out_arrays, to_vec3f(max - min), positive_x, positive_y, positive_z, negative_x, negative_y, negative_z
	);
}

} // namespace zylann::voxel::blocky
