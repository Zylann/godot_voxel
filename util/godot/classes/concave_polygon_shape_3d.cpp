#include "concave_polygon_shape_3d.h"
#include "../../math/conv.h"
#include "../../profiling.h"
#include "collision_shape_3d.h"
#include "mesh.h"

namespace zylann {

Ref<ConcavePolygonShape3D> create_concave_polygon_shape(Span<const Array> surfaces) {
	// Faster version of Mesh::create_trimesh_shape()
	// See https://github.com/Zylann/godot_voxel/issues/54

	ZN_PROFILE_SCOPE();

	PackedVector3Array face_points;
	int face_points_size = 0;

	// find the correct size for face_points
	for (unsigned int i = 0; i < surfaces.size(); i++) {
		const Array &surface_arrays = surfaces[i];
		if (surface_arrays.size() == 0) {
			// That surface is empty
			continue;
		}
		// If the surface is not empty then it must have an expected amount of data arrays
		ERR_CONTINUE(surface_arrays.size() != Mesh::ARRAY_MAX);
		PackedInt32Array indices = surface_arrays[Mesh::ARRAY_INDEX];
		face_points_size += indices.size();
	}
	face_points.resize(face_points_size);

	if (face_points_size < 3) {
		return Ref<ConcavePolygonShape3D>();
	}

	// Deindex surfaces into a single one
	unsigned int face_points_offset = 0;
	for (unsigned int i = 0; i < surfaces.size(); i++) {
		const Array &surface_arrays = surfaces[i];
		if (surface_arrays.size() == 0) {
			continue;
		}
		PackedVector3Array positions = surface_arrays[Mesh::ARRAY_VERTEX];
		PackedInt32Array indices = surface_arrays[Mesh::ARRAY_INDEX];

		ERR_FAIL_COND_V(positions.size() < 3, Ref<ConcavePolygonShape3D>());
		ERR_FAIL_COND_V(indices.size() < 3, Ref<ConcavePolygonShape3D>());
		ERR_FAIL_COND_V(indices.size() % 3 != 0, Ref<ConcavePolygonShape3D>());

		unsigned int face_points_count = face_points_offset + indices.size();

		{
			Vector3 *w = face_points.ptrw();
			const int *index_r = indices.ptr();
			const Vector3 *position_r = positions.ptr();

			for (unsigned int p = face_points_offset; p < face_points_count; ++p) {
				const int ii = p - face_points_offset;
#ifdef DEBUG_ENABLED
				CRASH_COND(ii < 0 || ii >= indices.size());
#endif
				const int index = index_r[ii];
#ifdef DEBUG_ENABLED
				CRASH_COND(index < 0 || index >= positions.size());
#endif
				w[p] = position_r[index];
			}
		}

		face_points_offset += indices.size();
	}

	Ref<ConcavePolygonShape3D> shape;
	{
		ZN_PROFILE_SCOPE_NAMED("Godot shape");
		shape.instantiate();
		shape->set_faces(face_points);
	}
	return shape;
}

Ref<ConcavePolygonShape3D> create_concave_polygon_shape(Span<const Vector3f> positions, Span<const int> indices) {
	ZN_PROFILE_SCOPE();

	PackedVector3Array face_points;

	if (indices.size() < 3) {
		return Ref<ConcavePolygonShape3D>();
	}

	face_points.resize(indices.size());

	ERR_FAIL_COND_V(positions.size() < 3, Ref<ConcavePolygonShape3D>());
	ERR_FAIL_COND_V(indices.size() < 3, Ref<ConcavePolygonShape3D>());
	ERR_FAIL_COND_V(indices.size() % 3 != 0, Ref<ConcavePolygonShape3D>());

	// Deindex mesh
	{
		Vector3 *w = face_points.ptrw();
		for (unsigned int ii = 0; ii < indices.size(); ++ii) {
			const int index = indices[ii];
			w[ii] = to_vec3(positions[index]);
		}
	}

	Ref<ConcavePolygonShape3D> shape;
	{
		ZN_PROFILE_SCOPE_NAMED("Godot shape");
		shape.instantiate();
		shape->set_faces(face_points);
	}
	return shape;
}

Ref<ConcavePolygonShape3D> create_concave_polygon_shape(const Array surface_arrays, unsigned int index_count) {
	ZN_PROFILE_SCOPE();

	Ref<ConcavePolygonShape3D> shape;

	if (surface_arrays.size() == 0) {
		return shape;
	}
	ZN_ASSERT(surface_arrays.size() == Mesh::ARRAY_MAX);

	PackedInt32Array indices = surface_arrays[Mesh::ARRAY_INDEX];
	ERR_FAIL_COND_V(index_count < 0 || index_count > static_cast<unsigned int>(indices.size()), shape);
	if (indices.size() < 3) {
		return shape;
	}

	PackedVector3Array positions = surface_arrays[Mesh::ARRAY_VERTEX];

	ERR_FAIL_COND_V(positions.size() < 3, shape);
	ERR_FAIL_COND_V(indices.size() < 3, shape);
	ERR_FAIL_COND_V(indices.size() % 3 != 0, shape);

	PackedVector3Array face_points;
	face_points.resize(indices.size());

	// Deindex mesh
	{
		Vector3 *w = face_points.ptrw();
		for (unsigned int ii = 0; ii < index_count; ++ii) {
			const int index = indices[ii];
			w[ii] = positions[index];
		}
	}

	{
		ZN_PROFILE_SCOPE_NAMED("Godot shape");
		shape.instantiate();
		shape->set_faces(face_points);
	}
	return shape;
}

} // namespace zylann
