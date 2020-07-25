#include "fast_quadric_mesh_simplifier.h"
#include "../thirdparty/fast_quadric_mesh_simplification/Simplify.h"

#include <scene/resources/mesh.h>

inline Vector3 to_godot(vec3f a) {
	return Vector3(a.x, a.y, a.z);
}

FastQuadricMeshSimplifier::FastQuadricMeshSimplifier() {
}

FastQuadricMeshSimplifier::~FastQuadricMeshSimplifier() {
}

void FastQuadricMeshSimplifier::simplify(Ref<ArrayMesh> p_mesh, float ratio, bool smooth) {
	ERR_FAIL_COND(p_mesh.is_null());

	Array arrays = p_mesh->surface_get_arrays(0);
	ERR_FAIL_COND(arrays.size() == 0);
	ERR_FAIL_COND(p_mesh->surface_get_primitive_type(0) != Mesh::PRIMITIVE_TRIANGLES);

	PoolVector3Array positions = arrays[Mesh::ARRAY_VERTEX];
	PoolVector3Array normals = arrays[Mesh::ARRAY_NORMAL];
	PoolIntArray indices = arrays[Mesh::ARRAY_INDEX];
	ERR_FAIL_COND(indices.size() % 3 != 0);
	ERR_FAIL_COND(indices.size() == 0);

	Simplify::vertices.resize(positions.size());
	Simplify::triangles.resize(indices.size() / 3);
	Simplify::refs.clear();

	// TODO Make the data local, global vars are evil

	{
		PoolVector3Array::Read r = positions.read();
		for (int i = 0; i < positions.size(); ++i) {
			Simplify::Vertex &v = Simplify::vertices[i];
			v.p.x = r[i].x;
			v.p.y = r[i].y;
			v.p.z = r[i].z;
		}
	}

	{
		PoolIntArray::Read r = indices.read();
		int triangle_count = indices.size() / 3;
		for (int ti = 0; ti < triangle_count; ++ti) {
			Simplify::Triangle &t = Simplify::triangles[ti];
			t.v[0] = r[ti * 3];
			t.v[1] = r[ti * 3 + 1];
			t.v[2] = r[ti * 3 + 2];
		}
	}

	int target_count = positions.size() * ratio;
	Simplify::simplify_mesh(target_count, 7.0, false);

	// Normals are kind of lost in the process, so I have to make a choice here...
	// https://github.com/sp4cerat/Fast-Quadric-Mesh-Simplification/issues/18

	if (smooth) {
		positions.resize(Simplify::vertices.size());
		normals.resize(Simplify::vertices.size());
		indices.resize(Simplify::triangles.size() * 3);

		{
			PoolVector3Array::Write w = positions.write();
			for (int i = 0; i < Simplify::vertices.size(); ++i) {
				const Simplify::Vertex &v = Simplify::vertices[i];
				w[i] = to_godot(v.p);
			}
		}
		{
			PoolIntArray::Write w = indices.write();
			PoolVector3Array::Write nw = normals.write();

			for (int ti = 0; ti < Simplify::triangles.size(); ++ti) {
				const Simplify::Triangle &t = Simplify::triangles[ti];
				if (!t.deleted) {
					w[ti * 3] = t.v[0];
					w[ti * 3 + 1] = t.v[1];
					w[ti * 3 + 2] = t.v[2];

					// All connected triangles have the same weight.
					// I have no idea how correct that is.
					// Also the library uses flipped normals for some reason.
					const Vector3 n = -to_godot(t.n);
					nw[t.v[0]] += n;
					nw[t.v[1]] += n;
					nw[t.v[2]] += n;
				}
			}
		}
		{
			PoolVector3Array::Write nw = normals.write();
			for (int i = 0; i < normals.size(); ++i) {
				nw[i].normalize();
			}
		}

	} else {
		// TODO Every triangle will be made of individual vertices, none will be re-used!
		// For that the mesh needs to be re-indexed,
		// and at this point I'm not sure if simplification will turn out to be that fast

		positions.resize(Simplify::triangles.size() * 3);
		normals.resize(positions.size());
		indices.resize(positions.size());

		{
			PoolVector3Array::Write nw = normals.write();
			PoolVector3Array::Write pw = positions.write();
			int vi = 0;

			for (int ti = 0; ti < Simplify::triangles.size(); ++ti) {
				const Simplify::Triangle &t = Simplify::triangles[ti];

				if (!t.deleted) {
					const vec3f p0 = Simplify::vertices[t.v[0]].p;
					const vec3f p1 = Simplify::vertices[t.v[1]].p;
					const vec3f p2 = Simplify::vertices[t.v[2]].p;

					pw[vi] = to_godot(p0);
					pw[vi + 1] = to_godot(p1);
					pw[vi + 2] = to_godot(p2);

					const Vector3 n = -to_godot(t.n);
					nw[vi] = n;
					nw[vi + 1] = n;
					nw[vi + 2] = n;

					vi += 3;
				}
			}
		}
		{
			PoolIntArray::Write iw = indices.write();
			for (int i = 0; i < indices.size(); ++i) {
				iw[i] = i;
			}
		}
	}

	// TODO Preserve UV and COLOR
	arrays.clear();
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = positions;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_INDEX] = indices;

	p_mesh->surface_remove(0);
	p_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
}

void FastQuadricMeshSimplifier::_bind_methods() {
	ClassDB::bind_method(D_METHOD("simplify", "mesh", "ratio", "smooth"), &FastQuadricMeshSimplifier::simplify);
}
