#include "voxel_debug.h"
#include "../util/direct_mesh_instance.h"
#include "../util/fixed_array.h"
#include "../util/utility.h"
#include <scene/resources/mesh.h>

namespace VoxelDebug {

FixedArray<Ref<Mesh>, ID_COUNT> g_wirecubes;
bool g_finalized = false;

template <typename T>
void raw_copy_to(PoolVector<T> &dst, const T *src, unsigned int count) {
	dst.resize(count);
	typename PoolVector<T>::Write w = dst.write();
	memcpy(w.ptr(), src, count * sizeof(T));
}

static Color get_color(ColorID id) {
	switch (id) {
		case ID_VOXEL_BOUNDS:
			return Color(1, 1, 1);
		case ID_OCTREE_BOUNDS:
			return Color(0.5, 0.5, 0.5);
		default:
			CRASH_NOW_MSG("Unexpected index");
	}
	return Color();
}

Ref<Mesh> get_wirecube(ColorID id) {
	CRASH_COND(g_finalized);

	Ref<Mesh> &wirecube = g_wirecubes[id];

	if (wirecube.is_null()) {
		const Vector3 positions_raw[] = {
			Vector3(0, 0, 0),
			Vector3(1, 0, 0),
			Vector3(1, 0, 1),
			Vector3(0, 0, 1),
			Vector3(0, 1, 0),
			Vector3(1, 1, 0),
			Vector3(1, 1, 1),
			Vector3(0, 1, 1)
		};
		PoolVector3Array positions;
		raw_copy_to(positions, positions_raw, 8);

		Color white(1.0, 1.0, 1.0);
		PoolColorArray colors;
		colors.resize(positions.size());
		{
			PoolColorArray::Write w = colors.write();
			for (int i = 0; i < colors.size(); ++i) {
				w[i] = white;
			}
		}

		const int indices_raw[] = {
			0, 1,
			1, 2,
			2, 3,
			3, 0,

			4, 5,
			5, 6,
			6, 7,
			7, 4,

			0, 4,
			1, 5,
			2, 6,
			3, 7
		};
		PoolIntArray indices;
		raw_copy_to(indices, indices_raw, 24);

		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = positions;
		arrays[Mesh::ARRAY_COLOR] = colors;
		arrays[Mesh::ARRAY_INDEX] = indices;
		Ref<ArrayMesh> mesh = memnew(ArrayMesh);
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);

		Ref<SpatialMaterial> mat;
		mat.instance();
		mat->set_albedo(get_color(id));
		mat->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
		mesh->surface_set_material(0, mat);

		wirecube = mesh;
	}

	return wirecube;
}

void free_resources() {
	for (unsigned int i = 0; i < g_wirecubes.size(); ++i) {
		g_wirecubes[i].unref();
	}
	g_finalized = true;
}

DebugRenderer::~DebugRenderer() {
	clear();
}

void DebugRenderer::clear() {
	for (auto it = _mesh_instances.begin(); it != _mesh_instances.end(); ++it) {
		memdelete(*it);
	}
	_mesh_instances.clear();
}

void DebugRenderer::set_world(World *world) {
	_world = world;
	for (auto it = _mesh_instances.begin(); it != _mesh_instances.end(); ++it) {
		(*it)->set_world(world);
	}
}

void DebugRenderer::begin() {
	CRASH_COND(_inside_block);
	CRASH_COND(_world == nullptr);
	_current = 0;
	_inside_block = true;
}

void DebugRenderer::draw_box(Transform t, ColorID color) {
	DirectMeshInstance *mi;
	if (_current >= _mesh_instances.size()) {
		mi = memnew(DirectMeshInstance);
		mi->create();
		mi->set_world(_world);
		_mesh_instances.push_back(mi);
	} else {
		mi = _mesh_instances[_current];
	}

	mi->set_mesh(get_wirecube(color));
	mi->set_transform(t);

	++_current;
}

void DebugRenderer::end() {
	CRASH_COND(!_inside_block);
	for (unsigned int i = _current; i < _mesh_instances.size(); ++i) {
		DirectMeshInstance *mi = _mesh_instances[i];
		mi->set_visible(false);
	}
	_inside_block = false;
}

} // namespace VoxelDebug
