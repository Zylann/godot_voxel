#ifndef VOXEL_GRAPH_RUNTIME_H
#define VOXEL_GRAPH_RUNTIME_H

#include "../../math/interval.h"
#include "../../math/vector3i.h"
#include "../../util/array_slice.h"
#include "program_graph.h"

class ImageRangeGrid;

// CPU VM to execute a voxel graph generator
class VoxelGraphRuntime {
public:
	struct CompilationResult {
		bool success = false;
		int node_id = -1;
		String message;
	};

	VoxelGraphRuntime();
	~VoxelGraphRuntime();

	void clear();
	bool compile(const ProgramGraph &graph, bool debug);
	float generate_single(const Vector3 &position);
	void generate_xz_slice(ArraySlice<float> dst, Vector2i dst_size, Vector2 min, Vector2 max, float y, int stride);
	Interval analyze_range(Vector3i min_pos, Vector3i max_pos);

	inline const CompilationResult &get_compilation_result() const {
		return _compilation_result;
	}

	inline bool has_output() const {
		return _sdf_output_address != -1;
	}

	uint16_t get_output_port_address(ProgramGraph::PortLocation port) const;
	float get_memory_value(uint16_t address) const;

	struct Buffer {
		float *data = nullptr;
		float constant_value;
		bool is_constant;
	};

private:
	bool _compile(const ProgramGraph &graph, bool debug);

	std::vector<uint8_t> _program;
	std::vector<float> _memory;

	std::vector<Buffer> _buffers;
	Vector2 _xz_last_min;
	Vector2 _xz_last_max;
	int _buffer_size = 0;

	std::vector<ImageRangeGrid *> _image_range_grids;
	uint32_t _xzy_program_start;
	float _last_x;
	float _last_z;
	int _sdf_output_address = -1;
	CompilationResult _compilation_result;

	HashMap<ProgramGraph::PortLocation, uint16_t, ProgramGraph::PortLocationHasher> _output_port_addresses;
};

#endif // VOXEL_GRAPH_RUNTIME_H
