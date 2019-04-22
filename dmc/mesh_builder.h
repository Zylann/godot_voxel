#ifndef MESH_BUILDER_H
#define MESH_BUILDER_H

#include "../utility.h"
#include <scene/resources/mesh.h>
#include <map>
#include <vector>

namespace dmc {

// Faster than SurfaceTool, only does what is needed to build a smooth mesh
class MeshBuilder {
public:
	MeshBuilder() :
			_reused_vertices(0) {}

	inline void add_vertex(Vector3 position, Vector3 normal) {

		int i = 0;

		if (_position_to_index.find(position) != _position_to_index.end()) {

			i = _position_to_index[position];
			++_reused_vertices;

		} else {

			i = _positions.size();
			_position_to_index[position] = i;

			_positions.push_back(position);
			_normals.push_back(normal);
		}

		_indices.push_back(i);
	}

	Ref<ArrayMesh> commit(bool wireframe);
	void clear();

	int get_reused_vertex_count() const { return _reused_vertices; }

private:
	std::vector<Vector3> _positions;
	std::vector<Vector3> _normals;
	std::vector<int> _indices;
	std::map<Vector3, int> _position_to_index;
	int _reused_vertices;
};

} // namespace dmc

#endif // MESH_BUILDER_H
