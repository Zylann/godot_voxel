#ifndef MESH_BUILDER_H
#define MESH_BUILDER_H

#include "../../util/containers/std_map.h"
#include "../../util/containers/std_vector.h"
#include "../../util/godot/core/array.h"
#include "../../util/math/vector3f.h"

namespace zylann::voxel::dmc {

// Faster than SurfaceTool, only does what is needed to build a smooth mesh
class MeshBuilder {
public:
	MeshBuilder() : _reused_vertices(0) {}

	inline void add_vertex(Vector3f position, Vector3f normal) {
		int i = 0;

		StdMap<Vector3f, int>::iterator it = _position_to_index.find(position);

		if (it != _position_to_index.end()) {
			i = it->second;
			++_reused_vertices;

		} else {
			i = _positions.size();
			_position_to_index.insert({ position, i });

			_positions.push_back(position);
			_normals.push_back(normal);
		}

		_indices.push_back(i);
	}

	void scale(float scale);
	Array commit(bool wireframe);
	void clear();

	int get_reused_vertex_count() const {
		return _reused_vertices;
	}

private:
	StdVector<Vector3f> _positions;
	StdVector<Vector3f> _normals;
	StdVector<int> _indices;
	StdMap<Vector3f, int> _position_to_index;
	int _reused_vertices;
};

} // namespace zylann::voxel::dmc

#endif // MESH_BUILDER_H
