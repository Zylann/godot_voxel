#include "voxel_graph_runtime.h"
#include "../../util/macros.h"
#include "../../util/noise/fast_noise_lite.h"
#include "../../util/profiling.h"
#include "image_range_grid.h"
#include "range_utility.h"
#include "voxel_generator_graph.h"
#include "voxel_graph_node_db.h"

//#include <core/image.h>
#include <core/math/math_funcs.h>
#include <modules/opensimplex/open_simplex_noise.h>
#include <scene/resources/curve.h>
#include <unordered_set>

//#ifdef DEBUG_ENABLED
//#define VOXEL_DEBUG_GRAPH_PROG_SENTINEL uint16_t(12345) // 48, 57 (base 10)
//#endif

template <typename T>
inline const T &read(const ArraySlice<uint8_t> &mem, uint32_t &p) {
#ifdef DEBUG_ENABLED
	CRASH_COND(p + sizeof(T) > mem.size());
#endif
	const T *v = reinterpret_cast<const T *>(&mem[p]);
	p += sizeof(T);
	return *v;
}

template <typename T>
inline void append(std::vector<uint8_t> &mem, const T &v) {
	size_t p = mem.size();
	mem.resize(p + sizeof(T));
	*(T *)(&mem[p]) = v;
}

VoxelGraphRuntime::VoxelGraphRuntime() {
	clear();
}

VoxelGraphRuntime::~VoxelGraphRuntime() {
	clear();
}

void VoxelGraphRuntime::clear() {
	_program.clear();
	_ranges.resize(4, Interval());
	_xzy_program_start = 0;

	_buffer_size = 0;
	for (auto it = _buffers.begin(); it != _buffers.end(); ++it) {
		Buffer &b = *it;
		if (b.data != nullptr && !b.is_binding) {
			memfree(b.data);
		}
	}
	_buffers.clear();

	_output_port_addresses.clear();

	_sdf_output_address = -1;
	_x_input_address = -1;
	_y_input_address = -1;
	_z_input_address = -1;

	_compilation_result = CompilationResult();

	for (auto it = _heap_resources.begin(); it != _heap_resources.end(); ++it) {
		HeapResource &r = *it;
		CRASH_COND(r.deleter == nullptr);
		CRASH_COND(r.ptr == nullptr);
		r.deleter(r.ptr);
	}
	_heap_resources.clear();
}

bool VoxelGraphRuntime::compile(const ProgramGraph &graph, bool debug) {
	const bool success = _compile(graph, debug);
	if (success == false) {
		const CompilationResult result = _compilation_result;
		clear();
		_compilation_result = result;
	}
	return success;
}

bool VoxelGraphRuntime::_compile(const ProgramGraph &graph, bool debug) {
	clear();

	std::vector<uint32_t> order;
	std::vector<uint32_t> terminal_nodes;

	graph.find_terminal_nodes(terminal_nodes);

	if (!debug) {
		// Exclude debug nodes
		unordered_remove_if(terminal_nodes, [&graph](uint32_t node_id) {
			const ProgramGraph::Node *node = graph.get_node(node_id);
			const VoxelGraphNodeDB::NodeType &type = VoxelGraphNodeDB::get_singleton()->get_type(node->type_id);
			return type.debug_only;
		});
	}

	graph.find_dependencies(terminal_nodes, order);

	uint32_t xzy_start_index = 0;

	// Optimize parts of the graph that only depend on X and Z,
	// so they can be moved in the outer loop when blocks are generated, running less times.
	// Moves them all at the beginning.
	{
		std::vector<uint32_t> immediate_deps;
		std::unordered_set<uint32_t> nodes_depending_on_y;
		std::vector<uint32_t> order_xz;
		std::vector<uint32_t> order_xzy;

		for (size_t i = 0; i < order.size(); ++i) {
			const uint32_t node_id = order[i];
			const ProgramGraph::Node *node = graph.get_node(node_id);

			bool depends_on_y = false;

			if (node->type_id == VoxelGeneratorGraph::NODE_INPUT_Y) {
				nodes_depending_on_y.insert(node_id);
				depends_on_y = true;
			}

			if (!depends_on_y) {
				immediate_deps.clear();
				graph.find_immediate_dependencies(node_id, immediate_deps);

				for (size_t j = 0; j < immediate_deps.size(); ++j) {
					const uint32_t dep_node_id = immediate_deps[j];

					if (nodes_depending_on_y.find(dep_node_id) != nodes_depending_on_y.end()) {
						depends_on_y = true;
						nodes_depending_on_y.insert(node_id);
						break;
					}
				}
			}

			if (depends_on_y) {
				order_xzy.push_back(node_id);
			} else {
				order_xz.push_back(node_id);
			}
		}

		xzy_start_index = order_xz.size();

		//#ifdef DEBUG_ENABLED
		//		const uint32_t order_xz_raw_size = order_xz.size();
		//		const uint32_t *order_xz_raw = order_xz.data();
		//		const uint32_t order_xzy_raw_size = order_xzy.size();
		//		const uint32_t *order_xzy_raw = order_xzy.data();
		//#endif

		size_t i = 0;
		for (size_t j = 0; j < order_xz.size(); ++j) {
			order[i++] = order_xz[j];
		}
		for (size_t j = 0; j < order_xzy.size(); ++j) {
			order[i++] = order_xzy[j];
		}
	}

	//#ifdef DEBUG_ENABLED
	//	const uint32_t order_raw_size = order.size();
	//	const uint32_t *order_raw = order.data();
	//#endif

	_ranges.clear();

	struct MemoryHelper {
		std::vector<Interval> &ra_mem;
		std::vector<Buffer> &buffer_mem;

		uint16_t add_binding() {
			uint16_t a = ra_mem.size();
			ra_mem.push_back(Interval());
			Buffer b;
			b.data = nullptr;
			b.is_constant = false;
			b.is_binding = true;
			buffer_mem.push_back(b);
			return a;
		}

		uint16_t add_var() {
			uint16_t a = ra_mem.size();
			ra_mem.push_back(Interval());
			Buffer b;
			b.data = nullptr;
			b.is_constant = false;
			b.is_binding = false;
			buffer_mem.push_back(b);
			return a;
		}

		uint16_t add_constant(float v) {
			uint16_t a = ra_mem.size();
			ra_mem.push_back(Interval::from_single_value(v));
			Buffer b;
			b.data = nullptr;
			b.is_constant = true;
			b.is_binding = false;
			b.constant_value = v;
			buffer_mem.push_back(b);
			return a;
		}
	};

	MemoryHelper mem{ _ranges, _buffers };

	// Main inputs X, Y, Z
	_x_input_address = mem.add_binding();
	_y_input_address = mem.add_binding();
	_z_input_address = mem.add_binding();

	std::vector<uint8_t> &program = _program;
	const VoxelGraphNodeDB &type_db = *VoxelGraphNodeDB::get_singleton();

	// Run through each node in order, and turn them into program instructions
	for (size_t i = 0; i < order.size(); ++i) {
		const uint32_t node_id = order[i];
		const ProgramGraph::Node *node = graph.get_node(node_id);
		const VoxelGraphNodeDB::NodeType &type = type_db.get_type(node->type_id);

		CRASH_COND(node == nullptr);
		CRASH_COND(node->inputs.size() != type.inputs.size());
		CRASH_COND(node->outputs.size() != type.outputs.size());

		if (i == xzy_start_index) {
			_xzy_program_start = _program.size();
		}

		// We still hardcode some of the nodes. Maybe we can abstract them too one day.
		switch (node->type_id) {
			case VoxelGeneratorGraph::NODE_CONSTANT: {
				CRASH_COND(type.outputs.size() != 1);
				CRASH_COND(type.params.size() != 1);
				const uint16_t a = mem.add_constant(node->params[0].operator float());
				_output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = a;
				continue;
			}

			case VoxelGeneratorGraph::NODE_INPUT_X:
				_output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = _x_input_address;
				continue;

			case VoxelGeneratorGraph::NODE_INPUT_Y:
				_output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = _y_input_address;
				continue;

			case VoxelGeneratorGraph::NODE_INPUT_Z:
				_output_port_addresses[ProgramGraph::PortLocation{ node_id, 0 }] = _z_input_address;
				continue;

			case VoxelGeneratorGraph::NODE_OUTPUT_SDF:
				if (_sdf_output_address != -1) {
					_compilation_result.success = false;
					_compilation_result.message = "Multiple SDF outputs are not supported";
					_compilation_result.node_id = node_id;
					return false;
				}
				CRASH_COND(node->inputs.size() != 1);
				if (node->inputs[0].connections.size() > 0) {
					ProgramGraph::PortLocation src_port = node->inputs[0].connections[0];
					const uint16_t *aptr = _output_port_addresses.getptr(src_port);
					// Previous node ports must have been registered
					CRASH_COND(aptr == nullptr);
					_sdf_output_address = *aptr;
				}
				continue;

			case VoxelGeneratorGraph::NODE_SDF_PREVIEW:
				continue;
		};

		// Add actual operation
		CRASH_COND(node->type_id > 0xff);
		append(program, static_cast<uint8_t>(node->type_id));

		// Inputs and outputs use a convention so we can have generic code for them.
		// Parameters are more specific, and may be affected by alignment so better just do them by hand

		// Add inputs
		for (size_t j = 0; j < type.inputs.size(); ++j) {
			uint16_t a;

			if (node->inputs[j].connections.size() == 0) {
				// No input, default it
				CRASH_COND(j >= node->default_inputs.size());
				float defval = node->default_inputs[j];
				a = mem.add_constant(defval);

			} else {
				ProgramGraph::PortLocation src_port = node->inputs[j].connections[0];
				const uint16_t *aptr = _output_port_addresses.getptr(src_port);
				// Previous node ports must have been registered
				CRASH_COND(aptr == nullptr);
				a = *aptr;
			}

			append(program, a);
		}

		// Add outputs
		for (size_t j = 0; j < type.outputs.size(); ++j) {
			const uint16_t a = mem.add_var();

			// This will be used by next nodes
			const ProgramGraph::PortLocation op{ node_id, static_cast<uint32_t>(j) };
			_output_port_addresses[op] = a;

			append(program, a);
		}

		// Add space for params size, default is no params so size is 0
		size_t params_size_index = program.size();
		append<uint16_t>(program, 0);

		if (type.compile_func != nullptr) {
			const size_t size_before = program.size();
			CompileContext ctx(*node, program, _heap_resources);
			type.compile_func(ctx);
			if (ctx.has_error()) {
				_compilation_result.success = false;
				_compilation_result.message = ctx.get_error_message();
				_compilation_result.node_id = node_id;
				return false;
			}
			const size_t params_size = program.size() - size_before;
			CRASH_COND(params_size > std::numeric_limits<uint16_t>::max());
			*reinterpret_cast<uint16_t *>(&program[params_size_index]) = params_size;
		}

#ifdef VOXEL_DEBUG_GRAPH_PROG_SENTINEL
		// Append a special value after each operation
		append(program, VOXEL_DEBUG_GRAPH_PROG_SENTINEL);
#endif
	}

	CRASH_COND(_buffers.size() != _ranges.size());

	PRINT_VERBOSE(String("Compiled voxel graph. Program size: {0}b, buffers: {1}")
						  .format(varray(
								  SIZE_T_TO_VARIANT(_program.size() * sizeof(float)),
								  SIZE_T_TO_VARIANT(_buffers.size()))));

	//ERR_FAIL_COND(_sdf_output_address == -1);
	_compilation_result.success = true;
	return true;
}

float VoxelGraphRuntime::generate_single(Vector3 position) {
	float output;
	generate_set(
			ArraySlice<float>(&position.x, 1),
			ArraySlice<float>(&position.y, 1),
			ArraySlice<float>(&position.z, 1),
			ArraySlice<float>(&output, 1), false);
	return output;
}

void VoxelGraphRuntime::generate_set(ArraySlice<float> in_x, ArraySlice<float> in_y, ArraySlice<float> in_z,
		ArraySlice<float> out_sdf, bool skip_xz) {

	// I don't like putting private helper functions in headers.
	struct L {
		static inline void bind_buffer(ArraySlice<Buffer> buffers, int a, ArraySlice<float> d) {
			Buffer &buffer = buffers[a];
			CRASH_COND(!buffer.is_binding);
			buffer.data = d.data();
			buffer.size = d.size();
			buffer.is_constant = false;
		}

		static inline void unbind_buffer(ArraySlice<Buffer> buffers, int a) {
			Buffer &buffer = buffers[a];
			CRASH_COND(!buffer.is_binding);
			buffer.data = nullptr;
		}
	};

	VOXEL_PROFILE_SCOPE();

#ifdef DEBUG_ENABLED
	CRASH_COND(_buffers.size() == 0);
	CRASH_COND(_buffers.size() != _ranges.size());
	CRASH_COND(!(in_x.size() == in_y.size() && in_y.size() == in_z.size() && in_z.size() == out_sdf.size()));
#endif
#ifdef TOOLS_ENABLED
	ERR_FAIL_COND_MSG(!has_output(), "The graph has no SDF output");
#endif

	ArraySlice<Buffer> buffers(_buffers, 0, _buffers.size());
	const uint32_t buffer_size = in_x.size();
	const bool buffer_size_changed = buffer_size != _buffer_size;
	_buffer_size = buffer_size;

	// Bind inputs
	if (_x_input_address != -1) {
		L::bind_buffer(buffers, _x_input_address, in_x);
	}
	if (_y_input_address != -1) {
		L::bind_buffer(buffers, _y_input_address, in_y);
	}
	if (_z_input_address != -1) {
		L::bind_buffer(buffers, _z_input_address, in_z);
	}

	// Prepare buffers
	if (buffer_size_changed) {
		// We ignore input buffers, these are supposed to be setup already
		for (size_t buffer_index = 0; buffer_index < _buffers.size(); ++buffer_index) {
			Buffer &buffer = _buffers[buffer_index];
			if (buffer.is_binding) {
				continue;
			}

			// TODO Use pool?
			if (buffer.data == nullptr) {
				buffer.data = reinterpret_cast<float *>(memrealloc(buffer.data, buffer_size * sizeof(float)));
			} else {
				buffer.data = reinterpret_cast<float *>(memalloc(buffer_size * sizeof(float)));
			}
			buffer.size = buffer_size;

			if (buffer.is_constant) {
				for (unsigned int j = 0; j < buffer_size; ++j) {
					buffer.data[j] = buffer.constant_value;
				}
			}
		}
	}
	/*if (use_range_analysis) {
		// TODO To be really worth it, we may need a runtime graph traversal pass,
		// where we build an execution map of nodes that are worthy 🔨

		const float ra_min = _memory[i];
		const float ra_max = _memory[i + _memory.size() / 2];

		buffer.is_constant = (ra_min == ra_max);
		if (buffer.is_constant) {
			buffer.constant_value = ra_min;
		}
	}*/

	uint32_t pc = skip_xz ? _xzy_program_start : 0;

	// STL is unreadable on debug builds of Godot, because _DEBUG isn't defined
	//#ifdef DEBUG_ENABLED
	//	const size_t memory_size = memory.size();
	//	const size_t program_size = _program.size();
	//	const float *memory_raw = memory.data();
	//	const uint8_t *program_raw = (const uint8_t *)_program.data();
	//#endif

	const ArraySlice<uint8_t> program(_program, 0, _program.size());

	while (pc < program.size()) {
		const uint8_t opid = program[pc++];
		const VoxelGraphNodeDB::NodeType &node_type = VoxelGraphNodeDB::get_singleton()->get_type(opid);

		const uint32_t inputs_size = node_type.inputs.size() * sizeof(uint16_t);
		const uint32_t outputs_size = node_type.outputs.size() * sizeof(uint16_t);

		const ArraySlice<uint16_t> inputs = program.sub(pc, inputs_size).reinterpret_cast_to<uint16_t>();
		pc += inputs_size;
		const ArraySlice<uint16_t> outputs = program.sub(pc, outputs_size).reinterpret_cast_to<uint16_t>();
		pc += outputs_size;

		const uint16_t params_size = read<uint16_t>(program, pc);
		ArraySlice<uint8_t> params;
		if (params_size > 0) {
			params = program.sub(pc, params_size);
			pc += params_size;
		}

		// Skip node if all its outputs are constant
		bool all_outputs_constant = true;
		for (uint32_t i = 0; i < outputs.size(); ++i) {
			const Buffer &buffer = buffers[outputs[i]];
			all_outputs_constant &= buffer.is_constant;
		}
		if (all_outputs_constant) {
			continue;
		}

		ERR_FAIL_COND(node_type.process_buffer_func == nullptr);
		node_type.process_buffer_func(ProcessBufferContext(inputs, outputs, params, buffers));
	}

	// Populate output buffers
	Buffer &sdf_output_buffer = buffers[_sdf_output_address];
	if (sdf_output_buffer.is_constant) {
		out_sdf.fill(sdf_output_buffer.constant_value);
	} else {
		memcpy(out_sdf.data(), sdf_output_buffer.data, buffer_size * sizeof(float));
	}

	// Unbind buffers
	if (_x_input_address != -1) {
		L::unbind_buffer(buffers, _x_input_address);
	}
	if (_y_input_address != -1) {
		L::unbind_buffer(buffers, _y_input_address);
	}
	if (_z_input_address != -1) {
		L::unbind_buffer(buffers, _z_input_address);
	}
}

// TODO Accept float bounds
Interval VoxelGraphRuntime::analyze_range(Vector3i min_pos, Vector3i max_pos) {
#ifdef TOOLS_ENABLED
	ERR_FAIL_COND_V_MSG(!has_output(), Interval(), "The graph has no SDF output");
#endif

	ArraySlice<Interval> ranges(_ranges, 0, _ranges.size());

	ranges[0] = Interval(min_pos.x, max_pos.x);
	ranges[1] = Interval(min_pos.y, max_pos.y);
	ranges[2] = Interval(min_pos.z, max_pos.z);

	const ArraySlice<uint8_t> program(_program, 0, _program.size());

	uint32_t pc = 0;
	while (pc < program.size()) {
		const uint8_t opid = program[pc++];
		const VoxelGraphNodeDB::NodeType &node_type = VoxelGraphNodeDB::get_singleton()->get_type(opid);

		const uint32_t inputs_size = node_type.inputs.size() * sizeof(uint16_t);
		const uint32_t outputs_size = node_type.outputs.size() * sizeof(uint16_t);

		const ArraySlice<uint16_t> inputs = program.sub(pc, inputs_size).reinterpret_cast_to<uint16_t>();
		pc += inputs_size;
		const ArraySlice<uint16_t> outputs = program.sub(pc, outputs_size).reinterpret_cast_to<uint16_t>();
		pc += outputs_size;

		const uint16_t params_size = read<uint16_t>(program, pc);
		ArraySlice<uint8_t> params;
		if (params_size > 0) {
			params = program.sub(pc, params_size);
			pc += params_size;
		}

		ERR_FAIL_COND_V(node_type.range_analysis_func == nullptr, Interval());
		node_type.range_analysis_func(RangeAnalysisContext(inputs, outputs, params, ranges));

#ifdef VOXEL_DEBUG_GRAPH_PROG_SENTINEL
		// If this fails, the program is ill-formed
		CRASH_COND(read<uint16_t>(_program, pc) != VOXEL_DEBUG_GRAPH_PROG_SENTINEL);
#endif
	}

	return ranges[_sdf_output_address];
}

uint16_t VoxelGraphRuntime::get_output_port_address(ProgramGraph::PortLocation port) const {
	const uint16_t *aptr = _output_port_addresses.getptr(port);
	ERR_FAIL_COND_V(aptr == nullptr, 0);
	return *aptr;
}

const VoxelGraphRuntime::Buffer &VoxelGraphRuntime::get_buffer(uint16_t address) const {
	CRASH_COND(address >= _buffers.size());
	return _buffers[address];
}
