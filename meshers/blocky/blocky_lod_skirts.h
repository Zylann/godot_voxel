// Only include this in the mesher to define it once.
// It is only in a separate header because I wanted to split things out.

#include "../../util/math/vector3f.h"
#include "blocky_tint_sampler.h"
#include "voxel_blocky_library_base.h"
#include "voxel_blocky_model.h"
#include "voxel_mesher_blocky.h"

namespace zylann::voxel::blocky {

Vector3f side_to_block_coordinates(const Vector3f v, const VoxelBlockyModel::Side side) {
	switch (side) {
		case VoxelBlockyModel::SIDE_NEGATIVE_X:
		case VoxelBlockyModel::SIDE_POSITIVE_X:
			return v.zyx();
		case VoxelBlockyModel::SIDE_NEGATIVE_Y:
		case VoxelBlockyModel::SIDE_POSITIVE_Y:
			// return v.zxy();
			return v.yzx();
		case VoxelBlockyModel::SIDE_NEGATIVE_Z:
		case VoxelBlockyModel::SIDE_POSITIVE_Z:
			return v;
		default:
			ZN_CRASH();
			return v;
	}
}

int get_side_sign(const VoxelBlockyModel::Side side) {
	switch (side) {
		case VoxelBlockyModel::SIDE_NEGATIVE_X:
		case VoxelBlockyModel::SIDE_NEGATIVE_Y:
		case VoxelBlockyModel::SIDE_NEGATIVE_Z:
			return -1;
		case VoxelBlockyModel::SIDE_POSITIVE_X:
		case VoxelBlockyModel::SIDE_POSITIVE_Y:
		case VoxelBlockyModel::SIDE_POSITIVE_Z:
			return 1;
		default:
			ZN_CRASH();
			return 1;
	}
}

// Adds extra voxel side geometry on the sides of the chunk for every voxel exposed to air. This creates
// "seams" that hide LOD cracks when meshes of different LOD are put next to each other.
// This method doesn't require to access voxels of the child LOD. The downside is that it won't always hide all the
// cracks, but the assumption is that it will do most of the time.
// AO is not handled, and probably doesn't need to be
template <typename TModelID>
void append_side_skirts(
		Span<const TModelID> buffer,
		const TintSampler tint_sampler,
		const Vector3T<int> jump,
		const int z, // Coordinate of the first or last voxel (not within the padded region)
		const int size_x,
		const int size_y,
		const VoxelBlockyModel::Side side,
		const BakedLibrary &library,
		StdVector<VoxelMesherBlocky::Arrays> &out_arrays_per_material
) {
	constexpr TModelID AIR = 0;
	constexpr int pad = 1;

	const int z_base = z * jump.z;
	const int side_sign = get_side_sign(side);

	// Buffers sent to chunk meshing have outer and inner voxels.
	// Inner voxels are those that are actually being meshed.
	// Outer voxels are not made part of the final mesh, but they exist to know how to occlude sides of inner voxels
	// touching them.

	// For each outer voxel on the side of the chunk (using side-relative coordinates)
	for (int x = pad; x < size_x - pad; ++x) {
		for (int y = pad; y < size_y - pad; ++y) {
			const int buffer_index = x * jump.x + y * jump.y + z_base;
			const int v = buffer[buffer_index];

			if (v == AIR) {
				continue;
			}

			// Check if the voxel is exposed to air

			const int nv0 = buffer[buffer_index - jump.x];
			const int nv1 = buffer[buffer_index + jump.x];
			const int nv2 = buffer[buffer_index - jump.y];
			const int nv3 = buffer[buffer_index + jump.y];

			if (nv0 != AIR && nv1 != AIR && nv2 != AIR && nv3 != AIR) {
				continue;
			}

			// Check if the outer voxel occludes an inner voxel
			// (this check is not actually accurate, maybe we'd have to do a full occlusion check using the library?)

			const TModelID nv4 = buffer[buffer_index - side_sign * jump.z];
			if (nv4 == AIR) {
				continue;
			}

			// If it does, add geometry for the side of that inner voxel

			const Vector3f pos = side_to_block_coordinates(Vector3f(x - pad, y - pad, z - (side_sign + 1)), side);

			if (nv4 >= library.models.size()) {
				// Bad ID, skip
				continue;
			}
			const BakedModel &voxel_baked_data = library.models[nv4];

			if (!voxel_baked_data.lod_skirts) {
				// A typical issue is making an ocean:
				// - Skirts will show up behind the water surface so it's not a good solution in that case.
				// - If sea level does not line up at different LODs, then there will be LOD "cracks" anyways. I don't
				// have a good solution for this. One way to workaround is to choose a sea level that lines up at every
				// LOD (such as Y=0), and let the seams occur in other cases which are usually way less frequent.
				// - Another way is to only reduce LOD resolution horizontally and not vertically, but that has a high
				// memory cost on large distances, so not silver bullet.
				// - Make water opaque when at large distances? If acceptable, this can be a good fix (Distant Horizons
				// mod was doing this at some point) but either require custom shader or the ability to specify
				// different models for different LODs in the library
				continue;
			}

			const BakedModel::Model &model = voxel_baked_data.model;
			const Color tint = voxel_baked_data.color * tint_sampler.evaluate(Vector3i(x, y, z));

			const FixedArray<BakedModel::SideSurface, MAX_SURFACES> &side_surfaces = model.sides_surfaces[side];

			for (unsigned int surface_index = 0; surface_index < model.surface_count; ++surface_index) {
				const BakedModel::Surface &surface = model.surfaces[surface_index];
				VoxelMesherBlocky::Arrays &arrays = out_arrays_per_material[surface.material_id];

				const BakedModel::SideSurface &side_surface = side_surfaces[surface_index];
				const unsigned int vertex_count = side_surface.positions.size();

				// TODO The following code is pretty much the same as the main meshing function.
				// We should put it in common once blocky mesher features are merged (blocky fluids, shadows occluders).
				// The baked occlusion part should be separated to run on top of color modulate.
				// Index offsets might not need a vector after all.

				const unsigned int index_offset = arrays.positions.size();

				{
					const unsigned int append_index = arrays.positions.size();
					arrays.positions.resize(arrays.positions.size() + vertex_count);
					Vector3f *w = arrays.positions.data() + append_index;
					for (unsigned int i = 0; i < vertex_count; ++i) {
						w[i] = side_surface.positions[i] + pos;
					}
				}

				{
					const unsigned int append_index = arrays.uvs.size();
					arrays.uvs.resize(arrays.uvs.size() + vertex_count);
					memcpy(arrays.uvs.data() + append_index, side_surface.uvs.data(), vertex_count * sizeof(Vector2f));
				}

				if (side_surface.tangents.size() > 0) {
					const unsigned int append_index = arrays.tangents.size();
					arrays.tangents.resize(arrays.tangents.size() + vertex_count * 4);
					memcpy(arrays.tangents.data() + append_index,
						   side_surface.tangents.data(),
						   (vertex_count * 4) * sizeof(float));
				}

				{
					const int append_index = arrays.normals.size();
					arrays.normals.resize(arrays.normals.size() + vertex_count);
					Vector3f *w = arrays.normals.data() + append_index;
					for (unsigned int i = 0; i < vertex_count; ++i) {
						w[i] = to_vec3f(Cube::g_side_normals[side]);
					}
				}

				{
					const int append_index = arrays.colors.size();
					arrays.colors.resize(arrays.colors.size() + vertex_count);
					Color *w = arrays.colors.data() + append_index;
					for (unsigned int i = 0; i < vertex_count; ++i) {
						w[i] = tint;
					}
				}

				{
					const unsigned int index_count = side_surface.indices.size();
					unsigned int i = arrays.indices.size();
					arrays.indices.resize(arrays.indices.size() + index_count);
					int *w = arrays.indices.data();
					for (unsigned int j = 0; j < index_count; ++j) {
						w[i++] = index_offset + side_surface.indices[j];
					}
				}
			}
		}
	}
}

template <typename TModelID>
void append_skirts(
		Span<const TModelID> buffer,
		const Vector3i size,
		StdVector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		const BakedLibrary &library,
		const TintSampler tint_sampler
) {
	ZN_PROFILE_SCOPE();

	const Vector3T<int> jump(size.y, 1, size.x * size.y);

	// Shortcuts
	StdVector<VoxelMesherBlocky::Arrays> &out = out_arrays_per_material;
	constexpr VoxelBlockyModel::Side NEGATIVE_X = VoxelBlockyModel::SIDE_NEGATIVE_X;
	constexpr VoxelBlockyModel::Side POSITIVE_X = VoxelBlockyModel::SIDE_POSITIVE_X;
	constexpr VoxelBlockyModel::Side NEGATIVE_Y = VoxelBlockyModel::SIDE_NEGATIVE_Y;
	constexpr VoxelBlockyModel::Side POSITIVE_Y = VoxelBlockyModel::SIDE_POSITIVE_Y;
	constexpr VoxelBlockyModel::Side NEGATIVE_Z = VoxelBlockyModel::SIDE_NEGATIVE_Z;
	constexpr VoxelBlockyModel::Side POSITIVE_Z = VoxelBlockyModel::SIDE_POSITIVE_Z;

	append_side_skirts(buffer, tint_sampler, jump.xyz(), 0, size.x, size.y, NEGATIVE_Z, library, out);
	append_side_skirts(buffer, tint_sampler, jump.xyz(), (size.z - 1), size.x, size.y, POSITIVE_Z, library, out);
	append_side_skirts(buffer, tint_sampler, jump.zyx(), 0, size.z, size.y, NEGATIVE_X, library, out);
	append_side_skirts(buffer, tint_sampler, jump.zyx(), (size.x - 1), size.z, size.y, POSITIVE_X, library, out);
	append_side_skirts(buffer, tint_sampler, jump.zxy(), 0, size.z, size.x, NEGATIVE_Y, library, out);
	append_side_skirts(buffer, tint_sampler, jump.zxy(), (size.y - 1), size.z, size.x, POSITIVE_Y, library, out);
}

} // namespace zylann::voxel::blocky
