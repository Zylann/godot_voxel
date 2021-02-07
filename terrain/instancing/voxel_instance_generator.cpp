#include "voxel_instance_generator.h"
#include "../../util/profiling.h"
#include <scene/resources/mesh.h>

static inline Vector3 normalized(Vector3 pos, float &length) {
	length = pos.length();
	if (length == 0) {
		return Vector3();
	}
	pos.x /= length;
	pos.y /= length;
	pos.z /= length;
	return pos;
}

void VoxelInstanceGenerator::generate_transforms(
		std::vector<Transform> &out_transforms,
		Vector3i grid_position,
		int lod_index,
		int layer_index,
		Array surface_arrays,
		const Transform &block_local_transform,
		UpMode up_mode) {

	VOXEL_PROFILE_SCOPE();

	if (surface_arrays.size() == 0) {
		return;
	}

	PoolVector3Array vertices = surface_arrays[ArrayMesh::ARRAY_VERTEX];

	if (vertices.size() == 0) {
		return;
	}

	PoolVector3Array normals = surface_arrays[ArrayMesh::ARRAY_NORMAL];

	const uint32_t block_pos_hash = Vector3iHasher::hash(grid_position);

	const uint32_t density_u32 = 0xffffffff * _density;
	const float vertical_alignment = _vertical_alignment;
	const float scale_min = _min_scale;
	const float scale_range = _max_scale - _min_scale;
	const bool random_vertical_flip = _random_vertical_flip;
	const float offset_along_normal = _offset_along_normal;
	const float normal_min_y = _min_surface_normal_y;
	const float normal_max_y = _max_surface_normal_y;
	const bool slope_filter = normal_min_y != -1.f || normal_max_y != 1.f;
	const bool height_filter = _min_height != std::numeric_limits<float>::min() ||
							   _max_height != std::numeric_limits<float>::max();
	const float min_height = _min_height;
	const float max_height = _max_height;

	Vector3 global_up(0.f, 1.f, 0.f);

	// Using different number generators so changing parameters affecting one doesn't affect the other
	const uint64_t seed = block_pos_hash + layer_index;
	RandomPCG pcg0;
	pcg0.seed(seed);
	RandomPCG pcg1;
	pcg1.seed(seed + 1);

	out_transforms.clear();

	PoolVector3Array::Read vr = vertices.read();
	PoolVector3Array::Read nr = normals.read();

	// TODO This part might be moved to the meshing thread if it turns out to be too heavy

	for (size_t i = 0; i < vertices.size(); ++i) {
		// TODO We could actually generate indexes and pick those, rather than iterating them all and rejecting
		if (pcg0.rand() >= density_u32) {
			continue;
		}

		Transform t;
		t.origin = vr[i];

		// TODO Check if that position has been edited somehow, so we can decide to not spawn there
		// Or remesh from generator and compare sdf but that's expensive

		Vector3 axis_y;

		// Warning: sometimes mesh normals are not perfectly normalized.
		// The cause is for meshing speed on CPU. It's normalized on GPU anyways.
		Vector3 surface_normal = nr[i];
		bool surface_normal_is_normalized = false;
		bool sphere_up_is_computed = false;
		bool sphere_distance_is_computed = false;
		float sphere_distance;

		if (vertical_alignment == 0.f) {
			surface_normal.normalize();
			surface_normal_is_normalized = true;
			axis_y = surface_normal;

		} else {
			if (up_mode == UP_MODE_SPHERE) {
				global_up = normalized(block_local_transform.origin + t.origin, sphere_distance);
				sphere_up_is_computed = true;
				sphere_distance_is_computed = true;
			}

			if (vertical_alignment < 1.f) {
				axis_y = surface_normal.linear_interpolate(global_up, vertical_alignment).normalized();

			} else {
				axis_y = global_up;
			}
		}

		if (slope_filter) {
			if (!surface_normal_is_normalized) {
				surface_normal.normalize();
			}

			float ny = surface_normal.y;
			if (up_mode == UP_MODE_SPHERE) {
				if (!sphere_up_is_computed) {
					global_up = normalized(block_local_transform.origin + t.origin, sphere_distance);
					sphere_up_is_computed = true;
					sphere_distance_is_computed = true;
				}
				ny = surface_normal.dot(global_up);
			}

			if (ny < normal_min_y || ny > normal_max_y) {
				// Discard
				continue;
			}
		}

		if (height_filter) {
			float y = t.origin.y;
			if (up_mode == UP_MODE_SPHERE) {
				if (!sphere_distance_is_computed) {
					sphere_distance = (block_local_transform.origin + t.origin).length();
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
		// TODO A pool of precomputed random directions would do the job too
		const Vector3 dir = Vector3(pcg1.randf() - 0.5f, pcg1.randf() - 0.5f, pcg1.randf() - 0.5f);
		const Vector3 axis_x = axis_y.cross(dir).normalized();
		const Vector3 axis_z = axis_x.cross(axis_y);

		t.basis = Basis(
				Vector3(axis_x.x, axis_y.x, axis_z.x),
				Vector3(axis_x.y, axis_y.y, axis_z.y),
				Vector3(axis_x.z, axis_y.z, axis_z.z));

		if (scale_range > 0.f) {
			const float scale = scale_min + scale_range * pcg1.randf();
			t.basis.scale(Vector3(scale, scale, scale));

		} else if (scale_min != 1.f) {
			t.basis.scale(Vector3(scale_min, scale_min, scale_min));
		}

		out_transforms.push_back(t);
	}
}

void VoxelInstanceGenerator::set_density(float density) {
	_density = max(density, 0.f);
}

float VoxelInstanceGenerator::get_density() const {
	return _density;
}

void VoxelInstanceGenerator::set_min_scale(float min_scale) {
	_min_scale = min_scale;
}

float VoxelInstanceGenerator::get_min_scale() const {
	return _min_scale;
}

void VoxelInstanceGenerator::set_max_scale(float max_scale) {
	_max_scale = max_scale;
}

float VoxelInstanceGenerator::get_max_scale() const {
	return _max_scale;
}

void VoxelInstanceGenerator::set_vertical_alignment(float amount) {
	_vertical_alignment = clamp(amount, 0.f, 1.f);
}

float VoxelInstanceGenerator::get_vertical_alignment() const {
	return _vertical_alignment;
}

void VoxelInstanceGenerator::set_offset_along_normal(float offset) {
	_offset_along_normal = offset;
}

float VoxelInstanceGenerator::get_offset_along_normal() const {
	return _offset_along_normal;
}

void VoxelInstanceGenerator::set_min_slope_degrees(float degrees) {
	_max_surface_normal_y = min(1.f, Math::cos(Math::deg2rad(clamp(degrees, -180.f, 180.f))));
}

float VoxelInstanceGenerator::get_min_slope_degrees() const {
	return _max_surface_normal_y;
}

void VoxelInstanceGenerator::set_max_slope_degrees(float degrees) {
	_min_surface_normal_y = max(-1.f, Math::cos(Math::deg2rad(clamp(degrees, -180.f, 180.f))));
}

float VoxelInstanceGenerator::get_max_slope_degrees() const {
	return _min_surface_normal_y;
}

void VoxelInstanceGenerator::set_min_height(float h) {
	_min_height = h;
}

float VoxelInstanceGenerator::get_min_height() const {
	return _min_height;
}

void VoxelInstanceGenerator::set_max_height(float h) {
	_max_height = h;
}

float VoxelInstanceGenerator::get_max_height() const {
	return _max_height;
}

void VoxelInstanceGenerator::set_random_vertical_flip(bool flip_enabled) {
	_random_vertical_flip = flip_enabled;
}

bool VoxelInstanceGenerator::get_random_vertical_flip() const {
	return _random_vertical_flip;
}

void VoxelInstanceGenerator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_density", "density"), &VoxelInstanceGenerator::set_density);
	ClassDB::bind_method(D_METHOD("get_density"), &VoxelInstanceGenerator::get_density);

	ClassDB::bind_method(D_METHOD("set_min_scale", "min_scale"), &VoxelInstanceGenerator::set_min_scale);
	ClassDB::bind_method(D_METHOD("get_min_scale"), &VoxelInstanceGenerator::get_min_scale);

	ClassDB::bind_method(D_METHOD("set_max_scale", "max_scale"), &VoxelInstanceGenerator::set_max_scale);
	ClassDB::bind_method(D_METHOD("get_max_scale"), &VoxelInstanceGenerator::get_max_scale);

	ClassDB::bind_method(D_METHOD("set_vertical_alignment", "amount"), &VoxelInstanceGenerator::set_vertical_alignment);
	ClassDB::bind_method(D_METHOD("get_vertical_alignment"), &VoxelInstanceGenerator::get_vertical_alignment);

	ClassDB::bind_method(D_METHOD("set_offset_along_normal", "offset"),
			&VoxelInstanceGenerator::set_offset_along_normal);
	ClassDB::bind_method(D_METHOD("get_offset_along_normal"), &VoxelInstanceGenerator::get_offset_along_normal);

	ClassDB::bind_method(D_METHOD("set_min_slope_degrees", "degrees"), &VoxelInstanceGenerator::set_min_slope_degrees);
	ClassDB::bind_method(D_METHOD("get_min_slope_degrees"), &VoxelInstanceGenerator::get_min_slope_degrees);

	ClassDB::bind_method(D_METHOD("set_max_slope_degrees", "degrees"), &VoxelInstanceGenerator::set_max_slope_degrees);
	ClassDB::bind_method(D_METHOD("get_max_slope_degrees"), &VoxelInstanceGenerator::get_max_slope_degrees);

	ClassDB::bind_method(D_METHOD("set_min_height", "height"), &VoxelInstanceGenerator::set_min_height);
	ClassDB::bind_method(D_METHOD("get_min_height"), &VoxelInstanceGenerator::get_min_height);

	ClassDB::bind_method(D_METHOD("set_max_height", "height"), &VoxelInstanceGenerator::set_max_height);
	ClassDB::bind_method(D_METHOD("get_max_height"), &VoxelInstanceGenerator::get_max_height);

	ClassDB::bind_method(D_METHOD("set_random_vertical_flip", "enabled"),
			&VoxelInstanceGenerator::set_random_vertical_flip);
	ClassDB::bind_method(D_METHOD("get_random_vertical_flip"), &VoxelInstanceGenerator::get_random_vertical_flip);

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "density", PROPERTY_HINT_RANGE, "0.0, 10.0, 0.1"),
			"set_density", "get_density");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "min_scale", PROPERTY_HINT_RANGE, "0.0, 1000.0, 0.1"),
			"set_min_scale", "get_min_scale");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "max_scale", PROPERTY_HINT_RANGE, "0.0, 1000.0, 0.1"),
			"set_max_scale", "get_max_scale");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "vertical_alignment", PROPERTY_HINT_RANGE, "0.0, 1.0, 0.01"),
			"set_vertical_alignment", "get_vertical_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "offset_along_normal"),
			"set_offset_along_normal", "get_offset_along_normal");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "min_slope_degrees", PROPERTY_HINT_RANGE, "-180.0, 180.0, 0.1"),
			"set_min_slope_degrees", "get_min_slope_degrees");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "max_slope_degrees", PROPERTY_HINT_RANGE, "-180.0, 180.0, 0.1"),
			"set_max_slope_degrees", "get_max_slope_degrees");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "min_height"), "set_min_height", "get_min_height");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "max_height"), "set_max_height", "get_max_height");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "random_vertical_flip"),
			"set_random_vertical_flip", "get_random_vertical_flip");
}
