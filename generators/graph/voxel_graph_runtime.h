#ifndef VOXEL_GRAPH_RUNTIME_H
#define VOXEL_GRAPH_RUNTIME_H

#include "../../math/interval.h"
#include "../../math/vector3i.h"
#include "program_graph.h"

class VoxelGraphRuntime {
public:
	VoxelGraphRuntime();

	void clear();
	void compile(const ProgramGraph &graph);
	float generate_single(const Vector3i &position);
	Interval analyze_range(Vector3i min_pos, Vector3i max_pos);

private:
	std::vector<uint8_t> _program;
	std::vector<float> _memory;
	uint32_t _xzy_program_start;
	int _last_x;
	int _last_z;
};

#endif // VOXEL_GRAPH_RUNTIME_H
