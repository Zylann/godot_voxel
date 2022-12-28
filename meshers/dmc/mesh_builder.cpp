#include "mesh_builder.h"
#include "../../util/godot/classes/mesh.h"
#include "../../util/godot/funcs.h"

namespace zylann::voxel::dmc {

Array MeshBuilder::commit(bool wireframe) {
	if (_positions.size() == 0) {
		return Array();
	}

	ERR_FAIL_COND_V(_indices.size() % 3 != 0, Array());

	if (wireframe) {
		// Debug purpose, no effort to be fast here
		std::vector<int> wireframe_indices;

		for (unsigned int i = 0; i < _indices.size(); i += 3) {
			wireframe_indices.push_back(_indices[i]);
			wireframe_indices.push_back(_indices[i + 1]);

			wireframe_indices.push_back(_indices[i + 1]);
			wireframe_indices.push_back(_indices[i + 2]);

			wireframe_indices.push_back(_indices[i + 2]);
			wireframe_indices.push_back(_indices[i]);
		}

		_indices = wireframe_indices;
	}

	PackedVector3Array positions;
	PackedVector3Array normals;
	PackedInt32Array indices;

	copy_to(positions, _positions);
	copy_to(normals, _normals);
	copy_to(indices, _indices);

	clear();

	Array surface;
	surface.resize(Mesh::ARRAY_MAX);
	surface[Mesh::ARRAY_VERTEX] = positions;
	surface[Mesh::ARRAY_NORMAL] = normals;
	surface[Mesh::ARRAY_INDEX] = indices;

	return surface;
}

void MeshBuilder::scale(float scale) {
	for (auto it = _positions.begin(); it != _positions.end(); ++it) {
		*it *= scale;
	}
}

void MeshBuilder::clear() {
	_positions.clear();
	_normals.clear();
	_indices.clear();
	_position_to_index.clear();
	_reused_vertices = 0;
}

} // namespace zylann::voxel::dmc
