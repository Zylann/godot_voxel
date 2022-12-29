#include "voxel_instance_generator.h"
#include "../../constants/voxel_string_names.h"
#include "../../util/container_funcs.h"
#include "../../util/godot/classes/array_mesh.h"
#include "../../util/godot/core/callable.h"
#include "../../util/godot/core/random_pcg.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"

namespace zylann::voxel {

namespace {
const float MAX_DENSITY = 1.f;
const char *DENSITY_HINT_STRING = "0.0, 1.0, 0.01";
} // namespace

inline Vector3f normalized(Vector3f pos, float &length) {
	length = math::length(pos);
	if (length == 0) {
		return Vector3f();
	}
	pos.x /= length;
	pos.y /= length;
	pos.z /= length;
	return pos;
}

// Heron's formula is overly represented on SO but uses 4 square roots. This uses only one.
// A parallelogram's area is found with the magnitude of the cross product of two adjacent side vectors,
// so a triangle's area is half of it
inline float get_triangle_area(Vector3 p0, Vector3 p1, Vector3 p2) {
	const Vector3 p01 = p1 - p0;
	const Vector3 p02 = p2 - p0;
	const Vector3 c = p01.cross(p02);
	return 0.5f * c.length();
}

void VoxelInstanceGenerator::generate_transforms(std::vector<Transform3f> &out_transforms, Vector3i grid_position,
		int lod_index, int layer_id, Array surface_arrays, UpMode up_mode, uint8_t octant_mask, float block_size) {
	ZN_PROFILE_SCOPE();

	if (surface_arrays.size() < ArrayMesh::ARRAY_VERTEX && surface_arrays.size() < ArrayMesh::ARRAY_NORMAL &&
			surface_arrays.size() < ArrayMesh::ARRAY_INDEX) {
		return;
	}

	PackedVector3Array vertices = surface_arrays[ArrayMesh::ARRAY_VERTEX];
	if (vertices.size() == 0) {
		return;
	}

	if (_density <= 0.f) {
		return;
	}

	PackedVector3Array normals = surface_arrays[ArrayMesh::ARRAY_NORMAL];
	ERR_FAIL_COND(normals.size() == 0);

	PackedInt32Array indices = surface_arrays[ArrayMesh::ARRAY_INDEX];
	ERR_FAIL_COND(indices.size() == 0);
	ERR_FAIL_COND(indices.size() % 3 != 0);

	const uint32_t block_pos_hash = Vector3iHasher::hash(grid_position);

	Vector3f global_up(0.f, 1.f, 0.f);

	// Using different number generators so changing parameters affecting one doesn't affect the other
	const uint64_t seed = block_pos_hash + layer_id;
	RandomPCG pcg0;
	pcg0.seed(seed);
	RandomPCG pcg1;
	pcg1.seed(seed + 1);

	out_transforms.clear();

	// TODO This part might be moved to the meshing thread if it turns out to be too heavy

	static thread_local std::vector<Vector3f> g_vertex_cache;
	static thread_local std::vector<Vector3f> g_normal_cache;
	static thread_local std::vector<float> g_noise_cache;

	std::vector<Vector3f> &vertex_cache = g_vertex_cache;
	std::vector<Vector3f> &normal_cache = g_normal_cache;

	vertex_cache.clear();
	normal_cache.clear();

	// Pick random points
	{
		ZN_PROFILE_SCOPE_NAMED("mesh to points");

		// PackedVector3Array::Read vertices_r = vertices.read();
		// PackedVector3Array::Read normals_r = normals.read();

		// Generate base positions
		switch (_emit_mode) {
			case EMIT_FROM_VERTICES: {
				// Density is interpreted differently here,
				// so it's possible a different emit mode will produce different amounts of instances.
				// I had to use `uint64` and clamp it because floats can't contain `0xffffffff` accurately. Instead
				// it results in `0x100000000`, one unit above.
				const uint32_t density_u32 =
						math::min(uint64_t(double(0xffffffff) * _density / MAX_DENSITY), uint64_t(0xffffffff));
				const int size = vertices.size();
				for (int i = 0; i < size; ++i) {
					// TODO We could actually generate indexes and pick those,
					// rather than iterating them all and rejecting
					if (pcg0.rand() >= density_u32) {
						continue;
					}
					vertex_cache.push_back(to_vec3f(vertices[i]));
					normal_cache.push_back(to_vec3f(normals[i]));
				}
			} break;

			case EMIT_FROM_FACES_FAST: {
				// PoolIntArray::Read indices_r = indices.read();

				const int triangle_count = indices.size() / 3;

				// Assumes triangles are all roughly under the same size, and Transvoxel ones do (when not simplified),
				// so we can use number of triangles as a metric proportional to the number of instances
				const int instance_count = _density * triangle_count;

				vertex_cache.resize(instance_count);
				normal_cache.resize(instance_count);

				for (int instance_index = 0; instance_index < instance_count; ++instance_index) {
					// Pick a random triangle
					const uint32_t ii = (pcg0.rand() % triangle_count) * 3;

					const int ia = indices[ii];
					const int ib = indices[ii + 1];
					const int ic = indices[ii + 2];

					const Vector3 &pa = vertices[ia];
					const Vector3 &pb = vertices[ib];
					const Vector3 &pc = vertices[ic];

					const Vector3 &na = normals[ia];
					const Vector3 &nb = normals[ib];
					const Vector3 &nc = normals[ic];

					const float t0 = pcg1.randf();
					const float t1 = pcg1.randf();

					// This formula gives pretty uniform distribution but involves a square root
					// const Vector3 p = pa.linear_interpolate(pb, t0).linear_interpolate(pc, 1.f - sqrt(t1));

					// This is an approximation
					const Vector3 p = pa.lerp(pb, t0).lerp(pc, t1);
					const Vector3 n = na.lerp(nb, t0).lerp(nc, t1);

					vertex_cache[instance_index] = to_vec3f(p);
					normal_cache[instance_index] = to_vec3f(n);
				}

			} break;

			case EMIT_FROM_FACES: {
				// PackedInt32Array::Read indices_r = indices.read();

				const int triangle_count = indices.size() / 3;

				// static thread_local std::vector<float> g_area_cache;
				// std::vector<float> &area_cache = g_area_cache;
				// area_cache.resize(triangle_count);

				// Does not assume triangles have the same size, so instead a "unit size" is used,
				// and more instances will be placed in triangles larger than this.
				// This is roughly the size of one voxel's triangle
				// const float unit_area = 0.5f * squared(block_size / 32.f);

				float accumulator = 0.f;
				const float inv_density = 1.f / _density;

				for (int triangle_index = 0; triangle_index < triangle_count; ++triangle_index) {
					const uint32_t ii = triangle_index * 3;

					const int ia = indices[ii];
					const int ib = indices[ii + 1];
					const int ic = indices[ii + 2];

					const Vector3 &pa = vertices[ia];
					const Vector3 &pb = vertices[ib];
					const Vector3 &pc = vertices[ic];

					const Vector3 &na = normals[ia];
					const Vector3 &nb = normals[ib];
					const Vector3 &nc = normals[ic];

					const float triangle_area = get_triangle_area(pa, pb, pc);
					accumulator += triangle_area;

					const int count_in_triangle = int(accumulator * _density);

					for (int i = 0; i < count_in_triangle; ++i) {
						const float t0 = pcg1.randf();
						const float t1 = pcg1.randf();

						// This formula gives pretty uniform distribution but involves a square root
						// const Vector3 p = pa.linear_interpolate(pb, t0).linear_interpolate(pc, 1.f - sqrt(t1));

						// This is an approximation
						const Vector3 p = pa.lerp(pb, t0).lerp(pc, t1);
						const Vector3 n = na.lerp(nb, t0).lerp(nc, t1);

						vertex_cache.push_back(to_vec3f(p));
						normal_cache.push_back(to_vec3f(n));
					}

					accumulator -= count_in_triangle * inv_density;
				}

			} break;

			default:
				CRASH_NOW();
		}
	}

	// Filter out by octants
	// This is done so some octants can be filled with user-edited data instead,
	// because mesh size may not necessarily match data block size
	if ((octant_mask & 0xff) != 0xff) {
		ZN_PROFILE_SCOPE_NAMED("octant filter");
		const float h = block_size / 2.f;
		for (unsigned int i = 0; i < vertex_cache.size(); ++i) {
			const Vector3f &pos = vertex_cache[i];
			const uint8_t octant_index = get_octant_index(pos, h);
			if ((octant_mask & (1 << octant_index)) == 0) {
				unordered_remove(vertex_cache, i);
				unordered_remove(normal_cache, i);
				--i;
			}
		}
	}

	std::vector<float> &noise_cache = g_noise_cache;

	// Filter out by noise
	if (_noise.is_valid()) {
		// Position of the block relative to the instancer node.
		// Use full-precision here because we deal with potentially large coordinates
		const Vector3 mesh_block_origin_d = grid_position * block_size;
		ZN_PROFILE_SCOPE_NAMED("noise filter");

		noise_cache.clear();

		switch (_noise_dimension) {
			case DIMENSION_2D: {
				for (size_t i = 0; i < vertex_cache.size(); ++i) {
					const Vector3 &pos = to_vec3(vertex_cache[i]) + mesh_block_origin_d;
					const float n = _noise->get_noise_2d(pos.x, pos.z);
					if (n < 0) {
						unordered_remove(vertex_cache, i);
						unordered_remove(normal_cache, i);
						--i;
					} else {
						noise_cache.push_back(n);
					}
				}
			} break;

			case DIMENSION_3D: {
				for (size_t i = 0; i < vertex_cache.size(); ++i) {
					const Vector3 &pos = to_vec3(vertex_cache[i]) + mesh_block_origin_d;
					const float n = _noise->get_noise_3d(pos.x, pos.y, pos.z);
					if (n < 0) {
						unordered_remove(vertex_cache, i);
						unordered_remove(normal_cache, i);
						--i;
					} else {
						noise_cache.push_back(n);
					}
				}
			} break;

			default:
				ERR_FAIL();
		}
	}

	const float vertical_alignment = _vertical_alignment;
	const float scale_min = _min_scale;
	const float scale_range = _max_scale - _min_scale;
	const bool random_vertical_flip = _random_vertical_flip;
	const float offset_along_normal = _offset_along_normal;
	const float normal_min_y = _min_surface_normal_y;
	const float normal_max_y = _max_surface_normal_y;
	const bool slope_filter = normal_min_y != -1.f || normal_max_y != 1.f;
	const bool height_filter =
			_min_height != std::numeric_limits<float>::min() || _max_height != std::numeric_limits<float>::max();
	const float min_height = _min_height;
	const float max_height = _max_height;

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
				global_up = normalized(mesh_block_origin + t.origin, sphere_distance);
				sphere_up_is_computed = true;
				sphere_distance_is_computed = true;
			}

			if (vertical_alignment < 1.f) {
				axis_y = math::normalized(math::lerp(surface_normal, global_up, vertical_alignment));

			} else {
				axis_y = global_up;
			}
		}

		if (slope_filter) {
			if (!surface_normal_is_normalized) {
				surface_normal = math::normalized(surface_normal);
			}

			float ny = surface_normal.y;
			if (up_mode == UP_MODE_SPHERE) {
				if (!sphere_up_is_computed) {
					global_up = normalized(mesh_block_origin + t.origin, sphere_distance);
					sphere_up_is_computed = true;
					sphere_distance_is_computed = true;
				}
				ny = math::dot(surface_normal, global_up);
			}

			if (ny < normal_min_y || ny > normal_max_y) {
				// Discard
				continue;
			}
		}

		if (height_filter) {
			float y = mesh_block_origin.y + t.origin.y;
			if (up_mode == UP_MODE_SPHERE) {
				if (!sphere_distance_is_computed) {
					sphere_distance = math::length(mesh_block_origin + t.origin);
					sphere_distance_is_computed = true;
				}
				y = sphere_distance;
			}

			if (y < min_height || y > max_height) {
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

			if (_noise.is_valid() && _noise_on_scale > 0.f) {
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
	density = math::max(density, 0.f);
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
}

VoxelInstanceGenerator::EmitMode VoxelInstanceGenerator::get_emit_mode() const {
	return _emit_mode;
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

void VoxelInstanceGenerator::set_min_slope_degrees(float degrees) {
	_min_slope_degrees = math::clamp(degrees, 0.f, 180.f);
	const float max_surface_normal_y = math::min(1.f, Math::cos(math::deg_to_rad(_min_slope_degrees)));
	if (max_surface_normal_y == _max_surface_normal_y) {
		return;
	}
	_max_surface_normal_y = max_surface_normal_y;
	emit_changed();
}

float VoxelInstanceGenerator::get_min_slope_degrees() const {
	return _min_slope_degrees;
}

void VoxelInstanceGenerator::set_max_slope_degrees(float degrees) {
	_max_slope_degrees = math::clamp(degrees, 0.f, 180.f);
	const float min_surface_normal_y = math::max(-1.f, Math::cos(math::deg_to_rad(_max_slope_degrees)));
	if (min_surface_normal_y == _min_surface_normal_y) {
		return;
	}
	_min_surface_normal_y = min_surface_normal_y;
	emit_changed();
}

float VoxelInstanceGenerator::get_max_slope_degrees() const {
	return _max_slope_degrees;
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
	if (_noise == noise) {
		return;
	}
	if (_noise.is_valid()) {
		_noise->disconnect(VoxelStringNames::get_singleton().changed,
				ZN_GODOT_CALLABLE_MP(this, VoxelInstanceGenerator, _on_noise_changed));
	}
	_noise = noise;
	if (_noise.is_valid()) {
		_noise->connect(VoxelStringNames::get_singleton().changed,
				ZN_GODOT_CALLABLE_MP(this, VoxelInstanceGenerator, _on_noise_changed));
	}
	emit_changed();
}

Ref<Noise> VoxelInstanceGenerator::get_noise() const {
	return _noise;
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

void VoxelInstanceGenerator::_on_noise_changed() {
	emit_changed();
}

void VoxelInstanceGenerator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_density", "density"), &VoxelInstanceGenerator::set_density);
	ClassDB::bind_method(D_METHOD("get_density"), &VoxelInstanceGenerator::get_density);

	ClassDB::bind_method(D_METHOD("set_emit_mode", "density"), &VoxelInstanceGenerator::set_emit_mode);
	ClassDB::bind_method(D_METHOD("get_emit_mode"), &VoxelInstanceGenerator::get_emit_mode);

	ClassDB::bind_method(D_METHOD("set_min_scale", "min_scale"), &VoxelInstanceGenerator::set_min_scale);
	ClassDB::bind_method(D_METHOD("get_min_scale"), &VoxelInstanceGenerator::get_min_scale);

	ClassDB::bind_method(D_METHOD("set_max_scale", "max_scale"), &VoxelInstanceGenerator::set_max_scale);
	ClassDB::bind_method(D_METHOD("get_max_scale"), &VoxelInstanceGenerator::get_max_scale);

	ClassDB::bind_method(
			D_METHOD("set_scale_distribution", "distribution"), &VoxelInstanceGenerator::set_scale_distribution);
	ClassDB::bind_method(D_METHOD("get_scale_distribution"), &VoxelInstanceGenerator::get_scale_distribution);

	ClassDB::bind_method(D_METHOD("set_vertical_alignment", "amount"), &VoxelInstanceGenerator::set_vertical_alignment);
	ClassDB::bind_method(D_METHOD("get_vertical_alignment"), &VoxelInstanceGenerator::get_vertical_alignment);

	ClassDB::bind_method(
			D_METHOD("set_offset_along_normal", "offset"), &VoxelInstanceGenerator::set_offset_along_normal);
	ClassDB::bind_method(D_METHOD("get_offset_along_normal"), &VoxelInstanceGenerator::get_offset_along_normal);

	ClassDB::bind_method(D_METHOD("set_min_slope_degrees", "degrees"), &VoxelInstanceGenerator::set_min_slope_degrees);
	ClassDB::bind_method(D_METHOD("get_min_slope_degrees"), &VoxelInstanceGenerator::get_min_slope_degrees);

	ClassDB::bind_method(D_METHOD("set_max_slope_degrees", "degrees"), &VoxelInstanceGenerator::set_max_slope_degrees);
	ClassDB::bind_method(D_METHOD("get_max_slope_degrees"), &VoxelInstanceGenerator::get_max_slope_degrees);

	ClassDB::bind_method(D_METHOD("set_min_height", "height"), &VoxelInstanceGenerator::set_min_height);
	ClassDB::bind_method(D_METHOD("get_min_height"), &VoxelInstanceGenerator::get_min_height);

	ClassDB::bind_method(D_METHOD("set_max_height", "height"), &VoxelInstanceGenerator::set_max_height);
	ClassDB::bind_method(D_METHOD("get_max_height"), &VoxelInstanceGenerator::get_max_height);

	ClassDB::bind_method(
			D_METHOD("set_random_vertical_flip", "enabled"), &VoxelInstanceGenerator::set_random_vertical_flip);
	ClassDB::bind_method(D_METHOD("get_random_vertical_flip"), &VoxelInstanceGenerator::get_random_vertical_flip);

	ClassDB::bind_method(D_METHOD("set_random_rotation", "enabled"), &VoxelInstanceGenerator::set_random_rotation);
	ClassDB::bind_method(D_METHOD("get_random_rotation"), &VoxelInstanceGenerator::get_random_rotation);

	ClassDB::bind_method(D_METHOD("set_noise", "noise"), &VoxelInstanceGenerator::set_noise);
	ClassDB::bind_method(D_METHOD("get_noise"), &VoxelInstanceGenerator::get_noise);

	ClassDB::bind_method(D_METHOD("set_noise_dimension", "dim"), &VoxelInstanceGenerator::set_noise_dimension);
	ClassDB::bind_method(D_METHOD("get_noise_dimension"), &VoxelInstanceGenerator::get_noise_dimension);

	ClassDB::bind_method(D_METHOD("set_noise_on_scale", "amount"), &VoxelInstanceGenerator::set_noise_on_scale);
	ClassDB::bind_method(D_METHOD("get_noise_on_scale"), &VoxelInstanceGenerator::get_noise_on_scale);

#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_noise_changed"), &VoxelInstanceGenerator::_on_noise_changed);
#endif

	ADD_GROUP("Emission", "");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "density", PROPERTY_HINT_RANGE, DENSITY_HINT_STRING), "set_density",
			"get_density");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "emit_mode", PROPERTY_HINT_ENUM, "Vertices,FacesFast,Faces"),
			"set_emit_mode", "get_emit_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_slope_degrees", PROPERTY_HINT_RANGE, "0.0, 180.0, 0.1"),
			"set_min_slope_degrees", "get_min_slope_degrees");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_slope_degrees", PROPERTY_HINT_RANGE, "0.0, 180.0, 0.1"),
			"set_max_slope_degrees", "get_max_slope_degrees");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_height"), "set_min_height", "get_min_height");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_height"), "set_max_height", "get_max_height");

	ADD_GROUP("Scale", "");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_scale", PROPERTY_HINT_RANGE, "0.0, 10.0, 0.01"), "set_min_scale",
			"get_min_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_scale", PROPERTY_HINT_RANGE, "0.0, 10.0, 0.01"), "set_max_scale",
			"get_max_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "scale_distribution", PROPERTY_HINT_ENUM, "Linear,Quadratic,Cubic,Quintic"),
			"set_scale_distribution", "get_scale_distribution");

	ADD_GROUP("Rotation", "");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vertical_alignment", PROPERTY_HINT_RANGE, "0.0, 1.0, 0.01"),
			"set_vertical_alignment", "get_vertical_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "random_vertical_flip"), "set_random_vertical_flip",
			"get_random_vertical_flip");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "random_rotation"), "set_random_rotation", "get_random_rotation");

	ADD_GROUP("Offset", "");

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "offset_along_normal"), "set_offset_along_normal", "get_offset_along_normal");

	ADD_GROUP("Noise", "");

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLite"), "set_noise",
			"get_noise");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "noise_dimension", PROPERTY_HINT_ENUM, "2D,3D"), "set_noise_dimension",
			"get_noise_dimension");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_on_scale", PROPERTY_HINT_RANGE, "0.0, 1.0, 0.01"),
			"set_noise_on_scale", "get_noise_on_scale");

	BIND_ENUM_CONSTANT(EMIT_FROM_VERTICES);
	BIND_ENUM_CONSTANT(EMIT_FROM_FACES_FAST);
	BIND_ENUM_CONSTANT(EMIT_FROM_FACES);
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
