#include "funcs.h"
#include "../profiling.h"

#include <core/engine.h>
#include <scene/resources/concave_polygon_shape.h>
#include <scene/resources/mesh.h>
#include <scene/resources/multimesh.h>

bool is_surface_triangulated(Array surface) {
	PoolVector3Array positions = surface[Mesh::ARRAY_VERTEX];
	PoolIntArray indices = surface[Mesh::ARRAY_INDEX];
	return positions.size() >= 3 && indices.size() >= 3;
}

bool is_mesh_empty(Ref<Mesh> mesh_ref) {
	if (mesh_ref.is_null()) {
		return true;
	}
	const Mesh &mesh = **mesh_ref;
	if (mesh.get_surface_count() == 0) {
		return true;
	}
	if (mesh.surface_get_array_len(0) == 0) {
		return true;
	}
	return false;
}

bool try_call_script(
		const Object *obj, StringName method_name, const Variant **args, unsigned int argc, Variant *out_ret) {

	ScriptInstance *script = obj->get_script_instance();
	// TODO Is has_method() needed? I've seen `call()` being called anyways in ButtonBase
	if (script == nullptr || !script->has_method(method_name)) {
		return false;
	}

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint() && !script->get_script()->is_tool()) {
		// Can't call a method on a non-tool script in the editor
		return false;
	}
#endif

	Variant::CallError err;
	Variant ret = script->call(method_name, args, argc, err);

	// TODO Why does Variant::get_call_error_text want a non-const Object pointer??? It only uses const methods
	ERR_FAIL_COND_V_MSG(err.error != Variant::CallError::CALL_OK, false,
			Variant::get_call_error_text(const_cast<Object *>(obj), method_name, nullptr, 0, err));
	// This had to be explicitely logged due to the usual GD debugger not working with threads

	if (out_ret != nullptr) {
		*out_ret = ret;
	}

	return true;
}

// Faster version of Mesh::create_trimesh_shape()
// See https://github.com/Zylann/godot_voxel/issues/54
//
Ref<ConcavePolygonShape> create_concave_polygon_shape(Vector<Array> surfaces) {
	VOXEL_PROFILE_SCOPE();

	PoolVector<Vector3> face_points;
	int face_points_size = 0;

	//find the correct size for face_points
	for (int i = 0; i < surfaces.size(); i++) {
		const Array &surface_arrays = surfaces[i];
		if (surface_arrays.size() == 0) {
			// That surface is empty
			continue;
		}
		// If the surface is not empty then it must have an expected amount of data arrays
		ERR_CONTINUE(surface_arrays.size() != Mesh::ARRAY_MAX);
		PoolVector<int> indices = surface_arrays[Mesh::ARRAY_INDEX];
		face_points_size += indices.size();
	}
	face_points.resize(face_points_size);

	if (face_points_size < 3) {
		return Ref<ConcavePolygonShape>();
	}

	//copy the points into it
	int face_points_offset = 0;
	for (int i = 0; i < surfaces.size(); i++) {
		const Array &surface_arrays = surfaces[i];
		if (surface_arrays.size() == 0) {
			continue;
		}
		PoolVector<Vector3> positions = surface_arrays[Mesh::ARRAY_VERTEX];
		PoolVector<int> indices = surface_arrays[Mesh::ARRAY_INDEX];

		ERR_FAIL_COND_V(positions.size() < 3, Ref<ConcavePolygonShape>());
		ERR_FAIL_COND_V(indices.size() < 3, Ref<ConcavePolygonShape>());
		ERR_FAIL_COND_V(indices.size() % 3 != 0, Ref<ConcavePolygonShape>());

		int face_points_count = face_points_offset + indices.size();

		{
			PoolVector<Vector3>::Write w = face_points.write();
			PoolVector<int>::Read index_r = indices.read();
			PoolVector<Vector3>::Read position_r = positions.read();

			for (int p = face_points_offset; p < face_points_count; ++p) {
				w[p] = position_r[index_r[p - face_points_offset]];
			}
		}

		face_points_offset += indices.size();
	}

	Ref<ConcavePolygonShape> shape = memnew(ConcavePolygonShape);
	shape->set_faces(face_points);
	return shape;
}

int get_visible_instance_count(const MultiMesh &mm) {
	int visible_count = mm.get_visible_instance_count();
	if (visible_count == -1) {
		visible_count = mm.get_instance_count();
	}
	return visible_count;
}
