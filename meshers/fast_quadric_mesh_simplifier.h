#ifndef FAST_QUADRIC_MESH_SIMPLIFIER_H
#define FAST_QUADRIC_MESH_SIMPLIFIER_H

#include <core/reference.h>

class ArrayMesh;

class FastQuadricMeshSimplifier : public Reference {
	GDCLASS(FastQuadricMeshSimplifier, Reference)
public:
	FastQuadricMeshSimplifier();
	~FastQuadricMeshSimplifier();

	void simplify(Ref<ArrayMesh> p_mesh, float ratio, bool smooth);

private:
	static void _bind_methods();
};

#endif // FAST_QUADRIC_MESH_SIMPLIFIER_H
