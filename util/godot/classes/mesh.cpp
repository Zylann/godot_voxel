#include "mesh.h"

namespace zylann::godot {

bool is_surface_triangulated(const Array &surface) {
	PackedVector3Array positions = surface[Mesh::ARRAY_VERTEX];
	PackedInt32Array indices = surface[Mesh::ARRAY_INDEX];
	return positions.size() >= 3 && indices.size() >= 3;
}

bool is_mesh_empty(Span<const Array> surfaces) {
	if (surfaces.size() == 0) {
		return true;
	}
	for (const Array &surface : surfaces) {
		if (is_surface_triangulated(surface)) {
			return false;
		}
	}
	return true;
}

void scale_vec3_array(PackedVector3Array &array, float scale) {
	// Getting raw pointer because between GDExtension and modules, syntax and performance of operator[] differs.
	Vector3 *array_data = array.ptrw();
	const int count = array.size();
	for (int i = 0; i < count; ++i) {
		array_data[i] *= scale;
	}
}

void offset_vec3_array(PackedVector3Array &array, Vector3 offset) {
	// Getting raw pointer because between GDExtension and modules, syntax and performance of operator[] differs.
	Vector3 *array_data = array.ptrw();
	const int count = array.size();
	for (int i = 0; i < count; ++i) {
		array_data[i] += offset;
	}
}

void scale_surface(Array &surface, float scale) {
	PackedVector3Array positions = surface[Mesh::ARRAY_VERTEX];
	// Avoiding stupid CoW, assuming this array holds the only instance of this vector
	surface[Mesh::ARRAY_VERTEX] = PackedVector3Array();
	scale_vec3_array(positions, scale);
	surface[Mesh::ARRAY_VERTEX] = positions;
}

void offset_surface(Array &surface, Vector3 offset) {
	PackedVector3Array positions = surface[Mesh::ARRAY_VERTEX];
	// Avoiding stupid CoW, assuming this array holds the only instance of this vector
	surface[Mesh::ARRAY_VERTEX] = PackedVector3Array();
	offset_vec3_array(positions, offset);
	surface[Mesh::ARRAY_VERTEX] = positions;
}

} // namespace zylann::godot
