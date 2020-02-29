#ifndef VOXEL_GRAPH_RUNTIME_H
#define VOXEL_GRAPH_RUNTIME_H

#include "../../math/interval.h"
#include "../../math/vector3i.h"
#include "program_graph.h"

class VoxelGraphRuntime {
public:
	void clear();
	void compile(const ProgramGraph &graph);
	float generate_single(const Vector3i &position);
	Interval analyze_range(Vector3i min_pos, Vector3i max_pos);

private:
	std::vector<uint8_t> _program;
	std::vector<float> _memory;
};

#endif // VOXEL_GRAPH_RUNTIME_H
