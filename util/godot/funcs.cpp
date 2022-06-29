#include "funcs.h"
#include "../math/conv.h"
#include "../profiling.h"

#include <core/config/engine.h>
#include <scene/main/node.h>
#include <scene/resources/concave_polygon_shape_3d.h>
#include <scene/resources/mesh.h>
#include <scene/resources/multimesh.h>
#include <map>
#include <sstream>
#include <unordered_map>

namespace zylann {

bool is_surface_triangulated(Array surface) {
	PackedVector3Array positions = surface[Mesh::ARRAY_VERTEX];
	PackedInt32Array indices = surface[Mesh::ARRAY_INDEX];
	return positions.size() >= 3 && indices.size() >= 3;
}

bool is_mesh_empty(const Mesh &mesh) {
	if (mesh.get_surface_count() == 0) {
		return true;
	}
	if (mesh.surface_get_array_len(0) == 0) {
		return true;
	}
	return false;
}

Ref<ConcavePolygonShape3D> create_concave_polygon_shape(Span<const Array> surfaces) {
	// Faster version of Mesh::create_trimesh_shape()
	// See https://github.com/Zylann/godot_voxel/issues/54

	ZN_PROFILE_SCOPE();

	PackedVector3Array face_points;
	int face_points_size = 0;

	//find the correct size for face_points
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

int get_visible_instance_count(const MultiMesh &mm) {
	int visible_count = mm.get_visible_instance_count();
	if (visible_count == -1) {
		visible_count = mm.get_instance_count();
	}
	return visible_count;
}

Array generate_debug_seams_wireframe_surface(const Mesh &src_mesh, int surface_index) {
	if (src_mesh.surface_get_primitive_type(surface_index) != Mesh::PRIMITIVE_TRIANGLES) {
		return Array();
	}
	Array src_surface = src_mesh.surface_get_arrays(surface_index);
	if (src_surface.is_empty()) {
		return Array();
	}
	PackedVector3Array src_positions = src_surface[Mesh::ARRAY_VERTEX];
	PackedVector3Array src_normals = src_surface[Mesh::ARRAY_NORMAL];
	PackedInt32Array src_indices = src_surface[Mesh::ARRAY_INDEX];
	if (src_indices.size() < 3) {
		return Array();
	}
	struct Dupe {
		int dst_index = 0;
		int count = 0;
	};

	// Using a map so we can have a comparator with floating error
	std::map<Vector3, Dupe> vertex_to_dupe;
	std::unordered_map<int, int> src_index_to_dst_index;
	std::vector<Vector3> dst_positions;
	{
		//const Vector3 *src_positions_read = src_positions.ptr();
		//const Vector3 *src_normals_read = src_normals.ptr();
		for (int i = 0; i < src_positions.size(); ++i) {
			const Vector3 pos = src_positions[i];
			auto dupe_it = vertex_to_dupe.find(pos);
			if (dupe_it == vertex_to_dupe.end()) {
				vertex_to_dupe.insert({ pos, Dupe() });
			} else {
				Dupe &dupe = dupe_it->second;
				if (dupe.count == 0) {
					dupe.dst_index = dst_positions.size();
					dst_positions.push_back(pos + src_normals[i] * 0.05);
				}
				++dupe.count;
				src_index_to_dst_index.insert({ i, dupe.dst_index });
			}
		}
	}

	std::vector<int> dst_indices;
	{
		//PoolIntArray::Read r = src_indices.read();
		for (int i = 0; i < src_indices.size(); i += 3) {
			const int vi0 = src_indices[i];
			const int vi1 = src_indices[i + 1];
			const int vi2 = src_indices[i + 2];

			auto v0_it = src_index_to_dst_index.find(vi0);
			auto v1_it = src_index_to_dst_index.find(vi1);
			auto v2_it = src_index_to_dst_index.find(vi2);

			if (v0_it != src_index_to_dst_index.end() && v1_it != src_index_to_dst_index.end()) {
				dst_indices.push_back(v0_it->second);
				dst_indices.push_back(v1_it->second);
			}
			if (v1_it != src_index_to_dst_index.end() && v2_it != src_index_to_dst_index.end()) {
				dst_indices.push_back(v1_it->second);
				dst_indices.push_back(v2_it->second);
			}
			if (v2_it != src_index_to_dst_index.end() && v0_it != src_index_to_dst_index.end()) {
				dst_indices.push_back(v2_it->second);
				dst_indices.push_back(v0_it->second);
			}
		}
	}

	if (dst_indices.size() == 0) {
		return Array();
	}

	ERR_FAIL_COND_V(dst_indices.size() % 2 != 0, Array());
	ERR_FAIL_COND_V(dst_positions.size() < 2, Array());

	PackedVector3Array dst_positions_pv;
	PackedInt32Array dst_indices_pv;
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

template <typename F>
void for_each_node_depth_first(Node *parent, F f) {
	ERR_FAIL_COND(parent == nullptr);
	f(parent);
	for (int i = 0; i < parent->get_child_count(); ++i) {
		for_each_node_depth_first(parent->get_child(i), f);
	}
}

void set_nodes_owner(Node *root, Node *owner) {
	for_each_node_depth_first(root, [owner](Node *node) { //
		node->set_owner(owner);
	});
}

void set_nodes_owner_except_root(Node *root, Node *owner) {
	ERR_FAIL_COND(root == nullptr);
	for (int i = 0; i < root->get_child_count(); ++i) {
		set_nodes_owner(root->get_child(i), owner);
	}
}

void copy_to(Vector<Vector3> &dst, const std::vector<Vector3f> &src) {
	dst.resize(src.size());
	// resize can fail in case allocation was not possible
	ERR_FAIL_COND(dst.size() != static_cast<int>(src.size()));

#ifdef REAL_T_IS_DOUBLE
	// Convert floats to doubles
	const unsigned int count = dst.size() * Vector3f::AXIS_COUNT;
	real_t *dst_w = reinterpret_cast<real_t *>(dst.ptrw());
	const float *src_r = reinterpret_cast<const float *>(src.data());
	for (unsigned int i = 0; i < count; ++i) {
		dst_w[i] = src_r[i];
	}
#else
	static_assert(sizeof(Vector3) == sizeof(Vector3f));
	memcpy(dst.ptrw(), reinterpret_cast<const Vector3 *>(src.data()), src.size() * sizeof(Vector3f));
#endif
}

void copy_to(Vector<Vector2> &dst, const std::vector<Vector2f> &src) {
	dst.resize(src.size());
	// resize can fail in case allocation was not possible
	ERR_FAIL_COND(dst.size() != static_cast<int>(src.size()));

#ifdef REAL_T_IS_DOUBLE
	// Convert floats to doubles
	const unsigned int count = dst.size() * Vector2f::AXIS_COUNT;
	real_t *dst_w = reinterpret_cast<real_t *>(dst.ptrw());
	const float *src_r = reinterpret_cast<const float *>(src.data());
	for (unsigned int i = 0; i < count; ++i) {
		dst_w[i] = src_r[i];
	}
#else
	static_assert(sizeof(Vector2) == sizeof(Vector2f));
	memcpy(dst.ptrw(), reinterpret_cast<const Vector2 *>(src.data()), src.size() * sizeof(Vector2f));
#endif
}

PackedStringArray to_godot(const std::vector<std::string_view> &svv) {
	PackedStringArray psa;
	psa.resize(svv.size());
	for (unsigned int i = 0; i < svv.size(); ++i) {
		psa.write[i] = to_godot(svv[i]);
	}
	return psa;
}

PackedStringArray to_godot(const std::vector<std::string> &sv) {
	PackedStringArray psa;
	psa.resize(sv.size());
	for (unsigned int i = 0; i < sv.size(); ++i) {
		psa.write[i] = to_godot(sv[i]);
	}
	return psa;
}

} // namespace zylann

std::stringstream &operator<<(std::stringstream &ss, GodotStringWrapper s) {
	const CharString cs = s.s.utf8();
	// String has non-explicit constructors from various types making this ambiguous
	ss.std::stringstream::operator<<(cs.get_data());
	return ss;
}
