#include "voxel_mesher_blocky.h"
#include "../../constants/cube_tables.h"
#include "../../storage/voxel_buffer.h"
#include "../../util/containers/span.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/packed_arrays.h"
#include "../../util/macros.h"
#include "../../util/math/conv.h"
#include "../../util/math/funcs.h"
// TODO GDX: String has no `operator+=`
#include "../../util/containers/container_funcs.h"
#include "../../util/godot/core/string.h"
#include "../../util/profiling.h"
#include "blocky_fluids_meshing_impl.h"
#include "blocky_lod_skirts.h"
#include "blocky_shadow_occluders.h"

#ifdef ZN_GODOT
#include "../../util/godot/core/class_db.h"
#endif

using namespace zylann::godot;

namespace zylann::voxel {

namespace blocky {

inline bool contributes_to_ao(const BakedLibrary &lib, uint32_t voxel_id) {
	if (voxel_id < lib.models.size()) {
		const BakedModel &t = lib.models[voxel_id];
		return t.contributes_to_ao;
	}
	return true;
}

StdVector<int> &get_tls_index_offsets() {
	static thread_local StdVector<int> tls_index_offsets;
	return tls_index_offsets;
}

template <typename Type_T>
void generate_mesh(
		StdVector<VoxelMesherBlocky::Arrays> &out_arrays_per_material,
		VoxelMesher::Output::CollisionSurface *collision_surface,
		const Span<const Type_T> type_buffer,
		const Vector3i block_size,
		const BakedLibrary &library,
		const bool bake_occlusion,
		const float baked_occlusion_darkness,
		const TintSampler tint_sampler
) {
	// TODO Optimization: not sure if this mandates a template function. There is so much more happening in this
	// function other than reading voxels, although reading is on the hottest path. It needs to be profiled. If
	// changing makes no difference, we could use a function pointer or switch inside instead to reduce executable size.

	ERR_FAIL_COND(
			block_size.x < static_cast<int>(2 * VoxelMesherBlocky::PADDING) ||
			block_size.y < static_cast<int>(2 * VoxelMesherBlocky::PADDING) ||
			block_size.z < static_cast<int>(2 * VoxelMesherBlocky::PADDING)
	);

	// Build lookup tables so to speed up voxel access.
	// These are values to add to an address in order to get given neighbor.

	const int row_size = block_size.y;
	const int deck_size = block_size.x * row_size;

	// Data must be padded, hence the off-by-one
	const Vector3i min = Vector3iUtil::create(VoxelMesherBlocky::PADDING);
	const Vector3i max = block_size - Vector3iUtil::create(VoxelMesherBlocky::PADDING);

	StdVector<int> &index_offsets = get_tls_index_offsets();
	index_offsets.clear();
	index_offsets.resize(out_arrays_per_material.size(), 0);

	int collision_surface_index_offset = 0;

	FixedArray<int, Cube::SIDE_COUNT> side_neighbor_lut;
	side_neighbor_lut[Cube::SIDE_LEFT] = row_size;
	side_neighbor_lut[Cube::SIDE_RIGHT] = -row_size;
	side_neighbor_lut[Cube::SIDE_BACK] = -deck_size;
	side_neighbor_lut[Cube::SIDE_FRONT] = deck_size;
	side_neighbor_lut[Cube::SIDE_BOTTOM] = -1;
	side_neighbor_lut[Cube::SIDE_TOP] = 1;

	FixedArray<int, Cube::EDGE_COUNT> edge_neighbor_lut;
	edge_neighbor_lut[Cube::EDGE_BOTTOM_BACK] =
			side_neighbor_lut[Cube::SIDE_BOTTOM] + side_neighbor_lut[Cube::SIDE_BACK];
	edge_neighbor_lut[Cube::EDGE_BOTTOM_FRONT] =
			side_neighbor_lut[Cube::SIDE_BOTTOM] + side_neighbor_lut[Cube::SIDE_FRONT];
	edge_neighbor_lut[Cube::EDGE_BOTTOM_LEFT] =
			side_neighbor_lut[Cube::SIDE_BOTTOM] + side_neighbor_lut[Cube::SIDE_LEFT];
	edge_neighbor_lut[Cube::EDGE_BOTTOM_RIGHT] =
			side_neighbor_lut[Cube::SIDE_BOTTOM] + side_neighbor_lut[Cube::SIDE_RIGHT];
	edge_neighbor_lut[Cube::EDGE_BACK_LEFT] = side_neighbor_lut[Cube::SIDE_BACK] + side_neighbor_lut[Cube::SIDE_LEFT];
	edge_neighbor_lut[Cube::EDGE_BACK_RIGHT] = side_neighbor_lut[Cube::SIDE_BACK] + side_neighbor_lut[Cube::SIDE_RIGHT];
	edge_neighbor_lut[Cube::EDGE_FRONT_LEFT] = side_neighbor_lut[Cube::SIDE_FRONT] + side_neighbor_lut[Cube::SIDE_LEFT];
	edge_neighbor_lut[Cube::EDGE_FRONT_RIGHT] =
			side_neighbor_lut[Cube::SIDE_FRONT] + side_neighbor_lut[Cube::SIDE_RIGHT];
	edge_neighbor_lut[Cube::EDGE_TOP_BACK] = side_neighbor_lut[Cube::SIDE_TOP] + side_neighbor_lut[Cube::SIDE_BACK];
	edge_neighbor_lut[Cube::EDGE_TOP_FRONT] = side_neighbor_lut[Cube::SIDE_TOP] + side_neighbor_lut[Cube::SIDE_FRONT];
	edge_neighbor_lut[Cube::EDGE_TOP_LEFT] = side_neighbor_lut[Cube::SIDE_TOP] + side_neighbor_lut[Cube::SIDE_LEFT];
	edge_neighbor_lut[Cube::EDGE_TOP_RIGHT] = side_neighbor_lut[Cube::SIDE_TOP] + side_neighbor_lut[Cube::SIDE_RIGHT];

	FixedArray<int, Cube::CORNER_COUNT> corner_neighbor_lut;

	corner_neighbor_lut[Cube::CORNER_BOTTOM_BACK_LEFT] = side_neighbor_lut[Cube::SIDE_BOTTOM] +
			side_neighbor_lut[Cube::SIDE_BACK] + side_neighbor_lut[Cube::SIDE_LEFT];

	corner_neighbor_lut[Cube::CORNER_BOTTOM_BACK_RIGHT] = side_neighbor_lut[Cube::SIDE_BOTTOM] +
			side_neighbor_lut[Cube::SIDE_BACK] + side_neighbor_lut[Cube::SIDE_RIGHT];

	corner_neighbor_lut[Cube::CORNER_BOTTOM_FRONT_RIGHT] = side_neighbor_lut[Cube::SIDE_BOTTOM] +
			side_neighbor_lut[Cube::SIDE_FRONT] + side_neighbor_lut[Cube::SIDE_RIGHT];

	corner_neighbor_lut[Cube::CORNER_BOTTOM_FRONT_LEFT] = side_neighbor_lut[Cube::SIDE_BOTTOM] +
			side_neighbor_lut[Cube::SIDE_FRONT] + side_neighbor_lut[Cube::SIDE_LEFT];

	corner_neighbor_lut[Cube::CORNER_TOP_BACK_LEFT] =
			side_neighbor_lut[Cube::SIDE_TOP] + side_neighbor_lut[Cube::SIDE_BACK] + side_neighbor_lut[Cube::SIDE_LEFT];

	corner_neighbor_lut[Cube::CORNER_TOP_BACK_RIGHT] = side_neighbor_lut[Cube::SIDE_TOP] +
			side_neighbor_lut[Cube::SIDE_BACK] + side_neighbor_lut[Cube::SIDE_RIGHT];

	corner_neighbor_lut[Cube::CORNER_TOP_FRONT_RIGHT] = side_neighbor_lut[Cube::SIDE_TOP] +
			side_neighbor_lut[Cube::SIDE_FRONT] + side_neighbor_lut[Cube::SIDE_RIGHT];

	corner_neighbor_lut[Cube::CORNER_TOP_FRONT_LEFT] = side_neighbor_lut[Cube::SIDE_TOP] +
			side_neighbor_lut[Cube::SIDE_FRONT] + side_neighbor_lut[Cube::SIDE_LEFT];

	// uint64_t time_prep = Time::get_singleton()->get_ticks_usec() - time_before;
	// time_before = Time::get_singleton()->get_ticks_usec();

	for (unsigned int z = min.z; z < (unsigned int)max.z; ++z) {
		for (unsigned int x = min.x; x < (unsigned int)max.x; ++x) {
			for (unsigned int y = min.y; y < (unsigned int)max.y; ++y) {
				// min and max are chosen such that you can visit 1 neighbor away from the current voxel without size
				// check

				const unsigned int voxel_index = y + x * row_size + z * deck_size;
				const unsigned int voxel_id = type_buffer[voxel_index];

				// TODO Don't assume air is 0?
				if (voxel_id == AIR_ID || !library.has_model(voxel_id)) {
					continue;
				}

				const BakedModel &voxel = library.models[voxel_id];
				const BakedModel::Model &model = voxel.model;

				// Calculate visibility of sides
				uint32_t visible_sides_mask = 0;
				for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {
					if ((model.empty_sides_mask & (1 << side)) != 0) {
						// This side is empty
						continue;
					}

					const uint32_t neighbor_voxel_id = type_buffer[voxel_index + side_neighbor_lut[side]];

					// Invalid voxels are treated like air
					if (neighbor_voxel_id < library.models.size()) {
						const BakedModel &other_vt = library.models[neighbor_voxel_id];
						if (!is_face_visible_regardless_of_shape(voxel, other_vt)) {
							// Visibility depends on the shape
							if (!is_face_visible_according_to_shape(library, voxel, other_vt, side)) {
								// Completely occluded
								continue;
							}
						}
					}

					visible_sides_mask |= (1 << side);
				}

				uint8_t model_surface_count = model.surface_count;

				Span<const BakedModel::Surface> model_surfaces = to_span(model.surfaces);

				const FixedArray<FixedArray<BakedModel::SideSurface, MAX_SURFACES>, Cube::SIDE_COUNT>
						*model_sides_surfaces = &model.sides_surfaces;

				// Hybrid approach: extract cube faces and decimate those that aren't visible,
				// and still allow voxels to have geometry that is not a cube.

				if (voxel.fluid_index != NULL_FLUID_INDEX) {
					if (!generate_fluid_model(
								voxel,
								type_buffer,
								voxel_index,
								1,
								row_size,
								deck_size,
								visible_sides_mask,
								library,
								model_surfaces,
								model_sides_surfaces
						)) {
						continue;
					}
					model_surface_count = 1;
				}

				const Color modulate_color = voxel.color * tint_sampler.evaluate(Vector3i(x, y, z));

				// Sides
				for (unsigned int side = 0; side < Cube::SIDE_COUNT; ++side) {
					if ((visible_sides_mask & (1 << side)) == 0) {
						// This side is culled
						continue;
					}

					// By default we render the whole side if we consider it visible
					const FixedArray<BakedModel::SideSurface, MAX_SURFACES> *side_surfaces =
							&((*model_sides_surfaces)[side]);

					// Might be only partially visible
					if (voxel.cutout_sides_enabled) {
						const uint32_t neighbor_voxel_id = type_buffer[voxel_index + side_neighbor_lut[side]];

						// Invalid voxels are treated like air
						if (neighbor_voxel_id < library.models.size()) {
							const BakedModel &other_vt = library.models[neighbor_voxel_id];

							const StdUnorderedMap<uint32_t, FixedArray<BakedModel::SideSurface, MAX_SURFACES>>
									&cutout_side_surfaces_by_neighbor_shape = model.cutout_side_surfaces[side];

							const unsigned int neighbor_shape_id =
									other_vt.model.side_pattern_indices[Cube::g_opposite_side[side]];

							// That's a hashmap lookup on a hot path. Cutting out sides like this should be used
							// sparsely if possible.
							// Unfortunately, use cases include certain water styles, which means oceans...
							// Eventually we should provide another approach for these
							auto it = cutout_side_surfaces_by_neighbor_shape.find(neighbor_shape_id);

							if (it != cutout_side_surfaces_by_neighbor_shape.end()) {
								// Use pre-cut side instead
								side_surfaces = &it->second;
							}
						}
					}

					// The face is visible

					int8_t shaded_corner[8] = { 0 };

					if (bake_occlusion) {
						// Combinatory solution for
						// https://0fps.net/2013/07/03/ambient-occlusion-for-minecraft-like-worlds/ (inverted)
						//	function vertexAO(side1, side2, corner) {
						//	  if(side1 && side2) {
						//		return 0
						//	  }
						//	  return 3 - (side1 + side2 + corner)
						//	}

						for (unsigned int j = 0; j < 4; ++j) {
							const unsigned int edge = Cube::g_side_edges[side][j];
							const int edge_neighbor_id = type_buffer[voxel_index + edge_neighbor_lut[edge]];
							if (contributes_to_ao(library, edge_neighbor_id)) {
								++shaded_corner[Cube::g_edge_corners[edge][0]];
								++shaded_corner[Cube::g_edge_corners[edge][1]];
							}
						}
						for (unsigned int j = 0; j < 4; ++j) {
							const unsigned int corner = Cube::g_side_corners[side][j];
							if (shaded_corner[corner] == 2) {
								shaded_corner[corner] = 3;
							} else {
								const int corner_neigbor_id = type_buffer[voxel_index + corner_neighbor_lut[corner]];
								if (contributes_to_ao(library, corner_neigbor_id)) {
									++shaded_corner[corner];
								}
							}
						}
					}

					// Subtracting 1 because the data is padded
					const Vector3f pos(x - 1, y - 1, z - 1);

					// TODO Move this into a function
					for (unsigned int surface_index = 0; surface_index < model_surface_count; ++surface_index) {
						const BakedModel::Surface &surface = model_surfaces[surface_index];

						VoxelMesherBlocky::Arrays &arrays = out_arrays_per_material[surface.material_id];

						ZN_ASSERT(surface.material_id < index_offsets.size());
						int &index_offset = index_offsets[surface.material_id];

						const BakedModel::SideSurface &side_surface = (*side_surfaces)[surface_index];

						const StdVector<Vector3f> &side_positions = side_surface.positions;
						const unsigned int vertex_count = side_surface.positions.size();

						const StdVector<Vector2f> &side_uvs = side_surface.uvs;
						const StdVector<float> &side_tangents = side_surface.tangents;

						// Append vertices of the faces in one go, don't use push_back

						{
							const int append_index = arrays.positions.size();
							arrays.positions.resize(arrays.positions.size() + vertex_count);
							Vector3f *w = arrays.positions.data() + append_index;
							for (unsigned int i = 0; i < vertex_count; ++i) {
								w[i] = side_positions[i] + pos;
							}
						}

						{
							const int append_index = arrays.uvs.size();
							arrays.uvs.resize(arrays.uvs.size() + vertex_count);
							memcpy(arrays.uvs.data() + append_index, side_uvs.data(), vertex_count * sizeof(Vector2f));
						}

						if (side_tangents.size() > 0) {
							const int append_index = arrays.tangents.size();
							arrays.tangents.resize(arrays.tangents.size() + vertex_count * 4);
							memcpy(arrays.tangents.data() + append_index,
								   side_tangents.data(),
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

							if (bake_occlusion) {
								for (unsigned int i = 0; i < vertex_count; ++i) {
									const Vector3f vertex_pos = side_positions[i];

									// General purpose occlusion colouring.
									// TODO Optimize for cubes
									// TODO Fix occlusion inconsistency caused by triangles orientation? Not sure if
									// worth it
									float shade = 0;
									for (unsigned int j = 0; j < 4; ++j) {
										unsigned int corner = Cube::g_side_corners[side][j];
										if (shaded_corner[corner] != 0) {
											float s = baked_occlusion_darkness *
													static_cast<float>(shaded_corner[corner]);
											// float k = 1.f - Cube::g_corner_position[corner].distance_to(v);
											float k = 1.f -
													math::distance_squared(Cube::g_corner_position[corner], vertex_pos);
											if (k < 0.0) {
												k = 0.0;
											}
											s *= k;
											if (s > shade) {
												shade = s;
											}
										}
									}
									const float gs = 1.0 - shade;
									w[i] = Color(gs, gs, gs) * modulate_color;
								}

							} else {
								for (unsigned int i = 0; i < vertex_count; ++i) {
									w[i] = modulate_color;
								}
							}
						}

						const StdVector<int> &side_indices = side_surface.indices;
						const unsigned int index_count = side_indices.size();

						{
							int i = arrays.indices.size();
							arrays.indices.resize(arrays.indices.size() + index_count);
							int *w = arrays.indices.data();
							for (unsigned int j = 0; j < index_count; ++j) {
								w[i++] = index_offset + side_indices[j];
							}
						}

						if (collision_surface != nullptr && surface.collision_enabled) {
							StdVector<Vector3f> &dst_positions = collision_surface->positions;
							StdVector<int> &dst_indices = collision_surface->indices;

							{
								const unsigned int append_index = dst_positions.size();
								dst_positions.resize(dst_positions.size() + vertex_count);
								Vector3f *w = dst_positions.data() + append_index;
								for (unsigned int i = 0; i < vertex_count; ++i) {
									w[i] = side_positions[i] + pos;
								}
							}

							{
								int i = dst_indices.size();
								dst_indices.resize(dst_indices.size() + index_count);
								int *w = dst_indices.data();
								for (unsigned int j = 0; j < index_count; ++j) {
									w[i++] = collision_surface_index_offset + side_indices[j];
								}
							}

							collision_surface_index_offset += vertex_count;
						}

						index_offset += vertex_count;
					}
				}

				// Inside
				for (unsigned int surface_index = 0; surface_index < model_surface_count; ++surface_index) {
					const BakedModel::Surface &surface = model_surfaces[surface_index];
					if (surface.positions.size() == 0) {
						continue;
					}
					// TODO Get rid of push_backs

					VoxelMesherBlocky::Arrays &arrays = out_arrays_per_material[surface.material_id];

					ZN_ASSERT(surface.material_id < index_offsets.size());
					int &index_offset = index_offsets[surface.material_id];

					const StdVector<Vector3f> &positions = surface.positions;
					const unsigned int vertex_count = positions.size();

					const StdVector<Vector3f> &normals = surface.normals;
					const StdVector<Vector2f> &uvs = surface.uvs;
					const StdVector<float> &tangents = surface.tangents;

					const Vector3f pos(x - 1, y - 1, z - 1);

					if (tangents.size() > 0) {
						const int append_index = arrays.tangents.size();
						arrays.tangents.resize(arrays.tangents.size() + vertex_count * 4);
						memcpy(arrays.tangents.data() + append_index,
							   tangents.data(),
							   (vertex_count * 4) * sizeof(float));
					}

					for (unsigned int i = 0; i < vertex_count; ++i) {
						arrays.normals.push_back(normals[i]);
						arrays.uvs.push_back(uvs[i]);
						arrays.positions.push_back(positions[i] + pos);
						// TODO handle ambient occlusion on inner parts
						arrays.colors.push_back(modulate_color);
					}

					const StdVector<int> &indices = surface.indices;
					const unsigned int index_count = indices.size();

					for (unsigned int i = 0; i < index_count; ++i) {
						arrays.indices.push_back(index_offset + indices[i]);
					}

					if (collision_surface != nullptr && surface.collision_enabled) {
						StdVector<Vector3f> &dst_positions = collision_surface->positions;
						StdVector<int> &dst_indices = collision_surface->indices;

						for (unsigned int i = 0; i < vertex_count; ++i) {
							dst_positions.push_back(positions[i] + pos);
						}
						for (unsigned int i = 0; i < index_count; ++i) {
							dst_indices.push_back(collision_surface_index_offset + indices[i]);
						}

						collision_surface_index_offset += vertex_count;
					}

					index_offset += vertex_count;
				}
			}
		}
	}
}

bool is_empty(const StdVector<VoxelMesherBlocky::Arrays> &arrays_per_material) {
	for (const VoxelMesherBlocky::Arrays &arrays : arrays_per_material) {
		if (arrays.indices.size() > 0) {
			return false;
		}
	}
	return true;
}

} // namespace blocky

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelMesherBlocky::VoxelMesherBlocky() {
	set_padding(PADDING, PADDING);
}

VoxelMesherBlocky::~VoxelMesherBlocky() {}

VoxelMesherBlocky::Cache &VoxelMesherBlocky::get_tls_cache() {
	thread_local Cache cache;
	return cache;
}

void VoxelMesherBlocky::set_library(Ref<VoxelBlockyLibraryBase> library) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.library = library;
}

Ref<VoxelBlockyLibraryBase> VoxelMesherBlocky::get_library() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.library;
}

void VoxelMesherBlocky::set_occlusion_darkness(float darkness) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.baked_occlusion_darkness = math::clamp(darkness, 0.0f, 1.0f);
}

float VoxelMesherBlocky::get_occlusion_darkness() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.baked_occlusion_darkness;
}

void VoxelMesherBlocky::set_occlusion_enabled(bool enable) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.bake_occlusion = enable;
}

bool VoxelMesherBlocky::get_occlusion_enabled() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.bake_occlusion;
}

void VoxelMesherBlocky::set_shadow_occluder_side(Side side, bool enabled) {
	RWLockWrite wlock(_parameters_lock);
	if (enabled) {
		_parameters.shadow_occluders_mask |= (1 << side);
	} else {
		_parameters.shadow_occluders_mask &= ~(1 << side);
	}
}

bool VoxelMesherBlocky::get_shadow_occluder_side(Side side) const {
	RWLockRead rlock(_parameters_lock);
	return (_parameters.shadow_occluders_mask & (1 << side)) != 0;
}

uint8_t VoxelMesherBlocky::get_shadow_occluder_mask() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.shadow_occluders_mask;
}

VoxelMesherBlocky::TintMode VoxelMesherBlocky::get_tint_mode() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.tint_mode;
}

void VoxelMesherBlocky::set_tint_mode(const VoxelMesherBlocky::TintMode new_mode) {
	ZN_ASSERT_RETURN(new_mode >= 0 && new_mode < TINT_MODE_COUNT);
	RWLockWrite wlock(_parameters_lock);
	_parameters.tint_mode = new_mode;
}

void VoxelMesherBlocky::build(VoxelMesher::Output &output, const VoxelMesher::Input &input) {
	const VoxelBuffer::ChannelId channel = VoxelBuffer::CHANNEL_TYPE;
	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	if (params.library.is_null()) {
		// This may be a configuration warning, the mesh will be left empty.
		// If it was an error it would spam unnecessarily in the editor as users set things up.
		return;
	}
	// ERR_FAIL_COND(params.library.is_null());

	Cache &cache = get_tls_cache();

	StdVector<Arrays> &arrays_per_material = cache.arrays_per_material;
	for (unsigned int i = 0; i < arrays_per_material.size(); ++i) {
		Arrays &a = arrays_per_material[i];
		a.clear();
	}

	float baked_occlusion_darkness = 0;
	if (params.bake_occlusion) {
		baked_occlusion_darkness = params.baked_occlusion_darkness / 3.0f;
	}

	// The technique is Culled faces.
	// Could be improved with greedy meshing: https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/
	// However I don't feel it's worth it yet:
	// - Not so much gain for organic worlds with lots of texture variations
	// - Works well with cubes but not with any shape
	// - Slower
	// => Could be implemented in a separate class?

	const VoxelBuffer &voxels = input.voxels;

	// Iterate 3D padded data to extract voxel faces.
	// This is the most intensive job in this class, so all required data should be as fit as possible.

	// The buffer we receive MUST be dense (i.e not compressed, and channels allocated).
	// That means we can use raw pointers to voxel data inside instead of using the higher-level getters,
	// and then save a lot of time.

	if (voxels.get_channel_compression(channel) == VoxelBuffer::COMPRESSION_UNIFORM) {
		// All voxels have the same type.
		// If it's all air, nothing to do. If it's all cubes, nothing to do either.
		// TODO Handle edge case of uniform block with non-cubic voxels!
		// If the type of voxel still produces geometry in this situation (which is an absurd use case but not an
		// error), decompress into a backing array to still allow the use of the same algorithm.
		return;

	} else if (voxels.get_channel_compression(channel) != VoxelBuffer::COMPRESSION_NONE) {
		// No other form of compression is allowed
		ERR_PRINT("VoxelMesherBlocky received unsupported voxel compression");
		return;
	}

	Span<const uint8_t> raw_channel;
	if (!voxels.get_channel_as_bytes_read_only(channel, raw_channel)) {
		// Case supposedly handled before...
		ERR_PRINT("Something wrong happened");
		return;
	}

	const Vector3i block_size = voxels.get_size();
	const VoxelBuffer::Depth channel_depth = voxels.get_channel_depth(channel);

	VoxelMesher::Output::CollisionSurface *collision_surface = nullptr;
	if (input.collision_hint) {
		collision_surface = &output.collision_surface;
	}

	unsigned int material_count = 0;
	{
		// We can only access baked data. Only this data is made for multithreaded access.
		RWLockRead lock(params.library->get_baked_data_rw_lock());
		const blocky::BakedLibrary &library_baked_data = params.library->get_baked_data();

		material_count = library_baked_data.indexed_materials_count;

		if (arrays_per_material.size() < material_count) {
			arrays_per_material.resize(material_count);
		}

		const blocky::TintSampler tint_sampler =
				blocky::TintSampler::create(voxels, static_cast<blocky::TintSampler::Mode>(params.tint_mode));

		switch (channel_depth) {
			case VoxelBuffer::DEPTH_8_BIT:
				blocky::generate_mesh(
						arrays_per_material,
						collision_surface,
						raw_channel,
						block_size,
						library_baked_data,
						params.bake_occlusion,
						baked_occlusion_darkness,
						tint_sampler
				);
				if (input.lod_index > 0) {
					blocky::append_skirts(
							raw_channel, block_size, arrays_per_material, library_baked_data, tint_sampler
					);
				}
				break;

			case VoxelBuffer::DEPTH_16_BIT: {
				Span<const uint16_t> model_ids = raw_channel.reinterpret_cast_to<const uint16_t>();
				blocky::generate_mesh(
						arrays_per_material,
						collision_surface,
						model_ids,
						block_size,
						library_baked_data,
						params.bake_occlusion,
						baked_occlusion_darkness,
						tint_sampler
				);
				if (input.lod_index > 0) {
					blocky::append_skirts(model_ids, block_size, arrays_per_material, library_baked_data, tint_sampler);
				}
			} break;

			default:
				ERR_PRINT("Unsupported voxel depth");
				return;
		}
	}

	if (input.lod_index > 0) {
		// Might not look good, but at least it's something
		const float lod_scale = 1 << input.lod_index;
		for (Arrays &arrays : arrays_per_material) {
			for (Vector3f &p : arrays.positions) {
				p = p * lod_scale;
			}
		}
		if (collision_surface != nullptr) {
			for (Vector3f &p : collision_surface->positions) {
				p = p * lod_scale;
			}
		}
	}

	// TODO Optimization: we could return a single byte array and use Mesh::add_surface down the line?
	// That API does not seem to exist yet though.

	for (unsigned int material_index = 0; material_index < material_count; ++material_index) {
		const Arrays &arrays = arrays_per_material[material_index];

		if (arrays.positions.size() != 0) {
			Array mesh_arrays;
			mesh_arrays.resize(Mesh::ARRAY_MAX);

			{
				PackedVector3Array positions;
				PackedVector2Array uvs;
				PackedVector3Array normals;
				PackedColorArray colors;
				PackedInt32Array indices;

				copy_to(positions, to_span_const(arrays.positions));
				copy_to(uvs, to_span_const(arrays.uvs));
				copy_to(normals, to_span_const(arrays.normals));
				copy_to(colors, to_span_const(arrays.colors));
				copy_to(indices, to_span_const(arrays.indices));

				mesh_arrays[Mesh::ARRAY_VERTEX] = positions;
				mesh_arrays[Mesh::ARRAY_TEX_UV] = uvs;
				mesh_arrays[Mesh::ARRAY_NORMAL] = normals;
				mesh_arrays[Mesh::ARRAY_COLOR] = colors;
				mesh_arrays[Mesh::ARRAY_INDEX] = indices;

				if (arrays.tangents.size() > 0) {
					PackedFloat32Array tangents;
					copy_to(tangents, to_span_const(arrays.tangents));
					mesh_arrays[Mesh::ARRAY_TANGENT] = tangents;
				}
			}

			output.surfaces.push_back(Output::Surface());
			Output::Surface &surface = output.surfaces.back();
			surface.arrays = mesh_arrays;
			surface.material_index = material_index;
		}
		//  else {
		// 	// Empty
		// 	output.surfaces.push_back(Output::Surface());
		// }
	}

	if (params.shadow_occluders_mask != 0 && !blocky::is_empty(arrays_per_material)) {
		// TODO Candidate for temp allocator (maybe even stack allocator in this case?)
		blocky::OccluderArrays occluder_arrays;

		RWLockRead lock(params.library->get_baked_data_rw_lock());
		const blocky::BakedLibrary &library_baked_data = params.library->get_baked_data();

		blocky::generate_shadow_occluders(
				occluder_arrays,
				raw_channel,
				channel_depth,
				block_size,
				library_baked_data,
				params.shadow_occluders_mask
		);

		if (input.lod_index > 0) {
			const float lod_scale = 1 << input.lod_index;
			for (Vector3f &p : occluder_arrays.vertices) {
				p *= lod_scale;
			}
		}

		if (occluder_arrays.indices.size() > 0) {
			Array mesh_arrays;
			mesh_arrays.resize(Mesh::ARRAY_MAX);

			{
				PackedVector3Array vertices;
				PackedInt32Array indices;

				copy_to(vertices, to_span_const(occluder_arrays.vertices));
				copy_to(indices, to_span_const(occluder_arrays.indices));

				mesh_arrays[Mesh::ARRAY_VERTEX] = vertices;
				mesh_arrays[Mesh::ARRAY_INDEX] = indices;
			}

			output.shadow_occluder = mesh_arrays;
			// output.surfaces.push_back(Output::Surface());
			// Output::Surface &surface = output.surfaces.back();
			// surface.arrays = mesh_arrays;
			// surface.material_index = 0;
		}
	}

	output.primitive_type = Mesh::PRIMITIVE_TRIANGLES;
}

// Ref<Resource> VoxelMesherBlocky::duplicate(bool p_subresources) const {
// 	Parameters params;
// 	{
// 		RWLockRead rlock(_parameters_lock);
// 		params = _parameters;
// 	}

// 	if (p_subresources && params.library.is_valid()) {
// 		params.library = params.library->duplicate(true);
// 	}

// 	Ref<VoxelMesherBlocky> c;
// 	c.instantiate();
// 	c->_parameters = params;
// 	return c;
// }

int VoxelMesherBlocky::get_used_channels_mask() const {
	int mask = (1 << VoxelBuffer::CHANNEL_TYPE);
	switch (get_tint_mode()) {
		case TINT_NONE:
			break;
		case TINT_RAW_COLOR:
			mask |= (1 << VoxelBuffer::CHANNEL_COLOR);
			break;
		default:
			ZN_PRINT_ERROR_ONCE("Unknown tint mode");
			break;
	}
	return mask;
}

Ref<Material> VoxelMesherBlocky::get_material_by_index(unsigned int index) const {
	Ref<VoxelBlockyLibraryBase> lib = get_library();
	if (lib.is_null()) {
		return Ref<Material>();
	}
	return lib->get_material_by_index(index);
}

unsigned int VoxelMesherBlocky::get_material_index_count() const {
	Ref<VoxelBlockyLibraryBase> lib = get_library();
	if (lib.is_null()) {
		return 0;
	}
	return lib->get_material_index_count();
}

#ifdef TOOLS_ENABLED

void VoxelMesherBlocky::get_configuration_warnings(PackedStringArray &out_warnings) const {
	Ref<VoxelBlockyLibraryBase> library = get_library();

	if (library.is_null()) {
		out_warnings.append(String(ZN_TTR("{0} has no {1} assigned."))
									.format(
											varray(VoxelMesherBlocky::get_class_static(),
												   VoxelBlockyLibraryBase::get_class_static())
									));
		return;
	}

	const blocky::BakedLibrary &baked_data = library->get_baked_data();
	RWLockRead rlock(library->get_baked_data_rw_lock());

	if (baked_data.models.size() == 0) {
		out_warnings.append(String(ZN_TTR("The {0} assigned to {1} has no baked models."))
									.format(varray(library->get_class(), VoxelMesherBlocky::get_class_static())));
		return;
	}

	library->get_configuration_warnings(out_warnings);
}

#endif // TOOLS_ENABLED

void VoxelMesherBlocky::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_library", "voxel_library"), &VoxelMesherBlocky::set_library);
	ClassDB::bind_method(D_METHOD("get_library"), &VoxelMesherBlocky::get_library);

	ClassDB::bind_method(D_METHOD("set_occlusion_enabled", "enable"), &VoxelMesherBlocky::set_occlusion_enabled);
	ClassDB::bind_method(D_METHOD("get_occlusion_enabled"), &VoxelMesherBlocky::get_occlusion_enabled);

	ClassDB::bind_method(D_METHOD("set_occlusion_darkness", "value"), &VoxelMesherBlocky::set_occlusion_darkness);
	ClassDB::bind_method(D_METHOD("get_occlusion_darkness"), &VoxelMesherBlocky::get_occlusion_darkness);

	ClassDB::bind_method(
			D_METHOD("set_shadow_occluder_side", "side", "enabled"), &VoxelMesherBlocky::set_shadow_occluder_side
	);
	ClassDB::bind_method(D_METHOD("get_shadow_occluder_side", "side"), &VoxelMesherBlocky::get_shadow_occluder_side);

	ClassDB::bind_method(D_METHOD("set_tint_mode", "mode"), &VoxelMesherBlocky::set_tint_mode);
	ClassDB::bind_method(D_METHOD("get_tint_mode"), &VoxelMesherBlocky::get_tint_mode);

	ADD_PROPERTY(
			PropertyInfo(
					Variant::OBJECT,
					"library",
					PROPERTY_HINT_RESOURCE_TYPE,
					VoxelBlockyLibraryBase::get_class_static(),
					PROPERTY_USAGE_DEFAULT
					// Sadly we can't use this hint because the property type is abstract... can't just choose a
					// default child class. This hint becomes less and less useful everytime I come across it...
					//| PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT
			),
			"set_library",
			"get_library"
	);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "occlusion_enabled"), "set_occlusion_enabled", "get_occlusion_enabled");
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "occlusion_darkness", PROPERTY_HINT_RANGE, "0,1,0.01"),
			"set_occlusion_darkness",
			"get_occlusion_darkness"
	);

	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "tint_mode", PROPERTY_HINT_ENUM, "None,RawColor"),
			"set_tint_mode",
			"get_tint_mode"
	);

	ADD_GROUP("Shadow Occluders", "shadow_occluder_");

#define ADD_SHADOW_OCCLUDER_PROPERTY(m_name, m_flag)                                                                   \
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, m_name), "set_shadow_occluder_side", "get_shadow_occluder_side", m_flag);

	ADD_SHADOW_OCCLUDER_PROPERTY("shadow_occluder_negative_x", SIDE_NEGATIVE_X);
	ADD_SHADOW_OCCLUDER_PROPERTY("shadow_occluder_positive_x", SIDE_POSITIVE_X);
	ADD_SHADOW_OCCLUDER_PROPERTY("shadow_occluder_negative_y", SIDE_NEGATIVE_Y);
	ADD_SHADOW_OCCLUDER_PROPERTY("shadow_occluder_positive_y", SIDE_POSITIVE_Y);
	ADD_SHADOW_OCCLUDER_PROPERTY("shadow_occluder_negative_z", SIDE_NEGATIVE_Z);
	ADD_SHADOW_OCCLUDER_PROPERTY("shadow_occluder_positive_z", SIDE_POSITIVE_Z);

	BIND_ENUM_CONSTANT(SIDE_NEGATIVE_X);
	BIND_ENUM_CONSTANT(SIDE_POSITIVE_X);
	BIND_ENUM_CONSTANT(SIDE_NEGATIVE_Y);
	BIND_ENUM_CONSTANT(SIDE_POSITIVE_Y);
	BIND_ENUM_CONSTANT(SIDE_NEGATIVE_Z);
	BIND_ENUM_CONSTANT(SIDE_POSITIVE_Z);

	BIND_ENUM_CONSTANT(TINT_NONE);
	BIND_ENUM_CONSTANT(TINT_RAW_COLOR);
}

} // namespace zylann::voxel
