#include "funcs.h"
#include "../funcs.h"
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

Array generate_debug_seams_wireframe_surface(Ref<Mesh> src_mesh, int surface_index) {
	if (src_mesh->surface_get_primitive_type(surface_index) != Mesh::PRIMITIVE_TRIANGLES) {
		return Array();
	}
	Array src_surface = src_mesh->surface_get_arrays(surface_index);
	if (src_surface.empty()) {
		return Array();
	}
	PoolVector3Array src_positions = src_surface[Mesh::ARRAY_VERTEX];
	PoolVector3Array src_normals = src_surface[Mesh::ARRAY_NORMAL];
	PoolIntArray src_indices = src_surface[Mesh::ARRAY_INDEX];
	if (src_indices.size() < 3) {
		return Array();
	}
	struct Dupe {
		int dst_index = 0;
		int count = 0;
	};

	// TODO Maybe use a Map actually, so we can have a comparator with floating error
	HashMap<Vector3, Dupe> vertex_to_dupe;
	HashMap<int, int> src_index_to_dst_index;
	std::vector<Vector3> dst_positions;
	{
		PoolVector3Array::Read src_positions_read = src_positions.read();
		PoolVector3Array::Read src_normals_read = src_normals.read();
		for (int i = 0; i < src_positions.size(); ++i) {
			const Vector3 pos = src_positions[i];
			Dupe *dptr = vertex_to_dupe.getptr(pos);
			if (dptr == nullptr) {
				vertex_to_dupe.set(pos, Dupe());
			} else {
				if (dptr->count == 0) {
					dptr->dst_index = dst_positions.size();
					dst_positions.push_back(pos + src_normals_read[i] * 0.05);
				}
				++dptr->count;
				src_index_to_dst_index.set(i, dptr->dst_index);
			}
		}
	}

	std::vector<int> dst_indices;
	{
		PoolIntArray::Read r = src_indices.read();
		for (int i = 0; i < src_indices.size(); i += 3) {
			const int vi0 = r[i];
			const int vi1 = r[i + 1];
			const int vi2 = r[i + 2];
			const int *v0ptr = src_index_to_dst_index.getptr(vi0);
			const int *v1ptr = src_index_to_dst_index.getptr(vi1);
			const int *v2ptr = src_index_to_dst_index.getptr(vi2);
			if (v0ptr != nullptr && v1ptr != nullptr) {
				dst_indices.push_back(*v0ptr);
				dst_indices.push_back(*v1ptr);
			}
			if (v1ptr != nullptr && v2ptr != nullptr) {
				dst_indices.push_back(*v1ptr);
				dst_indices.push_back(*v2ptr);
			}
			if (v2ptr != nullptr && v0ptr != nullptr) {
				dst_indices.push_back(*v2ptr);
				dst_indices.push_back(*v0ptr);
			}
		}
	}

	if (dst_indices.size() == 0) {
		return Array();
	}

	ERR_FAIL_COND_V(dst_indices.size() % 2 != 0, Array());
	ERR_FAIL_COND_V(dst_positions.size() < 2, Array());

	PoolVector3Array dst_positions_pv;
	PoolIntArray dst_indices_pv;
	raw_copy_to(dst_positions_pv, dst_positions);
	raw_copy_to(dst_indices_pv, dst_indices);
	Array dst_surface;
	dst_surface.resize(Mesh::ARRAY_MAX);
	dst_surface[Mesh::ARRAY_VERTEX] = dst_positions_pv;
	dst_surface[Mesh::ARRAY_INDEX] = dst_indices_pv;
	return dst_surface;

	// Ref<ArrayMesh> wire_mesh;
	// wire_mesh.instance();
	// wire_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, dst_surface);

	// Ref<SpatialMaterial> line_material;
	// line_material->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
	// line_material->set_albedo(Color(1.0, 0.0, 1.0));
	// wire_mesh->surface_set_material(0, line_material);

	// return wire_mesh;
}
