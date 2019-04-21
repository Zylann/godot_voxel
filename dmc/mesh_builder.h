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
	inline void add_vertex(Vector3 position, Vector3 normal) {

		int i = 0;

		// TODO Debug this to see if it's effectively indexing
		if (_position_to_index.find(position) != _position_to_index.end()) {

			i = _position_to_index[position];

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

private:
	std::vector<Vector3> _positions;
	std::vector<Vector3> _normals;
	std::vector<int> _indices;
	std::map<Vector3, int> _position_to_index;
};

} // namespace dmc

#endif // MESH_BUILDER_H
