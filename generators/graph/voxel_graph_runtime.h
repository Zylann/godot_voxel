#ifndef VOXEL_GRAPH_RUNTIME_H
#define VOXEL_GRAPH_RUNTIME_H

#include "../../math/interval.h"
#include "../../math/vector3i.h"
#include "program_graph.h"

// CPU VM to execute a voxel graph generator
class VoxelGraphRuntime {
public:
	VoxelGraphRuntime();

	void clear();
	void compile(const ProgramGraph &graph, bool debug);
	float generate_single(const Vector3i &position);
	Interval analyze_range(Vector3i min_pos, Vector3i max_pos);

	uint16_t get_output_port_address(ProgramGraph::PortLocation port) const;
	float get_memory_value(uint16_t address) const;

private:
	std::vector<uint8_t> _program;
	std::vector<float> _memory;
	uint32_t _xzy_program_start;
	int _last_x;
	int _last_z;
	int _sdf_output_address = -1;

	HashMap<ProgramGraph::PortLocation, uint16_t, ProgramGraph::PortLocationHasher> _output_port_addresses;
};

#endif // VOXEL_GRAPH_RUNTIME_H
