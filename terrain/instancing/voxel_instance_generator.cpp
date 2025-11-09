#include "voxel_instance_generator.h"
#include "../../constants/voxel_string_names.h"
#include "../../generators/voxel_generator.h"
#include "../../storage/voxel_buffer.h"
#include "../../util/containers/container_funcs.h"
#include "../../util/godot/classes/array_mesh.h"
#include "../../util/godot/classes/engine.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/packed_arrays.h"
#include "../../util/godot/core/random_pcg.h"
#include "../../util/godot/core/string.h"
#include "../../util/math/conv.h"
#include "../../util/math/triangle.h"
#include "../../util/math/vector4f.h"
#include "../../util/profiling.h"
#include "../../util/string/format.h"

#if defined(_MSC_VER)
#pragma warning(disable : 4701) // Potentially uninitialized local variable used.
#endif

namespace zylann::voxel {

namespace {

// This cap is for sanity, to prevent potential crashing.
const float MAX_DENSITY = 10.f;
// We expose a slider going below max density as it should not often be needed, but we allow greater if really necessary
const char *DENSITY_HINT_STRING = "0.0, 1.0, 0.01, or_greater";

static const unsigned int GEN_SDF_SAMPLE_COUNT_MIN = 2;
static const unsigned int GEN_SDF_SAMPLE_COUNT_MAX = 16;

// Repositions points by sampling SDF from the voxel generator, and putting them closer to a position where
// the SDF crosses zero.
// Side-effects:
// - Points might appear buried or floating compared to their low-resolution mesh when seen from far away
// - Generator queries can incur a significant performance cost
// - Instance distribution may become less even in some cases
void snap_surface_points_from_generator_sdf(
		// Point positions relative to `positions_origin`, assumed to already be close to the surface
		Span<Vector3f> positions,
		// Normals along which points will be moved
		Span<const Vector3f> normals,
		const Vector3 positions_origin,
		// TODO Need to investigate whether generate_series can be made const
		VoxelGenerator &generator,
		// Distance to search below and above points along normals.
		// It should be relatively small, otherwise points could get teleported away from their expected location.
		const float search_distance,
		// How many samples are taken from the generator per point. Must be >= 2.
		const unsigned int sample_count,
		// Precalculated bounds within which input points are located. Does not have to be exact. Used for optimizing
		// generator queries.
		const Vector3f chunk_min_pos,
		const Vector3f chunk_max_pos
) {
	ZN_PROFILE_SCOPE();

	if (!generator.supports_series_generation()) {
		ZN_PRINT_ERROR_ONCE(
				format("Can't snap instance positions from generator SDF, {} doesn't support series generation.",
					   generator.get_class())
		);
		return;
	}

	ZN_ASSERT_RETURN(sample_count >= GEN_SDF_SAMPLE_COUNT_MIN);
	ZN_ASSERT_RETURN_MSG(sample_count <= GEN_SDF_SAMPLE_COUNT_MAX, "Sample count is too high");

	// TODO Candidates for temp allocator
	StdVector<float> x_buffer;
	StdVector<float> y_buffer;
	StdVector<float> z_buffer;
	StdVector<float> sd_buffer;

	const Vector3f positions_origin_f = to_vec3f(positions_origin);

	const float distance_between_samples = 2.f * search_distance / (sample_count - 1);

	{
		ZN_PROFILE_SCOPE_NAMED("Buffer preparation");

		const unsigned int buffer_len = positions.size() * sample_count;

		x_buffer.resize(buffer_len);
		y_buffer.resize(buffer_len);
		z_buffer.resize(buffer_len);
		sd_buffer.resize(buffer_len);

		const float sample_count_inv_den = 1.f / static_cast<float>(sample_count - 1);

		for (unsigned int i = 0; i < positions.size(); ++i) {
			const unsigned int k0 = i * sample_count;
			const Vector3f mid_pos_world = positions[i] + positions_origin_f;
			const Vector3f normal = normals[i];

			const Vector3f min_pos = mid_pos_world - search_distance * normal;
			const Vector3f max_pos = mid_pos_world + search_distance * normal;

			for (unsigned int j = 0; j < sample_count; ++j) {
				const unsigned int k = k0 + j;

				// Goes from 0 to 1 included
				const float t = static_cast<float>(j) * sample_count_inv_den;

				const Vector3f pos = math::lerp(min_pos, max_pos, t);

				x_buffer[k] = pos.x;
				y_buffer[k] = pos.y;
				z_buffer[k] = pos.z;
			}
		}
	}

	generator.generate_series(
			to_span(x_buffer),
			to_span(y_buffer),
			to_span(z_buffer),
			VoxelBuffer::CHANNEL_SDF,
			to_span(sd_buffer),
			chunk_min_pos,
			chunk_max_pos
	);

	// int debug_hits = 0;
	// int debug_misses = 0;

	for (unsigned int i = 0; i < positions.size(); ++i) {
		Span<const float> sd_samples = to_span_from_position_and_size(sd_buffer, i * sample_count, sample_count);

		bool s0 = sd_samples[0] >= 0.f;
		int zero_cross_index = -1;

		// Find two samples where the sign becomes positive
		for (unsigned int j = 1; j < sd_samples.size(); ++j) {
			const float sd = sd_samples[j];
			const bool s1 = sd >= 0.f;
			if (s0 != s1 && sd > 0.f) {
				zero_cross_index = j;
				break;
			}
		}

		if (zero_cross_index != -1) {
			// Found surface nearby
			const unsigned int i0 = zero_cross_index - 1;
			const unsigned int i1 = zero_cross_index;
			// Estimate where zero is between the two samples
			const float sd0 = sd_samples[i0];
			const float sd1 = sd_samples[i1];
			const float dsd = sd1 - sd0;
			float zc_alpha;
			if (Math::is_zero_approx(dsd)) {
				zc_alpha = 0.5f;
			} else {
				zc_alpha = -sd0 / dsd;
			}
			// Compute offset
			const float offset_distance_from_min = distance_between_samples * (static_cast<float>(i0) + zc_alpha);
			positions[i] = positions[i] + (offset_distance_from_min - search_distance) * normals[i];

			// ++debug_hits;
			//
		} else {
			// The instance is either buried, floating beyond the search distance, or spawned in a gap so small that
			// samples were not precise enough to find it.
			// Fallback to the smallest sample to approach it.
			float min_sd = sd_samples[0];
			unsigned int min_index = 0;
			for (unsigned int j = 1; j < sd_samples.size(); ++j) {
				const float sd = sd_samples[j];
				if (Math::abs(sd) < min_sd) {
					min_sd = sd;
					min_index = j;
				}
			}

			const float offset_distance_from_min = distance_between_samples * static_cast<float>(min_index);
			positions[i] = positions[i] + (offset_distance_from_min - search_distance) * normals[i];

			// ++debug_misses;
			// TODO Consider removing it?
		}
	}

	// ZN_PRINT_VERBOSE(format("Gen SDF snap hits {} misses {}", debug_hits, debug_misses));
}

struct TexAttrib {
	uint32_t packed_indices;
	uint32_t packed_weights;
};

inline bool vertex_contains_enough_material(
		const TexAttrib attrib,
		const unsigned int threshold,
		const uint32_t material_mask
) {
	for (unsigned int i = 0; i < 4; ++i) {
		const unsigned int vmat_weight = (attrib.packed_weights >> (i * 8)) & 0xff;
		if (vmat_weight > threshold) {
			const unsigned int vmat_index = (attrib.packed_indices >> (i * 8)) & 0xff;
			if (((1 << vmat_index) & material_mask) != 0) {
				return true;
			}
		}
	}
	return false;
}

inline float u8_to_unorm(uint32_t u) {
	return static_cast<float>(u) / 255.f;
}

inline float csum(const Vector4f v) {
	return v.x + v.y + v.z + v.w;
}

inline Vector4f linear_normalize(const Vector4f v) {
	const float sum = math::max(csum(v), 0.0001f);
	return v * (1.f / sum);
}

inline Vector4f unpack_weights(const uint32_t packed_weights) {
	Vector4f weights;
	for (unsigned int i = 0; i < 4; ++i) {
		weights[i] = u8_to_unorm((packed_weights >> (i * 8)) & 0xff);
	}
	return weights;
}

inline bool triangle_contains_enough_material_interpolated(
		const Span<const TexAttrib> attrib_array,
		const Span<const int32_t> mesh_indices,
		const Span<const float> barycentrics,
		const unsigned int instance_index,
		const unsigned int ii0,
		const float threshold,
		const uint32_t material_mask
) {
	const uint32_t vi0 = mesh_indices[ii0 + 0];
	const uint32_t vi1 = mesh_indices[ii0 + 1];
	const uint32_t vi2 = mesh_indices[ii0 + 2];

	const uint32_t packed_indices = attrib_array[vi0].packed_indices;

	const uint32_t packed_weights0 = attrib_array[vi0].packed_weights;
	const uint32_t packed_weights1 = attrib_array[vi1].packed_weights;
	const uint32_t packed_weights2 = attrib_array[vi2].packed_weights;

#ifdef DEV_ENABLED
	{
		// We assume each vertex actually has the same indices, it's a property of the mesh for
		// interpolation to make sense
		ZN_ASSERT(
				attrib_array[vi0].packed_indices == attrib_array[vi1].packed_indices &&
				attrib_array[vi1].packed_indices == attrib_array[vi2].packed_indices
		);
	}
#endif

	// Early out
	const bool found_any = vertex_contains_enough_material({ packed_indices, packed_weights0 }, 0, material_mask) ||
			vertex_contains_enough_material({ packed_indices, packed_weights1 }, 0, material_mask) ||
			vertex_contains_enough_material({ packed_indices, packed_weights2 }, 0, material_mask);
	if (!found_any) {
		return false;
	}

	// Unpack, normalize and compare

	const Vector4f rweights0 = unpack_weights(packed_weights0);
	const Vector4f rweights1 = unpack_weights(packed_weights1);
	const Vector4f rweights2 = unpack_weights(packed_weights2);

	const Vector4f weights0 = linear_normalize(rweights0);
	const Vector4f weights1 = linear_normalize(rweights1);
	const Vector4f weights2 = linear_normalize(rweights2);

	const Vector3f bary(
			barycentrics[instance_index * 3 + 0],
			barycentrics[instance_index * 3 + 1],
			barycentrics[instance_index * 3 + 2]
	);

	for (unsigned int i = 0; i < 4; ++i) {
		const uint32_t vmat_index = (packed_indices >> (i * 8)) & 0xff;
		if (((1 << vmat_index) & material_mask) != 0) {
			const float weight = math::interpolate_triangle(weights0[i], weights1[i], weights2[i], bary);
			if (weight >= threshold) {
				return true;
			}
		}
	}

	return false;
}

// Filter out by voxel materials
// Assuming 4x8-bit weights and 4x8-bit indices as used in VoxelMesherTransvoxel for now, but might have other
// formats in the future
void filter_instances_by_voxel_materials(
		StdVector<Vector3f> &instance_positions,
		StdVector<Vector3f> &instance_normals,
		// Barycentric coordinates that were used to position the instance in the triangle.
		// Not used in vertex emission mode.
		StdVector<float> &instance_barycentrics,
		// In vertex emission mode, index in the mesh vertex array.
		// In other modes, index of the first triangle index in the mesh's index array.
		StdVector<uint32_t> &instance_indices,

		Span<const int32_t> mesh_indices,
		Span<const TexAttrib> mesh_tex_attrib_array,
		const float weight_threshold,
		const uint32_t material_mask,
		const VoxelInstanceGenerator::EmitMode emit_mode
) {
	ZN_PROFILE_SCOPE();

	switch (emit_mode) {
		case VoxelInstanceGenerator::EMIT_FROM_VERTICES: {
			const unsigned int weight_threshold_i =
					math::clamp(static_cast<unsigned int>(weight_threshold * 255.f), 0u, 255u);
			// Indices are vertices
			for (unsigned int instance_index = 0; instance_index < instance_positions.size();) {
				const unsigned int vi = instance_indices[instance_index];
				const TexAttrib attrib = mesh_tex_attrib_array[vi];
				if (vertex_contains_enough_material(attrib, weight_threshold_i, material_mask)) {
					instance_index += 1;
				} else {
					// Remove instance
					unordered_remove(instance_positions, instance_index);
					unordered_remove(instance_normals, instance_index);
					unordered_remove(instance_indices, instance_index);
				}
			}
		} break;

		case VoxelInstanceGenerator::EMIT_FROM_FACES:
		case VoxelInstanceGenerator::EMIT_FROM_FACES_FAST:
		case VoxelInstanceGenerator::EMIT_ONE_PER_TRIANGLE: {
#ifdef DEV_ENABLED
			ZN_ASSERT(instance_barycentrics.size() / 3 == instance_positions.size());
#endif

			// Indices are the index in the index buffer of the first vertex of the triangle in which the instance
			// was spawned in
			const Span<const float> barycentrics_s = to_span(instance_barycentrics);
			for (unsigned int instance_index = 0; instance_index < instance_positions.size();) {
				const uint32_t ii0 = instance_indices[instance_index];
				// if (L::triangle_contains_enough_material_rough(
				if (triangle_contains_enough_material_interpolated(
							mesh_tex_attrib_array,
							mesh_indices,
							barycentrics_s,
							instance_index,
							ii0,
							weight_threshold,
							material_mask
					)) {
					instance_index += 1;
				} else {
					// Remove instance
					unordered_remove(instance_positions, instance_index);
					unordered_remove(instance_normals, instance_index);
					unordered_remove(instance_indices, instance_index);

					unordered_remove(instance_barycentrics, instance_index * 3 + 2);
					unordered_remove(instance_barycentrics, instance_index * 3 + 1);
					unordered_remove(instance_barycentrics, instance_index * 3 + 0);
				}
			}
		} break;

		default:
			ZN_PRINT_ERROR_ONCE("Unhandled emit mode");
			break;
	}
}

void generate_random_points_from_vertices(
		Span<const Vector3> mesh_vertices,
		Span<const Vector3> mesh_normals,
		const float input_density,
		// The mesh vertices are assumed to be within (0,0,0) and (block_size, block_size, block_size)
		const float block_size,
		RandomPCG &pcg,
		StdVector<Vector3f> &out_positions,
		StdVector<Vector3f> &out_normals,
		StdVector<uint32_t> *out_indices
) {
	// Density is interpreted differently here,
	// so it's possible a different emit mode will produce different amounts of instances.
	// I had to use `uint64` and clamp it because floats can't contain `0xffffffff` accurately. Instead
	// it results in `0x100000000`, one unit above.
	const float density = math::clamp(input_density, 0.f, 1.f);
	static constexpr float max_density = 1.f;
	const uint32_t density_u32 = math::min(uint64_t(double(0xffffffff) * density / max_density), uint64_t(0xffffffff));
	const int size = mesh_vertices.size();
	const float margin = block_size - block_size * 0.01f;
	for (int i = 0; i < size; ++i) {
		// TODO We could actually generate indexes and pick those,
		// rather than iterating them all and rejecting
		if (pcg.rand() >= density_u32) {
			continue;
		}
		// Ignore vertices located on the positive faces of the block. They are usually shared with the
		// neighbor block, which causes a density bias and overlapping instances
		const Vector3f pos = to_vec3f(mesh_vertices[i]);
		if (pos.x > margin || pos.y > margin || pos.z > margin) {
			continue;
		}
		out_positions.push_back(pos);
		out_normals.push_back(to_vec3f(mesh_normals[i]));
		if (out_indices != nullptr) {
			out_indices->push_back(i);
		}
	}
}

void generate_random_points_from_triangles_fast(
		Span<const Vector3> mesh_vertices,
		Span<const Vector3> mesh_normals,
		Span<const int32_t> mesh_indices,
		const float density,
		RandomPCG &pcg0,
		RandomPCG &pcg1,
		StdVector<Vector3f> &out_positions,
		StdVector<Vector3f> &out_normals,
		StdVector<uint32_t> *out_indices,
		StdVector<float> *out_barycentrics
) {
	const int triangle_count = mesh_indices.size() / 3;

	// Assumes triangles are all roughly under the same size, and Transvoxel ones do (when not simplified),
	// so we can use number of triangles as a metric proportional to the number of instances
	const int instance_count = density * triangle_count;

	out_positions.resize(instance_count);
	out_normals.resize(instance_count);

	for (int instance_index = 0; instance_index < instance_count; ++instance_index) {
		// Pick a random triangle
		const uint32_t ii = (pcg0.rand() % triangle_count) * 3;

		const int ia = mesh_indices[ii];
		const int ib = mesh_indices[ii + 1];
		const int ic = mesh_indices[ii + 2];

		const Vector3f pa = to_vec3f(mesh_vertices[ia]);
		const Vector3f pb = to_vec3f(mesh_vertices[ib]);
		const Vector3f pc = to_vec3f(mesh_vertices[ic]);

		const Vector3f na = to_vec3f(mesh_normals[ia]);
		const Vector3f nb = to_vec3f(mesh_normals[ib]);
		const Vector3f nc = to_vec3f(mesh_normals[ic]);

		const float t0 = pcg1.randf();
		const float t1 = pcg1.randf();

		// This formula gives pretty uniform distribution but involves a square root
		// const Vector3 p = pa.linear_interpolate(pb, t0).linear_interpolate(pc, 1.f - sqrt(t1));

		// This is an approximation
		// const Vector3 p = pa.lerp(pb, t0).lerp(pc, t1);
		// const Vector3 n = na.lerp(nb, t0).lerp(nc, t1);

		const Vector3f bary = math::get_triangle_random_barycentric(t0, t1);
		const Vector3f p = math::interpolate_triangle(pa, pb, pc, bary);
		const Vector3f n = math::interpolate_triangle(na, nb, nc, bary);

		out_positions[instance_index] = to_vec3f(p);
		out_normals[instance_index] = to_vec3f(n);

		if (out_indices != nullptr) {
			out_indices->push_back(ii);
		}

		if (out_barycentrics != nullptr) {
			out_barycentrics->push_back(bary.x);
			out_barycentrics->push_back(bary.y);
			out_barycentrics->push_back(bary.z);
		}
	}
}

void generate_random_points_from_triangles(
		Span<const Vector3> mesh_vertices,
		Span<const Vector3> mesh_normals,
		Span<const int32_t> mesh_indices,
		const float density,
		const float triangle_area_threshold,
		RandomPCG &pcg,
		StdVector<Vector3f> &out_positions,
		StdVector<Vector3f> &out_normals,
		StdVector<uint32_t> *out_indices,
		StdVector<float> *out_barycentrics
) {
	// PackedInt32Array::Read indices_r = indices.read();

	const int triangle_count = mesh_indices.size() / 3;

	// static thread_local StdVector<float> g_area_cache;
	// StdVector<float> &area_cache = g_area_cache;
	// area_cache.resize(triangle_count);

	// Does not assume triangles have the same size, so instead a "unit size" is used,
	// and more instances will be placed in triangles larger than this.
	// This is roughly the size of one voxel's triangle
	// const float unit_area = 0.5f * squared(block_size / 32.f);

	float area_accumulator = 0.f;
	// Here density means "instances per space unit squared".
	// So inverse density means "units squared per instance"
	const float inv_density = 1.f / density;

	for (int triangle_index = 0; triangle_index < triangle_count; ++triangle_index) {
		const uint32_t ii = triangle_index * 3;

		const int ia = mesh_indices[ii];
		const int ib = mesh_indices[ii + 1];
		const int ic = mesh_indices[ii + 2];

		const Vector3f &pa = to_vec3f(mesh_vertices[ia]);
		const Vector3f &pb = to_vec3f(mesh_vertices[ib]);
		const Vector3f &pc = to_vec3f(mesh_vertices[ic]);

		const float triangle_area = math::get_triangle_area(pa, pb, pc);
		if (triangle_area <= triangle_area_threshold) {
			continue;
		}

		const Vector3f &na = to_vec3f(mesh_normals[ia]);
		const Vector3f &nb = to_vec3f(mesh_normals[ib]);
		const Vector3f &nc = to_vec3f(mesh_normals[ic]);

		area_accumulator += triangle_area;

		const int count_in_triangle = int(area_accumulator * density);

		for (int i = 0; i < count_in_triangle; ++i) {
			const float t0 = pcg.randf();
			const float t1 = pcg.randf();

			// This formula gives pretty uniform distribution but involves a square root
			// const Vector3 p = pa.linear_interpolate(pb, t0).linear_interpolate(pc, 1.f - sqrt(t1));

			// This is an approximation
			// const Vector3f rp = math::lerp(math::lerp(pa, pb, t0), pc, t1);
			// const Vector3f rn = math::lerp(math::lerp(na, nb, t0), nc, t1);

			const Vector3f bary = math::get_triangle_random_barycentric(t0, t1);
			const Vector3f p = math::interpolate_triangle(pa, pb, pc, bary);
			const Vector3f n = math::interpolate_triangle(na, nb, nc, bary);

			out_positions.push_back(p);
			out_normals.push_back(n);

			if (out_indices != nullptr) {
				out_indices->push_back(ii);
			}

			if (out_barycentrics != nullptr) {
				out_barycentrics->push_back(bary.x);
				out_barycentrics->push_back(bary.y);
				out_barycentrics->push_back(bary.z);
			}
		}

		area_accumulator -= count_in_triangle * inv_density;
	}
}

void generate_one_random_point_per_triangle(
		Span<const Vector3> mesh_vertices,
		Span<const Vector3> mesh_normals,
		Span<const int32_t> mesh_indices,
		const float triangle_area_threshold,
		const float jitter,
		RandomPCG &pcg,
		StdVector<Vector3f> &out_positions,
		StdVector<Vector3f> &out_normals,
		StdVector<uint32_t> *out_indices,
		StdVector<float> *out_barycentrics
) {
	const int triangle_count = mesh_indices.size() / 3;
	const float one_third = 1.f / 3.f;

	out_positions.reserve(triangle_count);
	out_normals.reserve(triangle_count);

	for (int triangle_index = 0; triangle_index < triangle_count; ++triangle_index) {
		const uint32_t ii = triangle_index * 3;

		const int ia = mesh_indices[ii];
		const int ib = mesh_indices[ii + 1];
		const int ic = mesh_indices[ii + 2];

		const Vector3f &pa = to_vec3f(mesh_vertices[ia]);
		const Vector3f &pb = to_vec3f(mesh_vertices[ib]);
		const Vector3f &pc = to_vec3f(mesh_vertices[ic]);

		if (triangle_area_threshold > 0.f) {
			const float triangle_area = math::get_triangle_area(pa, pb, pc);
			if (triangle_area < triangle_area_threshold) {
				continue;
			}
		}

		const Vector3f &na = to_vec3f(mesh_normals[ia]);
		const Vector3f &nb = to_vec3f(mesh_normals[ib]);
		const Vector3f &nc = to_vec3f(mesh_normals[ic]);

		const Vector3f cp = (pa + pb + pc) * one_third;
		const Vector3f cn = (na + nb + nc) * one_third;

		if (jitter == 0.f) {
			out_positions.push_back(to_vec3f(cp));
			out_normals.push_back(to_vec3f(cn));

			if (out_barycentrics != nullptr) {
				out_barycentrics->push_back(one_third);
				out_barycentrics->push_back(one_third);
				out_barycentrics->push_back(one_third);
			}

		} else {
			const float t0 = pcg.randf();
			const float t1 = pcg.randf();

			// This formula gives pretty uniform distribution but involves a square root
			// const Vector3 p = pa.linear_interpolate(pb, t0).linear_interpolate(pc, 1.f - sqrt(t1));

			// This is an approximation
			// const Vector3f rp = math::lerp(math::lerp(pa, pb, t0), pc, t1);
			// const Vector3f rn = math::lerp(math::lerp(na, nb, t0), nc, t1);

			const Vector3f rbary = math::get_triangle_random_barycentric(t0, t1);
			const Vector3f bary = math::lerp(Vector3f(one_third), rbary, jitter);
			const Vector3f p = math::interpolate_triangle(pa, pb, pc, bary);
			const Vector3f n = math::interpolate_triangle(na, nb, nc, bary);

			out_positions.push_back(p);
			out_normals.push_back(n);

			if (out_barycentrics != nullptr) {
				out_barycentrics->push_back(bary.x);
				out_barycentrics->push_back(bary.y);
				out_barycentrics->push_back(bary.z);
			}
		}

		if (out_indices != nullptr) {
			out_indices->push_back(ii);
		}
	}
}

void generate_noise_at_positions_with_graph(
		StdVector<Vector3f> &instance_positions,
		const Vector3 mesh_block_origin_d,
		// TODO Should be const, investigate if it can be fixed
		pg::VoxelGraphFunction &noise_graph,
		const VoxelInstanceGenerator::Dimension noise_dimension,
		StdVector<float> &out_noise
) {
	out_noise.resize(instance_positions.size());

	// Check noise graph validity
	std::shared_ptr<pg::VoxelGraphFunction::CompiledGraph> compiled_graph = noise_graph.get_compiled_graph();
	if (compiled_graph != nullptr) {
		const int input_count = compiled_graph->runtime.get_input_count();
		const int output_count = compiled_graph->runtime.get_output_count();

		bool valid = (output_count == 1);

		switch (noise_dimension) {
			case VoxelInstanceGenerator::DIMENSION_2D:
				if (input_count != 2) {
					valid = false;
				}
				break;
			case VoxelInstanceGenerator::DIMENSION_3D:
				if (input_count != 3) {
					valid = false;
				}
				break;
			default:
				ERR_FAIL();
		}

		if (!valid) {
			compiled_graph = nullptr;
		}
	}

	if (compiled_graph != nullptr) {
		// Execute graph

		// TODO Candidates for temp allocator
		static thread_local StdVector<float> g_noise_graph_x_cache;
		static thread_local StdVector<float> g_noise_graph_y_cache;
		static thread_local StdVector<float> g_noise_graph_z_cache;

		StdVector<float> &x_buffer = g_noise_graph_x_cache;
		StdVector<float> &z_buffer = g_noise_graph_z_cache;
		x_buffer.resize(instance_positions.size());
		z_buffer.resize(instance_positions.size());

		FixedArray<Span<float>, 1> outputs;
		outputs[0] = to_span(out_noise);

		switch (noise_dimension) {
			case VoxelInstanceGenerator::DIMENSION_2D: {
				for (size_t i = 0; i < instance_positions.size(); ++i) {
					const Vector3 &pos = to_vec3(instance_positions[i]) + mesh_block_origin_d;
					x_buffer[i] = pos.x;
					z_buffer[i] = pos.z;
				}

				FixedArray<Span<const float>, 2> inputs;
				inputs[0] = to_span(x_buffer);
				inputs[1] = to_span(z_buffer);

				noise_graph.execute(to_span(inputs), to_span(outputs));
			} break;

			case VoxelInstanceGenerator::DIMENSION_3D: {
				StdVector<float> &y_buffer = g_noise_graph_y_cache;
				y_buffer.resize(instance_positions.size());

				for (size_t i = 0; i < instance_positions.size(); ++i) {
					const Vector3 &pos = to_vec3(instance_positions[i]) + mesh_block_origin_d;
					x_buffer[i] = pos.x;
					y_buffer[i] = pos.y;
					z_buffer[i] = pos.z;
				}

				FixedArray<Span<const float>, 3> inputs;
				inputs[0] = to_span(x_buffer);
				inputs[1] = to_span(y_buffer);
				inputs[2] = to_span(z_buffer);

				noise_graph.execute(to_span(inputs), to_span(outputs));
			} break;

			default:
				ERR_FAIL();
		}

	} else {
		// Error fallback
		for (float &v : out_noise) {
			v = 0.f;
		}
	}
}

struct MeshData {
	Span<const Vector3> vertices;
	Span<const Vector3> normals;
	Span<const TexAttrib> texture_data;
	Span<const int32_t> indices;

	inline bool is_empty() const {
		return indices.size() == 0;
	}
};

MeshData parse_arrays(
		const Array &surface_arrays,
		const int32_t vertex_range_end,
		const int32_t index_range_end,
		const bool use_tex_attribs
) {
	if (vertex_range_end == 0) {
		return {};
	}
	if (index_range_end == 0) {
		return {};
	}

	if (surface_arrays.size() < ArrayMesh::ARRAY_VERTEX && surface_arrays.size() < ArrayMesh::ARRAY_NORMAL &&
		surface_arrays.size() < ArrayMesh::ARRAY_INDEX) {
		return {};
	}

	const PackedVector3Array vertices_pa = surface_arrays[ArrayMesh::ARRAY_VERTEX];
	if (vertices_pa.size() == 0) {
		return {};
	}

	Span<const Vector3> vertices = to_span(vertices_pa);
	if (vertex_range_end > 0) {
		vertices = vertices.sub(0, vertex_range_end);
	}

	const PackedVector3Array normals_pa = surface_arrays[ArrayMesh::ARRAY_NORMAL];
	ERR_FAIL_COND_V(normals_pa.size() == 0, {});
	Span<const Vector3> normals = to_span(normals_pa);
	if (vertex_range_end > 0) {
		normals = normals.sub(0, vertex_range_end);
	}

	Span<const TexAttrib> tex_attribs;
	if (use_tex_attribs && surface_arrays.size() >= Mesh::ARRAY_CUSTOM1) {
		const PackedFloat32Array src_vertex_data = surface_arrays[Mesh::ARRAY_CUSTOM1];
		tex_attribs = to_span(src_vertex_data).reinterpret_cast_to<const TexAttrib>();
		if (vertex_range_end > 0) {
			tex_attribs = tex_attribs.sub(0, vertex_range_end);
		}
	}

	const PackedInt32Array mesh_indices_pa = surface_arrays[ArrayMesh::ARRAY_INDEX];
	ERR_FAIL_COND_V(mesh_indices_pa.size() == 0, {});
	ERR_FAIL_COND_V(mesh_indices_pa.size() % 3 != 0, {});
	Span<const int32_t> mesh_indices = to_span(mesh_indices_pa);
	if (index_range_end > 0) {
		mesh_indices = mesh_indices.sub(0, index_range_end);
	}

	return { vertices, normals, tex_attribs, mesh_indices };
}

void filter_instances_by_octant(
		StdVector<Vector3f> &instance_positions,
		StdVector<Vector3f> &instance_normals,
		StdVector<uint32_t> *instance_indices,
		StdVector<float> *instance_barycentrics,
		const float block_size,
		const uint8_t octant_mask
) {
	ZN_PROFILE_SCOPE();
	const float h = block_size / 2.f;
	for (unsigned int i = 0; i < instance_positions.size();) {
		const Vector3f &pos = instance_positions[i];
		const uint8_t octant_index = VoxelInstanceGenerator::get_octant_index(pos, h);
		if ((octant_mask & (1 << octant_index)) == 0) {
			unordered_remove(instance_positions, i);
			unordered_remove(instance_normals, i);
			if (instance_indices != nullptr) {
				unordered_remove(*instance_indices, i);
			}
			if (instance_barycentrics != nullptr) {
				unordered_remove(*instance_barycentrics, i * 3 + 2);
				unordered_remove(*instance_barycentrics, i * 3 + 1);
				unordered_remove(*instance_barycentrics, i * 3 + 0);
			}
		} else {
			++i;
		}
	}
}

struct LinearFalloffRange {
	float min0;
	float min1;
	float max0;
	float max1;
	float min_falloff;
	float max_falloff;

	LinearFalloffRange(const float p_min, const float p_max, const float p_min_falloff, const float p_max_falloff) {
		min0 = p_min;
		min1 = p_min + p_min_falloff;
		max0 = p_max - p_max_falloff;
		max1 = p_max;
		min_falloff = p_min_falloff;
		max_falloff = p_max_falloff;
	}

	inline bool should_discard(const float v, RandomPCG &rng) const {
		if (v < min0 || v > max1) {
			return true;
		}
		if (v < min1) {
			const float d = (v - min0) / min_falloff;
			const float n = rng.randf();
			// We use the square because it gives a better perceived gradient over a surface, as the notion of
			// `density` for a surface would be proportional to `points/meters^2`
			return n > d * d;
		} else if (v > max0) {
			const float d = (max1 - v) / max_falloff;
			const float n = rng.randf();
			return n > d * d;
		}
		return false;
	}
};

struct AngularFalloffRange {
	float min0_rad;
	// float min1_rad;
	// float max0_rad;
	float max1_rad;

	float min0_cosine;
	float min1_cosine;
	float max0_cosine;
	float max1_cosine;

	float min_falloff_rad;
	float max_falloff_rad;

	AngularFalloffRange(
			const float min_degrees,
			const float max_degrees,
			const float min_falloff_degrees,
			const float max_falloff_degrees
	) {
		min0_rad = math::deg_to_rad(min_degrees);
		const float min1_rad = math::deg_to_rad(min_degrees + min_falloff_degrees);
		const float max0_rad = math::deg_to_rad(max_degrees - max_falloff_degrees);
		max1_rad = math::deg_to_rad(max_degrees);

		// Order is reversed because `cos` is decreasing in the 0..180 degree range
		min0_cosine = math::cos(max1_rad);
		min1_cosine = math::cos(max0_rad);
		max0_cosine = math::cos(min1_rad);
		max1_cosine = math::cos(min0_rad);

		min_falloff_rad = math::deg_to_rad(min_falloff_degrees);
		max_falloff_rad = math::deg_to_rad(max_falloff_degrees);
	}

	inline bool should_discard_cosine(const float cosine, RandomPCG &rng) const {
		if (cosine < min0_cosine || cosine > max1_cosine) {
			return true;
		}
		if (cosine < min1_cosine) {
			const float angle = Math::acos(math::clamp(cosine, 0.f, 1.f));
			const float d = (max1_rad - angle) / max_falloff_rad;
			const float n = rng.randf();
			// We use the square because it gives a better perceived gradient over a surface, as the notion of
			// `density` for a surface would be proportional to `points/meters^2`
			return n > d * d;
		}
		if (cosine > max0_cosine) {
			const float angle = Math::acos(math::clamp(cosine, 0.f, 1.f));
			const float d = (angle - min0_rad) / min_falloff_rad;
			const float n = rng.randf();
			return n > d * d;
		}
		return false;
	}
};

} // namespace

void VoxelInstanceGenerator::generate_transforms(
		StdVector<Transform3f> &out_transforms,
		const Vector3i grid_position,
		// TODO `lod_index` has become unused, remove?
		const int lod_index,
		const int layer_id,
		// TODO Provide arrays or offset to ignore transition meshes (transvoxel)
		Array surface_arrays,
		const int32_t vertex_range_end,
		const int32_t index_range_end,
		const UpMode up_mode,
		const uint8_t octant_mask,
		const float block_size,
		Ref<VoxelGenerator> voxel_generator
) {
	ZN_PROFILE_SCOPE();

	if (_density <= 0.f) {
		return;
	}

	const MeshData mesh =
			parse_arrays(surface_arrays, vertex_range_end, index_range_end, _voxel_material_filter_enabled);
	if (mesh.is_empty()) {
		return;
	}

	const uint32_t block_pos_hash = Vector3iHasher::hash(grid_position);

	Vector3f global_up(0.f, 1.f, 0.f);

	// Using different number generators so changing parameters affecting one doesn't affect the other
	const uint64_t seed = block_pos_hash + layer_id;
	RandomPCG pcg0;
	pcg0.seed(seed);
	RandomPCG pcg1;
	pcg1.seed(seed + 1);

	out_transforms.clear();

	// TODO Candidates for temp allocator
	static thread_local StdVector<Vector3f> g_vertex_cache;
	static thread_local StdVector<Vector3f> g_normal_cache;
	static thread_local StdVector<uint32_t> g_index_cache;
	static thread_local StdVector<float> g_noise_cache;
	// static thread_local StdVector<float> g_noise_graph_output_cache;

	static thread_local StdVector<float> g_barycentrics_cache;

	StdVector<Vector3f> &vertex_cache = g_vertex_cache;
	StdVector<Vector3f> &normal_cache = g_normal_cache;
	StdVector<uint32_t> &index_cache = g_index_cache;

	StdVector<float> &barycentrics = g_barycentrics_cache;

	vertex_cache.clear();
	normal_cache.clear();
	index_cache.clear();
	barycentrics.clear();

	const bool voxel_material_filter_enabled = _voxel_material_filter_enabled;
	const uint32_t voxel_material_filter_mask = _voxel_material_filter_mask;

	const bool index_cache_used = voxel_material_filter_enabled;
	const bool barycentrics_used = voxel_material_filter_enabled && _emit_mode != EMIT_FROM_VERTICES;

	// Do an early check to see if there is any material that we can potentially find
	if (voxel_material_filter_enabled) {
		ZN_PROFILE_SCOPE_NAMED("material filter mesh-wide early check");

		bool found_any = false;
		for (const TexAttrib &attrib : mesh.texture_data) {
			if (vertex_contains_enough_material(attrib, 0, voxel_material_filter_mask)) {
				found_any = true;
				break;
			}
		}
		if (!found_any) {
			return;
		}
	}

	// Pick random points
	// Generate base positions
	switch (_emit_mode) {
		case EMIT_FROM_VERTICES:
			generate_random_points_from_vertices(
					mesh.vertices,
					mesh.normals,
					_density,
					block_size,
					pcg0,
					vertex_cache,
					normal_cache,
					index_cache_used ? &index_cache : nullptr
			);
			break;

		case EMIT_FROM_FACES_FAST:
			generate_random_points_from_triangles_fast(
					mesh.vertices,
					mesh.normals,
					mesh.indices,
					_density,
					pcg0,
					pcg1,
					vertex_cache,
					normal_cache,
					index_cache_used ? &index_cache : nullptr,
					barycentrics_used ? &barycentrics : nullptr
			);
			break;

		case EMIT_FROM_FACES:
			generate_random_points_from_triangles(
					mesh.vertices,
					mesh.normals,
					mesh.indices,
					_density,
					math::squared(1 << lod_index) * _triangle_area_threshold_lod0,
					pcg1,
					vertex_cache,
					normal_cache,
					index_cache_used ? &index_cache : nullptr,
					barycentrics_used ? &barycentrics : nullptr
			);
			break;

		case EMIT_ONE_PER_TRIANGLE:
			// Density has no effect here.
			generate_one_random_point_per_triangle(
					mesh.vertices,
					mesh.normals,
					mesh.indices,
					math::squared(1 << lod_index) * _triangle_area_threshold_lod0,
					_jitter,
					pcg1,
					vertex_cache,
					normal_cache,
					index_cache_used ? &index_cache : nullptr,
					barycentrics_used ? &barycentrics : nullptr
			);
			break;

		default:
			ZN_CRASH();
	}

#ifdef DEV_ENABLED
	if (barycentrics_used) {
		ZN_ASSERT((barycentrics.size() % 3) == 0);
		ZN_ASSERT(barycentrics.size() / 3 == vertex_cache.size());
	}
#endif

	// Filter out by octants
	// This is done so some octants can be filled with user-edited data instead,
	// because mesh size may not necessarily match data block size
	if ((octant_mask & 0xff) != 0xff) {
		filter_instances_by_octant(
				vertex_cache,
				normal_cache,
				index_cache_used ? &index_cache : nullptr,
				barycentrics_used ? &barycentrics : nullptr,
				block_size,
				octant_mask
		);
	}

	if (voxel_material_filter_enabled) {
		filter_instances_by_voxel_materials(
				vertex_cache,
				normal_cache,
				barycentrics,
				index_cache,
				mesh.indices,
				mesh.texture_data,
				_voxel_material_filter_threshold,
				_voxel_material_filter_mask,
				_emit_mode
		);

		// Index cache has no use yet after this. To detect future mistakes if any, make it obvious by clearing it
		index_cache.clear();
	}

	// Position of the block relative to the instancer node.
	// Use full-precision here because we deal with potentially large coordinates
	const Vector3 mesh_block_origin_d = grid_position * block_size;

	// Don't directly access member vars because they can be modified by the editor thread (the resources themselves can
	// get modified with relatively no harm, but the pointers can't)
	Ref<pg::VoxelGraphFunction> noise_graph;
	Ref<Noise> noise;
	{
		ShortLockScope slock(_ptr_settings_lock);
		noise = _noise;
		noise_graph = _noise_graph;
	}

	StdVector<float> &noise_cache = g_noise_cache;

	// Filter out by noise graph
	if (noise_graph.is_valid()) {
		generate_noise_at_positions_with_graph(
				vertex_cache, mesh_block_origin_d, **noise_graph, _noise_dimension, noise_cache
		);
	}

	// Legacy noise (noise graph is more versatile, but this remains for compatibility)
	if (noise.is_valid()) {
		noise_cache.resize(vertex_cache.size());

		switch (_noise_dimension) {
			case DIMENSION_2D: {
				if (noise_graph.is_valid()) {
					// Multiply output of noise graph
					for (size_t i = 0; i < vertex_cache.size(); ++i) {
						const Vector3 &pos = to_vec3(vertex_cache[i]) + mesh_block_origin_d;
						// Casting to float because Noise returns `real_t`, which is `double` in 64-bit float builds,
						// but we don't need doubles for noise in this context...
						noise_cache[i] *= math::max(float(noise->get_noise_2d(pos.x, pos.z)), 0.f);
					}
				} else {
					// Use noise directly
					for (size_t i = 0; i < vertex_cache.size(); ++i) {
						const Vector3 &pos = to_vec3(vertex_cache[i]) + mesh_block_origin_d;
						noise_cache[i] = noise->get_noise_2d(pos.x, pos.z);
					}
				}
			} break;

			case DIMENSION_3D: {
				if (noise_graph.is_valid()) {
					for (size_t i = 0; i < vertex_cache.size(); ++i) {
						const Vector3 &pos = to_vec3(vertex_cache[i]) + mesh_block_origin_d;
						noise_cache[i] *= math::max(float(noise->get_noise_3d(pos.x, pos.y, pos.z)), 0.f);
					}
				} else {
					for (size_t i = 0; i < vertex_cache.size(); ++i) {
						const Vector3 &pos = to_vec3(vertex_cache[i]) + mesh_block_origin_d;
						noise_cache[i] = noise->get_noise_3d(pos.x, pos.y, pos.z);
					}
				}
			} break;

			default:
				ERR_FAIL();
		}
	}

	const bool use_noise = noise.is_valid() || noise_graph.is_valid();

	// Filter out by noise
	if (use_noise) {
		ZN_PROFILE_SCOPE_NAMED("Noise filter");

		const float falloff = _noise_falloff;

		const float threshold = _noise_threshold;
		if (threshold != 0.f) {
			for (float &n : noise_cache) {
				n += threshold;
			}
		}

		if (falloff <= 0.f) {
			for (size_t i = 0; i < vertex_cache.size();) {
				const float n = noise_cache[i];
				if (n <= 0.f) {
					unordered_remove(vertex_cache, i);
					unordered_remove(normal_cache, i);
					unordered_remove(noise_cache, i);
					// We don't use the index cache after this... for now
					// if (index_cache_used) {
					// 	unordered_remove(index_cache, i);
					// }
				} else {
					++i;
				}
			}
		} else {
			for (size_t i = 0; i < vertex_cache.size();) {
				const float n = noise_cache[i];
				const float r = pcg1.randf();
				const float d = n / falloff;
				if (d < 0.f || r > d * d) {
					unordered_remove(vertex_cache, i);
					unordered_remove(normal_cache, i);
					unordered_remove(noise_cache, i);
					// We don't use the index cache after this... for now
					// if (index_cache_used) {
					// 	unordered_remove(index_cache, i);
					// }
				} else {
					++i;
				}
			}
		}
	}

	// snap from generator SDF
	if (_gen_sdf_snap_settings.enabled && voxel_generator.is_valid()) {
		const Vector3f min_pos = to_vec3f(mesh_block_origin_d);
		const Vector3f max_pos = min_pos + Vector3f(block_size);

		snap_surface_points_from_generator_sdf(
				to_span(vertex_cache),
				to_span(normal_cache),
				mesh_block_origin_d,
				**voxel_generator,
				_gen_sdf_snap_settings.search_distance,
				_gen_sdf_snap_settings.sample_count,
				min_pos,
				max_pos
		);
	}

	const float vertical_alignment = _vertical_alignment;
	const float scale_min = _min_scale;
	const float scale_range = _max_scale - _min_scale;
	const bool random_vertical_flip = _random_vertical_flip;
	const float offset_along_normal = _offset_along_normal;

	const bool slope_filter_active = _min_slope_degrees != 0.f || _max_slope_degrees != 180.f;
	const AngularFalloffRange slope_filter(
			_min_slope_degrees, _max_slope_degrees, _min_slope_falloff_degrees, _max_slope_falloff_degrees
	);

	const bool height_filter_active =
			_min_height != std::numeric_limits<float>::min() || _max_height != std::numeric_limits<float>::max();
	const LinearFalloffRange height_filter(_min_height, _max_height, _min_height_falloff, _max_height_falloff);

	const Vector3f fixed_look_axis = up_mode == UP_MODE_POSITIVE_Y ? Vector3f(1, 0, 0) : Vector3f(0, 1, 0);
	const Vector3f fixed_look_axis_alternative = up_mode == UP_MODE_POSITIVE_Y ? Vector3f(0, 1, 0) : Vector3f(1, 0, 0);
	const Vector3f mesh_block_origin = to_vec3f(grid_position * block_size);

	// Calculate orientations and scales
	for (size_t vertex_index = 0; vertex_index < vertex_cache.size(); ++vertex_index) {
		Transform3f t;
		t.origin = vertex_cache[vertex_index];

		// Warning: sometimes mesh normals are not perfectly normalized.
		// The cause is for meshing speed on CPU. It's normalized on GPU anyways.
		Vector3f surface_normal = normal_cache[vertex_index];

		Vector3f axis_y;

		bool surface_normal_is_normalized = false;
		bool sphere_up_is_computed = false;
		bool sphere_distance_is_computed = false;
		float sphere_distance;

		if (vertical_alignment == 0.f) {
			surface_normal = math::normalized(surface_normal);
			surface_normal_is_normalized = true;
			axis_y = surface_normal;

		} else {
			if (up_mode == UP_MODE_SPHERE) {
				global_up = math::normalized(mesh_block_origin + t.origin, sphere_distance);
				sphere_up_is_computed = true;
				sphere_distance_is_computed = true;
			}

			if (vertical_alignment < 1.f) {
				axis_y = math::normalized(math::lerp(surface_normal, global_up, vertical_alignment));

			} else {
				axis_y = global_up;
			}
		}

		if (slope_filter_active) {
			if (!surface_normal_is_normalized) {
				surface_normal = math::normalized(surface_normal);
			}

			// If the normal points straight up, it will be 1, and angle is considered to be 0. Then angle increases as
			// ground gets sloped or goes upside down, up to 180 degrees
			float ny = surface_normal.y;
			if (up_mode == UP_MODE_SPHERE) {
				if (!sphere_up_is_computed) {
					global_up = math::normalized(mesh_block_origin + t.origin, sphere_distance);
					sphere_up_is_computed = true;
					sphere_distance_is_computed = true;
				}
				ny = math::dot(surface_normal, global_up);
			}

			if (slope_filter.should_discard_cosine(ny, pcg1)) {
				continue;
			}
		}

		if (height_filter_active) {
			float y = mesh_block_origin.y + t.origin.y;
			if (up_mode == UP_MODE_SPHERE) {
				if (!sphere_distance_is_computed) {
					sphere_distance = math::length(mesh_block_origin + t.origin);
					sphere_distance_is_computed = true;
				}
				y = sphere_distance;
			}

			if (height_filter.should_discard(y, pcg1)) {
				continue;
			}
		}

		t.origin += offset_along_normal * axis_y;

		// Allows to use two faces of a single rock to create variety in the same layer
		if (random_vertical_flip && (pcg1.rand() & 1) == 1) {
			axis_y = -axis_y;
			// TODO Should have to flip another axis as well?
		}

		// Pick a random rotation from the floor's normal.
		// We may check for cases too close to Y to avoid broken basis due to float precision limits,
		// even if that could differ from the expected result
		Vector3f dir;
		if (_random_rotation) {
			do {
				// TODO Optimization: a pool of precomputed random directions would do the job too? Or would it waste
				// the cache?
				dir = math::normalized(Vector3f(pcg1.randf() - 0.5f, pcg1.randf() - 0.5f, pcg1.randf() - 0.5f));
				// TODO Any way to check if the two vectors are close to aligned without normalizing `dir`?
			} while (Math::abs(math::dot(dir, axis_y)) > 0.9999f);

		} else {
			// If the surface is aligned with this axis, it will create a "pole" where all instances are looking at.
			// When getting too close to it, we may pick a different axis.
			dir = fixed_look_axis;
			if (Math::abs(math::dot(dir, axis_y)) > 0.9999f) {
				dir = fixed_look_axis_alternative;
			}
		}

		const Vector3f axis_x = math::normalized(math::cross(axis_y, dir));
		const Vector3f axis_z = math::cross(axis_x, axis_y);

		// In Godot 3, the Basis constructor expected 3 rows, but in Godot 4 it was changed to take 3 columns...
		// t.basis = Basis3f(Vector3f(axis_x.x, axis_y.x, axis_z.x), Vector3f(axis_x.y, axis_y.y, axis_z.y),
		// 		Vector3f(axis_x.z, axis_y.z, axis_z.z));
		t.basis = Basis3f(axis_x, axis_y, axis_z);

		if (scale_range > 0.f) {
			float r = pcg1.randf();

			switch (_scale_distribution) {
				case DISTRIBUTION_QUADRATIC:
					r = r * r;
					break;
				case DISTRIBUTION_CUBIC:
					r = r * r * r;
					break;
				case DISTRIBUTION_QUINTIC:
					r = r * r * r * r * r;
					break;
				default:
					break;
			}

			if (use_noise && _noise_on_scale > 0.f) {
#ifdef DEBUG_ENABLED
				CRASH_COND(vertex_index >= noise_cache.size());
#endif
				// Multiplied noise because it gives more pronounced results
				const float n = math::clamp(noise_cache[vertex_index] * 2.f, 0.f, 1.f);
				r *= Math::lerp(1.f, n, _noise_on_scale);
			}

			const float scale = scale_min + scale_range * r;

			t.basis.scale(scale);

		} else if (scale_min != 1.f) {
			t.basis.scale(scale_min);
		}

		out_transforms.push_back(t);
	}

	// TODO Investigate if this helps (won't help with authored terrain)
	// if (graph_generator.is_valid()) {
	// 	for (size_t i = 0; i < _transform_cache.size(); ++i) {
	// 		Transform &t = _transform_cache[i];
	// 		const Vector3 up = t.get_basis().get_axis(Vector3::AXIS_Y);
	// 		t.origin = graph_generator->approximate_surface(t.origin, up * 0.5f);
	// 	}
	// }
}

void VoxelInstanceGenerator::set_density(float density) {
	density = math::clamp(density, 0.f, MAX_DENSITY);
	if (density == _density) {
		return;
	}
	_density = density;
	emit_changed();
}

float VoxelInstanceGenerator::get_density() const {
	return _density;
}

void VoxelInstanceGenerator::set_emit_mode(EmitMode mode) {
	ERR_FAIL_INDEX(mode, EMIT_MODE_COUNT);
	if (_emit_mode == mode) {
		return;
	}
	_emit_mode = mode;
	emit_changed();
	notify_property_list_changed();
}

VoxelInstanceGenerator::EmitMode VoxelInstanceGenerator::get_emit_mode() const {
	return _emit_mode;
}

void VoxelInstanceGenerator::set_jitter(const float p_jitter) {
	const float jitter = math::clamp(p_jitter, 0.f, 1.f);
	if (jitter == _jitter) {
		return;
	}
	_jitter = jitter;
	emit_changed();
}

float VoxelInstanceGenerator::get_jitter() const {
	return _jitter;
}

void VoxelInstanceGenerator::set_triangle_area_threshold(const float p_threshold) {
	const float threshold = math::max(p_threshold, 0.f);
	if (threshold == _triangle_area_threshold_lod0) {
		return;
	}
	_triangle_area_threshold_lod0 = threshold;
	emit_changed();
}

float VoxelInstanceGenerator::get_triangle_area_threshold() const {
	return _triangle_area_threshold_lod0;
}

void VoxelInstanceGenerator::set_min_scale(float min_scale) {
	if (_min_scale == min_scale) {
		return;
	}
	_min_scale = min_scale;
	emit_changed();
}

float VoxelInstanceGenerator::get_min_scale() const {
	return _min_scale;
}

void VoxelInstanceGenerator::set_max_scale(float max_scale) {
	if (max_scale == _max_scale) {
		return;
	}
	_max_scale = max_scale;
	emit_changed();
}

float VoxelInstanceGenerator::get_max_scale() const {
	return _max_scale;
}

void VoxelInstanceGenerator::set_scale_distribution(Distribution distribution) {
	ERR_FAIL_INDEX(distribution, DISTRIBUTION_COUNT);
	if (distribution == _scale_distribution) {
		return;
	}
	_scale_distribution = distribution;
	emit_changed();
}

VoxelInstanceGenerator::Distribution VoxelInstanceGenerator::get_scale_distribution() const {
	return _scale_distribution;
}

void VoxelInstanceGenerator::set_vertical_alignment(float amount) {
	amount = math::clamp(amount, 0.f, 1.f);
	if (_vertical_alignment == amount) {
		return;
	}
	_vertical_alignment = amount;
	emit_changed();
}

float VoxelInstanceGenerator::get_vertical_alignment() const {
	return _vertical_alignment;
}

void VoxelInstanceGenerator::set_offset_along_normal(float offset) {
	if (_offset_along_normal == offset) {
		return;
	}
	_offset_along_normal = offset;
	emit_changed();
}

float VoxelInstanceGenerator::get_offset_along_normal() const {
	return _offset_along_normal;
}

void VoxelInstanceGenerator::set_min_slope_degrees(float p_degrees) {
	const float degrees = math::clamp(p_degrees, 0.f, 180.f);
	if (degrees == _min_slope_degrees) {
		return;
	}
	_min_slope_degrees = degrees;
	emit_changed();
}

float VoxelInstanceGenerator::get_min_slope_degrees() const {
	return _min_slope_degrees;
}

void VoxelInstanceGenerator::set_max_slope_degrees(float p_degrees) {
	const float degrees = math::clamp(p_degrees, 0.f, 180.f);
	if (degrees == _max_slope_degrees) {
		return;
	}
	_max_slope_degrees = degrees;
	emit_changed();
}

float VoxelInstanceGenerator::get_max_slope_degrees() const {
	return _max_slope_degrees;
}

void VoxelInstanceGenerator::set_min_slope_falloff_degrees(float p_degrees) {
	const float degrees = math::clamp(p_degrees, 0.f, 180.f);
	if (degrees == _min_slope_falloff_degrees) {
		return;
	}
	_min_slope_falloff_degrees = degrees;
	emit_changed();
}

float VoxelInstanceGenerator::get_min_slope_falloff_degrees() const {
	return _min_slope_falloff_degrees;
}

void VoxelInstanceGenerator::set_max_slope_falloff_degrees(float p_degrees) {
	const float degrees = math::clamp(p_degrees, 0.f, 180.f);
	if (degrees == _max_slope_falloff_degrees) {
		return;
	}
	_max_slope_falloff_degrees = degrees;
	emit_changed();
}

float VoxelInstanceGenerator::get_max_slope_falloff_degrees() const {
	return _max_slope_falloff_degrees;
}

void VoxelInstanceGenerator::set_min_height(float h) {
	if (h == _min_height) {
		return;
	}
	_min_height = h;
	emit_changed();
}

float VoxelInstanceGenerator::get_min_height() const {
	return _min_height;
}

void VoxelInstanceGenerator::set_max_height(float h) {
	if (_max_height == h) {
		return;
	}
	_max_height = h;
	emit_changed();
}

float VoxelInstanceGenerator::get_max_height() const {
	return _max_height;
}

void VoxelInstanceGenerator::set_min_height_falloff(float f) {
	f = math::max(f, 0.f);
	if (f == _min_height_falloff) {
		return;
	}
	_min_height_falloff = f;
	emit_changed();
}

float VoxelInstanceGenerator::get_min_height_falloff() const {
	return _min_height_falloff;
}

void VoxelInstanceGenerator::set_max_height_falloff(float f) {
	f = math::max(f, 0.f);
	if (_max_height_falloff == f) {
		return;
	}
	_max_height_falloff = f;
	emit_changed();
}

float VoxelInstanceGenerator::get_max_height_falloff() const {
	return _max_height_falloff;
}

void VoxelInstanceGenerator::set_random_vertical_flip(bool flip_enabled) {
	if (flip_enabled == _random_vertical_flip) {
		return;
	}
	_random_vertical_flip = flip_enabled;
	emit_changed();
}

bool VoxelInstanceGenerator::get_random_vertical_flip() const {
	return _random_vertical_flip;
}

void VoxelInstanceGenerator::set_random_rotation(bool enabled) {
	if (enabled != _random_rotation) {
		_random_rotation = enabled;
		emit_changed();
	}
}

bool VoxelInstanceGenerator::get_random_rotation() const {
	return _random_rotation;
}

void VoxelInstanceGenerator::set_noise(Ref<Noise> noise) {
	{
		ShortLockScope slock(_ptr_settings_lock);

		if (_noise == noise) {
			return;
		}
		if (_noise.is_valid()) {
			_noise->disconnect(
					VoxelStringNames::get_singleton().changed,
					callable_mp(this, &VoxelInstanceGenerator::_on_noise_changed)
			);
		}
		_noise = noise;
		if (_noise.is_valid()) {
			_noise->connect(
					VoxelStringNames::get_singleton().changed,
					callable_mp(this, &VoxelInstanceGenerator::_on_noise_changed)
			);
		}
	}
	// Emit signal outside of the locked region to avoid eventual deadlocks if handlers want to access the property
	emit_changed();
	notify_property_list_changed();
}

Ref<Noise> VoxelInstanceGenerator::get_noise() const {
	ShortLockScope slock(_ptr_settings_lock);
	return _noise;
}

void VoxelInstanceGenerator::set_noise_graph(Ref<pg::VoxelGraphFunction> func) {
	{
		ShortLockScope slock(_ptr_settings_lock);

		if (_noise_graph == func) {
			return;
		}
		if (_noise_graph.is_valid()) {
			_noise_graph->disconnect(
					VoxelStringNames::get_singleton().changed,
					callable_mp(this, &VoxelInstanceGenerator::_on_noise_graph_changed)
			);
			_noise_graph->disconnect(
					VoxelStringNames::get_singleton().compiled,
					callable_mp(this, &VoxelInstanceGenerator::_on_noise_graph_changed)
			);
		}

		_noise_graph = func;

		if (_noise_graph.is_valid()) {
			// Compile on assignment because there isn't really a good place to do it...
			func->compile(Engine::get_singleton()->is_editor_hint());

			_noise_graph->connect(
					VoxelStringNames::get_singleton().changed,
					callable_mp(this, &VoxelInstanceGenerator::_on_noise_graph_changed)
			);
			_noise_graph->connect(
					VoxelStringNames::get_singleton().compiled,
					callable_mp(this, &VoxelInstanceGenerator::_on_noise_graph_changed)
			);
		}
	}
	// Emit signal outside of the locked region to avoid eventual deadlocks if handlers want to access the property
	emit_changed();
	notify_property_list_changed();
}

Ref<pg::VoxelGraphFunction> VoxelInstanceGenerator::get_noise_graph() const {
	ShortLockScope slock(_ptr_settings_lock);
	return _noise_graph;
}

void VoxelInstanceGenerator::set_noise_dimension(Dimension dim) {
	ERR_FAIL_INDEX(dim, DIMENSION_COUNT);
	if (dim == _noise_dimension) {
		return;
	}
	_noise_dimension = dim;
	emit_changed();
}

VoxelInstanceGenerator::Dimension VoxelInstanceGenerator::get_noise_dimension() const {
	return _noise_dimension;
}

void VoxelInstanceGenerator::set_noise_falloff(const float p_falloff) {
	const float falloff = math::max(p_falloff, 0.f);
	if (falloff == _noise_falloff) {
		return;
	}
	_noise_falloff = falloff;
	emit_changed();
}

float VoxelInstanceGenerator::get_noise_falloff() const {
	return _noise_falloff;
}

void VoxelInstanceGenerator::set_noise_threshold(const float threshold) {
	if (threshold == _noise_threshold) {
		return;
	}
	_noise_threshold = threshold;
	emit_changed();
}

float VoxelInstanceGenerator::get_noise_threshold() const {
	return _noise_threshold;
}

void VoxelInstanceGenerator::set_noise_on_scale(float amount) {
	amount = math::clamp(amount, 0.f, 1.f);
	if (amount == _noise_on_scale) {
		return;
	}
	_noise_on_scale = amount;
	emit_changed();
}

float VoxelInstanceGenerator::get_noise_on_scale() const {
	return _noise_on_scale;
}

void VoxelInstanceGenerator::set_voxel_material_filter_enabled(bool enabled) {
	if (enabled == _voxel_material_filter_enabled) {
		return;
	}
	_voxel_material_filter_enabled = enabled;
	emit_changed();
}

bool VoxelInstanceGenerator::is_voxel_material_filter_enabled() const {
	return _voxel_material_filter_enabled;
}

void VoxelInstanceGenerator::set_voxel_material_filter_mask(const uint32_t mask) {
	if (mask == _voxel_material_filter_mask) {
		return;
	}
	_voxel_material_filter_mask = mask;
	emit_changed();
}

uint32_t VoxelInstanceGenerator::get_voxel_material_filter_mask() const {
	return _voxel_material_filter_mask;
}

void VoxelInstanceGenerator::set_voxel_material_filter_threshold(const float p_threshold) {
	const float threshold = math::clamp(p_threshold, 0.f, 1.f);
	if (threshold == _voxel_material_filter_threshold) {
		return;
	}
	_voxel_material_filter_threshold = threshold;
	emit_changed();
}

float VoxelInstanceGenerator::get_voxel_material_filter_threshold() const {
	return _voxel_material_filter_threshold;
}

void VoxelInstanceGenerator::set_snap_to_generator_sdf_enabled(bool enabled) {
	if (_gen_sdf_snap_settings.enabled == enabled) {
		return;
	}
	_gen_sdf_snap_settings.enabled = enabled;
	emit_changed();
}

bool VoxelInstanceGenerator::get_snap_to_generator_sdf_enabled() const {
	return _gen_sdf_snap_settings.enabled;
}

void VoxelInstanceGenerator::set_snap_to_generator_sdf_search_distance(float new_distance) {
	const float checked_distance = math::max(new_distance, 0.f);
	if (checked_distance == _gen_sdf_snap_settings.search_distance) {
		return;
	}
	_gen_sdf_snap_settings.search_distance = checked_distance;
	emit_changed();
}

float VoxelInstanceGenerator::get_snap_to_generator_sdf_search_distance() const {
	return _gen_sdf_snap_settings.search_distance;
}

void VoxelInstanceGenerator::set_snap_to_generator_sdf_sample_count(int new_sample_count) {
	const uint8_t checked_sample_count =
			zylann::math::clamp<int>(new_sample_count, GEN_SDF_SAMPLE_COUNT_MIN, GEN_SDF_SAMPLE_COUNT_MAX);
	if (checked_sample_count == _gen_sdf_snap_settings.sample_count) {
		return;
	}
	_gen_sdf_snap_settings.sample_count = checked_sample_count;
	emit_changed();
}

int VoxelInstanceGenerator::get_snap_to_generator_sdf_sample_count() const {
	return _gen_sdf_snap_settings.sample_count;
}

PackedInt32Array VoxelInstanceGenerator::_b_get_voxel_material_filter_array() const {
	const unsigned int bit_count = sizeof(_voxel_material_filter_mask) * 8;
	PackedInt32Array array;
	for (unsigned int i = 0; i < bit_count; ++i) {
		if ((_voxel_material_filter_mask & (1 << i)) != 0) {
			array.append(i);
		}
	}
	return array;
}

void VoxelInstanceGenerator::_b_set_voxel_material_filter_array(PackedInt32Array material_indices) {
	const unsigned int bit_count = sizeof(_voxel_material_filter_mask) * 8;
	uint32_t mask = 0;
	Span<const int32_t> indices = to_span(material_indices);
	for (const int32_t si : indices) {
		ZN_ASSERT_CONTINUE(si >= 0);
		const unsigned int i = static_cast<unsigned int>(si);
		ZN_ASSERT_CONTINUE(i < bit_count);
		mask |= (1 << i);
	}
#if TOOLS_ENABLED
	// Only warn when running the game, because when users add new items to the array in the editor,
	// it is likely to have duplicates temporarily, until they set the desired values.
	if (!Engine::get_singleton()->is_editor_hint()) {
		const DuplicateSearchResult res = find_duplicate(indices);
		if (res.is_valid()) {
			ZN_PRINT_WARNING(format(
					"The array of material indices contains a duplicate (at indices {} and {}).", res.first, res.second
			));
		}
	}
#endif
	set_voxel_material_filter_mask(mask);
}

void VoxelInstanceGenerator::_on_noise_changed() {
	emit_changed();
}

void VoxelInstanceGenerator::_on_noise_graph_changed() {
	emit_changed();
}

#ifdef TOOLS_ENABLED

void VoxelInstanceGenerator::get_configuration_warnings(PackedStringArray &warnings) const {
	Ref<pg::VoxelGraphFunction> noise_graph = get_noise_graph();

	if (noise_graph.is_valid()) {
		// Graph compiles?
		zylann::godot::get_resource_configuration_warnings(**noise_graph, warnings, []() { return "noise_graph: "; });

		// Check I/Os
		const int expected_input_count = (_noise_dimension == DIMENSION_2D ? 2 : 3);
		const int expected_output_count = 1;
		const int input_count = noise_graph->get_input_definitions().size();
		const int output_count = noise_graph->get_output_definitions().size();
		if (input_count != expected_input_count) {
			warnings.append(String("The noise graph has an invalid number of inputs. Expected {0}, found {1}")
									.format(varray(expected_input_count, input_count)));
		}
		if (output_count != expected_output_count) {
			warnings.append(String("The noise graph has an invalid number of outputs. Expected {0}, found {1}")
									.format(varray(expected_output_count, output_count)));
		}
	}
}

void VoxelInstanceGenerator::_validate_property(PropertyInfo &p_property) const {
	// In core, `PropertyInfo.name` is a String so `operator == "literal"` works.
	// But in GodotCpp, it is a StringName, which does not have such operator.
	// So I had to use StringNames to make the code compile in both scenarios without hurting performance.
	const VoxelStringNames &sn = VoxelStringNames::get_singleton();

	if (p_property.name == sn.jitter) {
		if (_emit_mode != EMIT_ONE_PER_TRIANGLE) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
		return;
	}

	if (p_property.name == sn.triangle_area_threshold) {
		if (_emit_mode != EMIT_FROM_FACES && _emit_mode != EMIT_ONE_PER_TRIANGLE) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
		return;
	}

	if (p_property.name == sn.density) {
		if (_emit_mode == EMIT_ONE_PER_TRIANGLE) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
		return;
	}

	if (_noise.is_null() && _noise_graph.is_null()) {
		if (p_property.name == sn.noise_dimension) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
			return;
		}

		if (p_property.name == sn.noise_on_scale) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
			return;
		}
	}
}

#endif

void VoxelInstanceGenerator::_bind_methods() {
	using Self = VoxelInstanceGenerator;

	ClassDB::bind_method(D_METHOD("set_density", "density"), &Self::set_density);
	ClassDB::bind_method(D_METHOD("get_density"), &Self::get_density);

	ClassDB::bind_method(D_METHOD("set_emit_mode", "density"), &Self::set_emit_mode);
	ClassDB::bind_method(D_METHOD("get_emit_mode"), &Self::get_emit_mode);

	ClassDB::bind_method(D_METHOD("set_jitter", "jitter"), &Self::set_jitter);
	ClassDB::bind_method(D_METHOD("get_jitter"), &Self::get_jitter);

	ClassDB::bind_method(D_METHOD("set_triangle_area_threshold", "threshold"), &Self::set_triangle_area_threshold);
	ClassDB::bind_method(D_METHOD("get_triangle_area_threshold"), &Self::get_triangle_area_threshold);

	ClassDB::bind_method(D_METHOD("set_min_scale", "min_scale"), &Self::set_min_scale);
	ClassDB::bind_method(D_METHOD("get_min_scale"), &Self::get_min_scale);

	ClassDB::bind_method(D_METHOD("set_max_scale", "max_scale"), &Self::set_max_scale);
	ClassDB::bind_method(D_METHOD("get_max_scale"), &Self::get_max_scale);

	ClassDB::bind_method(D_METHOD("set_scale_distribution", "distribution"), &Self::set_scale_distribution);
	ClassDB::bind_method(D_METHOD("get_scale_distribution"), &Self::get_scale_distribution);

	ClassDB::bind_method(D_METHOD("set_vertical_alignment", "amount"), &Self::set_vertical_alignment);
	ClassDB::bind_method(D_METHOD("get_vertical_alignment"), &Self::get_vertical_alignment);

	ClassDB::bind_method(D_METHOD("set_offset_along_normal", "offset"), &Self::set_offset_along_normal);
	ClassDB::bind_method(D_METHOD("get_offset_along_normal"), &Self::get_offset_along_normal);

	ClassDB::bind_method(D_METHOD("set_min_slope_degrees", "degrees"), &Self::set_min_slope_degrees);
	ClassDB::bind_method(D_METHOD("get_min_slope_degrees"), &Self::get_min_slope_degrees);

	ClassDB::bind_method(D_METHOD("set_max_slope_degrees", "degrees"), &Self::set_max_slope_degrees);
	ClassDB::bind_method(D_METHOD("get_max_slope_degrees"), &Self::get_max_slope_degrees);

	ClassDB::bind_method(D_METHOD("set_min_slope_falloff_degrees", "degrees"), &Self::set_min_slope_falloff_degrees);
	ClassDB::bind_method(D_METHOD("get_min_slope_falloff_degrees"), &Self::get_min_slope_falloff_degrees);

	ClassDB::bind_method(D_METHOD("set_max_slope_falloff_degrees", "degrees"), &Self::set_max_slope_falloff_degrees);
	ClassDB::bind_method(D_METHOD("get_max_slope_falloff_degrees"), &Self::get_max_slope_falloff_degrees);

	ClassDB::bind_method(D_METHOD("set_min_height", "height"), &Self::set_min_height);
	ClassDB::bind_method(D_METHOD("get_min_height"), &Self::get_min_height);

	ClassDB::bind_method(D_METHOD("set_max_height", "height"), &Self::set_max_height);
	ClassDB::bind_method(D_METHOD("get_max_height"), &Self::get_max_height);

	ClassDB::bind_method(D_METHOD("set_min_height_falloff", "distance"), &Self::set_min_height_falloff);
	ClassDB::bind_method(D_METHOD("get_min_height_falloff"), &Self::get_min_height_falloff);

	ClassDB::bind_method(D_METHOD("set_max_height_falloff", "distance"), &Self::set_max_height_falloff);
	ClassDB::bind_method(D_METHOD("get_max_height_falloff"), &Self::get_max_height_falloff);

	ClassDB::bind_method(D_METHOD("set_random_vertical_flip", "enabled"), &Self::set_random_vertical_flip);
	ClassDB::bind_method(D_METHOD("get_random_vertical_flip"), &Self::get_random_vertical_flip);

	ClassDB::bind_method(D_METHOD("set_random_rotation", "enabled"), &Self::set_random_rotation);
	ClassDB::bind_method(D_METHOD("get_random_rotation"), &Self::get_random_rotation);

	ClassDB::bind_method(D_METHOD("set_noise", "noise"), &Self::set_noise);
	ClassDB::bind_method(D_METHOD("get_noise"), &Self::get_noise);

	ClassDB::bind_method(D_METHOD("set_noise_graph", "graph"), &Self::set_noise_graph);
	ClassDB::bind_method(D_METHOD("get_noise_graph"), &Self::get_noise_graph);

	ClassDB::bind_method(D_METHOD("set_noise_dimension", "dim"), &Self::set_noise_dimension);
	ClassDB::bind_method(D_METHOD("get_noise_dimension"), &Self::get_noise_dimension);

	ClassDB::bind_method(D_METHOD("set_noise_falloff", "falloff"), &Self::set_noise_falloff);
	ClassDB::bind_method(D_METHOD("get_noise_falloff"), &Self::get_noise_falloff);

	ClassDB::bind_method(D_METHOD("set_noise_threshold", "threshold"), &Self::set_noise_threshold);
	ClassDB::bind_method(D_METHOD("get_noise_threshold"), &Self::get_noise_threshold);

	ClassDB::bind_method(D_METHOD("set_noise_on_scale", "amount"), &Self::set_noise_on_scale);
	ClassDB::bind_method(D_METHOD("get_noise_on_scale"), &Self::get_noise_on_scale);

	ClassDB::bind_method(
			D_METHOD("set_voxel_texture_filter_enabled", "enabled"), &Self::set_voxel_material_filter_enabled
	);
	ClassDB::bind_method(D_METHOD("is_voxel_texture_filter_enabled"), &Self::is_voxel_material_filter_enabled);

	ClassDB::bind_method(D_METHOD("set_voxel_texture_filter_mask", "mask"), &Self::set_voxel_material_filter_mask);
	ClassDB::bind_method(D_METHOD("get_voxel_texture_filter_mask"), &Self::get_voxel_material_filter_mask);

	ClassDB::bind_method(
			D_METHOD("set_voxel_texture_filter_threshold", "threshold"), &Self::set_voxel_material_filter_threshold
	);
	ClassDB::bind_method(D_METHOD("get_voxel_texture_filter_threshold"), &Self::get_voxel_material_filter_threshold);

	ClassDB::bind_method(
			D_METHOD("set_voxel_texture_filter_array", "texture_indices"), &Self::_b_set_voxel_material_filter_array
	);
	ClassDB::bind_method(D_METHOD("get_voxel_texture_filter_array"), &Self::_b_get_voxel_material_filter_array);

	ClassDB::bind_method(
			D_METHOD("set_snap_to_generator_sdf_enabled", "enabled"), &Self::set_snap_to_generator_sdf_enabled
	);
	ClassDB::bind_method(D_METHOD("get_snap_to_generator_sdf_enabled"), &Self::get_snap_to_generator_sdf_enabled);

	ClassDB::bind_method(
			D_METHOD("set_snap_to_generator_sdf_search_distance", "d"), &Self::set_snap_to_generator_sdf_search_distance
	);
	ClassDB::bind_method(
			D_METHOD("get_snap_to_generator_sdf_search_distance"), &Self::get_snap_to_generator_sdf_search_distance
	);

	ClassDB::bind_method(
			D_METHOD("set_snap_to_generator_sdf_sample_count", "enabled"), &Self::set_snap_to_generator_sdf_sample_count
	);
	ClassDB::bind_method(
			D_METHOD("get_snap_to_generator_sdf_sample_count"), &Self::get_snap_to_generator_sdf_sample_count
	);

	ADD_GROUP("Emission", "");

	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "emit_mode", PROPERTY_HINT_ENUM, "Vertices,FacesFast,Faces,OnePerTriangle"),
			"set_emit_mode",
			"get_emit_mode"
	);

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "density", PROPERTY_HINT_RANGE, DENSITY_HINT_STRING),
			"set_density",
			"get_density"
	);

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "jitter", PROPERTY_HINT_RANGE, "0.0, 1.0, 0.01"), "set_jitter", "get_jitter"
	);

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "triangle_area_threshold", PROPERTY_HINT_RANGE, "0.0, 10.0, 0.01"),
			"set_triangle_area_threshold",
			"get_triangle_area_threshold"
	);

	ADD_GROUP("Scale", "");

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "min_scale", PROPERTY_HINT_RANGE, "0.0, 10.0, 0.01"),
			"set_min_scale",
			"get_min_scale"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "max_scale", PROPERTY_HINT_RANGE, "0.0, 10.0, 0.01"),
			"set_max_scale",
			"get_max_scale"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "scale_distribution", PROPERTY_HINT_ENUM, "Linear,Quadratic,Cubic,Quintic"),
			"set_scale_distribution",
			"get_scale_distribution"
	);

	ADD_GROUP("Rotation", "");

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "vertical_alignment", PROPERTY_HINT_RANGE, "0.0, 1.0, 0.01"),
			"set_vertical_alignment",
			"get_vertical_alignment"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "random_vertical_flip"), "set_random_vertical_flip", "get_random_vertical_flip"
	);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "random_rotation"), "set_random_rotation", "get_random_rotation");

	ADD_GROUP("Offset", "");

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "offset_along_normal"), "set_offset_along_normal", "get_offset_along_normal"
	);

	ADD_GROUP("Slope filtering", "");

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "min_slope_degrees", PROPERTY_HINT_RANGE, "0.0, 180.0, 0.1"),
			"set_min_slope_degrees",
			"get_min_slope_degrees"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "max_slope_degrees", PROPERTY_HINT_RANGE, "0.0, 180.0, 0.1"),
			"set_max_slope_degrees",
			"get_max_slope_degrees"
	);

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "min_slope_falloff_degrees", PROPERTY_HINT_RANGE, "0.0, 180.0, 0.1"),
			"set_min_slope_falloff_degrees",
			"get_min_slope_falloff_degrees"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "max_slope_falloff_degrees", PROPERTY_HINT_RANGE, "0.0, 180.0, 0.1"),
			"set_max_slope_falloff_degrees",
			"get_max_slope_falloff_degrees"
	);

	ADD_GROUP("Height filtering", "");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_height"), "set_min_height", "get_min_height");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_height"), "set_max_height", "get_max_height");

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "min_height_falloff"), "set_min_height_falloff", "get_min_height_falloff"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "max_height_falloff"), "set_max_height_falloff", "get_max_height_falloff"
	);

	ADD_GROUP("Noise filtering", "");

	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, Noise::get_class_static()),
			"set_noise",
			"get_noise"
	);
	ADD_PROPERTY(
			PropertyInfo(
					Variant::OBJECT,
					"noise_graph",
					PROPERTY_HINT_RESOURCE_TYPE,
					pg::VoxelGraphFunction::get_class_static()
			),
			"set_noise_graph",
			"get_noise_graph"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "noise_falloff", PROPERTY_HINT_RANGE, "0.0, 1.0, 0.01"),
			"set_noise_falloff",
			"get_noise_falloff"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "noise_threshold", PROPERTY_HINT_RANGE, "-2.0, 2.0, 0.01"),
			"set_noise_threshold",
			"get_noise_threshold"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "noise_dimension", PROPERTY_HINT_ENUM, "2D,3D"),
			"set_noise_dimension",
			"get_noise_dimension"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "noise_on_scale", PROPERTY_HINT_RANGE, "0.0, 1.0, 0.01"),
			"set_noise_on_scale",
			"get_noise_on_scale"
	);

	ADD_GROUP("Texture filtering", "");

	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "voxel_texture_filter_enabled"),
			"set_voxel_texture_filter_enabled",
			"is_voxel_texture_filter_enabled"
	);
	// ADD_PROPERTY(
	// 		PropertyInfo(Variant::INT, "voxel_texture_filter_mask"),
	// 		"set_voxel_texture_filter_mask",
	// 		"get_voxel_texture_filter_mask"
	// );
	ADD_PROPERTY(
			PropertyInfo(Variant::PACKED_INT32_ARRAY, "voxel_texture_filter_array"),
			"set_voxel_texture_filter_array",
			"get_voxel_texture_filter_array"
	);

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "voxel_texture_filter_threshold", PROPERTY_HINT_RANGE, "0.0, 1.0, 0.01"),
			"set_voxel_texture_filter_threshold",
			"get_voxel_texture_filter_threshold"
	);

	ADD_GROUP("Snap to generator SDF", "snap_to_generator_sdf_");

	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "snap_to_generator_sdf_enabled"),
			"set_snap_to_generator_sdf_enabled",
			"get_snap_to_generator_sdf_enabled"
	);

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "snap_to_generator_sdf_search_distance"),
			"set_snap_to_generator_sdf_search_distance",
			"get_snap_to_generator_sdf_search_distance"
	);

	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "snap_to_generator_sdf_sample_count", PROPERTY_HINT_RANGE, "2,16"),
			"set_snap_to_generator_sdf_sample_count",
			"get_snap_to_generator_sdf_sample_count"
	);

	BIND_ENUM_CONSTANT(EMIT_FROM_VERTICES);
	BIND_ENUM_CONSTANT(EMIT_FROM_FACES_FAST);
	BIND_ENUM_CONSTANT(EMIT_FROM_FACES);
	BIND_ENUM_CONSTANT(EMIT_ONE_PER_TRIANGLE);
	BIND_ENUM_CONSTANT(EMIT_MODE_COUNT);

	BIND_ENUM_CONSTANT(DISTRIBUTION_LINEAR);
	BIND_ENUM_CONSTANT(DISTRIBUTION_QUADRATIC);
	BIND_ENUM_CONSTANT(DISTRIBUTION_CUBIC);
	BIND_ENUM_CONSTANT(DISTRIBUTION_QUINTIC);
	BIND_ENUM_CONSTANT(DISTRIBUTION_COUNT);

	BIND_ENUM_CONSTANT(DIMENSION_2D);
	BIND_ENUM_CONSTANT(DIMENSION_3D);
	BIND_ENUM_CONSTANT(DIMENSION_COUNT);
}

} // namespace zylann::voxel
