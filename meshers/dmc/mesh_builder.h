#ifndef MESH_BUILDER_H
#define MESH_BUILDER_H

#include <core/map.h>
#include <core/math/vector3.h>
#include <vector>

namespace dmc {

// Faster than SurfaceTool, only does what is needed to build a smooth mesh
class MeshBuilder {
public:
	MeshBuilder() :
			_reused_vertices(0) {}

	inline void add_vertex(Vector3 position, Vector3 normal) {

		int i = 0;

		Map<Vector3, int>::Element *e = _position_to_index.find(position);

		if (e) {

			i = e->get();
			++_reused_vertices;

		} else {

			i = _positions.size();
			_position_to_index.insert(position, i);

			_positions.push_back(position);
			_normals.push_back(normal);
		}

		_indices.push_back(i);
	}

	void scale(float scale);
	Array commit(bool wireframe);
	void clear();

	int get_reused_vertex_count() const { return _reused_vertices; }

private:
	std::vector<Vector3> _positions;
	std::vector<Vector3> _normals;
	std::vector<int> _indices;
	Map<Vector3, int> _position_to_index;
	int _reused_vertices;
};

} // namespace dmc

#endif // MESH_BUILDER_H
