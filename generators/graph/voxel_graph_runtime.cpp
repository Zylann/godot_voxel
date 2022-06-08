#include "voxel_graph_runtime.h"
#include "../../util/container_funcs.h"
#include "../../util/log.h"
#include "../../util/macros.h"
#include "../../util/profiling.h"
#include "../../util/string_funcs.h"
#ifdef TOOLS_ENABLED
#include "../../util/profiling_clock.h"
#endif
#include "voxel_generator_graph.h"
#include "voxel_graph_node_db.h"

#include <unordered_set>

//#ifdef DEBUG_ENABLED
//#define VOXEL_DEBUG_GRAPH_PROG_SENTINEL uint16_t(12345) // 48, 57 (base 10)
//#endif

namespace zylann::voxel {

VoxelGraphRuntime::VoxelGraphRuntime() {
	clear();
}

VoxelGraphRuntime::~VoxelGraphRuntime() {
	clear();
}

void VoxelGraphRuntime::clear() {
	_program.clear();
}

static Span<const uint16_t> get_outputs_from_op_address(Span<const uint16_t> operations, uint16_t op_address) {
	const uint16_t opid = operations[op_address];
	const VoxelGraphNodeDB::NodeType &node_type = VoxelGraphNodeDB::get_singleton().get_type(opid);

	const uint32_t inputs_count = node_type.inputs.size();
	const uint32_t outputs_count = node_type.outputs.size();

	// The +1 is for `opid`
	return operations.sub(op_address + 1 + inputs_count, outputs_count);
}

bool VoxelGraphRuntime::is_operation_constant(const State &state, uint16_t op_address) const {
	Span<const uint16_t> outputs = get_outputs_from_op_address(to_span_const(_program.operations), op_address);

	for (unsigned int i = 0; i < outputs.size(); ++i) {
		const uint16_t output_address = outputs[i];
		const Buffer &buffer = state.get_buffer(output_address);
		if (!(buffer.is_constant || state.get_range(output_address).is_single_value() ||
					buffer.local_users_count == 0)) {
			// At least one of the outputs cannot be predicted in the current area
			return false;
		}
	}

	return true;
}

void VoxelGraphRuntime::generate_optimized_execution_map(
		const State &state, ExecutionMap &execution_map, bool debug) const {
	FixedArray<unsigned int, MAX_OUTPUTS> all_outputs;
	for (unsigned int i = 0; i < _program.outputs_count; ++i) {
		all_outputs[i] = i;
	}
	generate_optimized_execution_map(state, execution_map, to_span_const(all_outputs, _program.outputs_count), debug);
}

const VoxelGraphRuntime::ExecutionMap &VoxelGraphRuntime::get_default_execution_map() const {
	return _program.default_execution_map;
}

// Generates a list of adresses for the operations to execute,
// skipping those that are deemed constant by the last range analysis.
// If a non-constant operation only contributes to a constant one, it will also be skipped.
// This has the effect of optimizing locally at runtime without relying on explicit conditionals.
// It can be useful for biomes, where some branches become constant when not used in the final blending.
void VoxelGraphRuntime::generate_optimized_execution_map(
		const State &state, ExecutionMap &execution_map, Span<const unsigned int> required_outputs, bool debug) const {
	ZN_PROFILE_SCOPE();

	// Range analysis results must have been computed
	ERR_FAIL_COND(state.ranges.size() == 0);

	const Program &program = _program;
	const DependencyGraph &graph = program.dependency_graph;

	execution_map.clear();

	// if (program.default_execution_map.size() == 0) {
	// 	// Can't reduce more than this
	// 	return;
	// }

	// This function will run a lot of times so better re-use the same vector
	static thread_local std::vector<uint16_t> to_process;
	to_process.clear();

	for (unsigned int i = 0; i < required_outputs.size(); ++i) {
		const unsigned int output_index = required_outputs[i];
		const unsigned int dg_index = program.outputs[output_index].dependency_graph_node_index;
		to_process.push_back(dg_index);
	}

	enum ProcessResult { NOT_PROCESSED, SKIPPABLE, REQUIRED };

	static thread_local std::vector<ProcessResult> results;
	results.clear();
	results.resize(graph.nodes.size(), NOT_PROCESSED);

	while (to_process.size() != 0) {
		const uint32_t node_index = to_process.back();
		const unsigned int to_process_previous_size = to_process.size();

		// Check needed because Godot never compiles with `_DEBUG`...
#ifdef DEBUG_ENABLED
		CRASH_COND(node_index >= graph.nodes.size());
#endif
		const DependencyGraph::Node &node = graph.nodes[node_index];

		// Ignore inputs because they are not present in the operations list
		if (!node.is_input && is_operation_constant(state, node.op_address)) {
			// Skip this operation for now.
			// If no other dependency reaches it, it will be effectively skipped in the result.
			to_process.pop_back();
			results[node_index] = SKIPPABLE;
			continue;
		}

		for (uint32_t i = node.first_dependency; i < node.end_dependency; ++i) {
			const uint32_t dep_node_index = graph.dependencies[i];
			if (results[dep_node_index] != NOT_PROCESSED) {
				// Already processed
				continue;
			}
			to_process.push_back(dep_node_index);
		}

		if (to_process_previous_size == to_process.size()) {
			to_process.pop_back();
			results[node_index] = REQUIRED;
		}
	}

	if (debug) {
		std::vector<uint32_t> &debug_nodes = execution_map.debug_nodes;
		CRASH_COND(debug_nodes.size() > 0);

		for (unsigned int node_index = 0; node_index < graph.nodes.size(); ++node_index) {
			const ProcessResult res = results[node_index];
			const DependencyGraph::Node &node = graph.nodes[node_index];

			if (res == REQUIRED) {
				uint32_t debug_node_id = node.debug_node_id;
				auto it = _program.expanded_node_id_to_user_node_id.find(debug_node_id);

				if (it != _program.expanded_node_id_to_user_node_id.end()) {
					debug_node_id = it->second;
					// if (std::find(debug_nodes.begin(), debug_nodes.end(), debug_node_id) != debug_nodes.end()) {
					// 	// Ignore duplicates. Some nodes can have been expanded into multiple ones.
					// 	continue;
					// }
				}

				debug_nodes.push_back(debug_node_id);
			}
		}
	}

	Span<const uint16_t> operations(program.operations.data(), 0, program.operations.size());
	bool xzy_start_not_assigned = true;

	// Now we have to fill buffers with the local constants we may have found.
	// We iterate nodes primarily because we have to preserve a certain order relative to outer loop optimization.
	for (unsigned int node_index = 0; node_index < graph.nodes.size(); ++node_index) {
		const ProcessResult res = results[node_index];
		const DependencyGraph::Node &node = graph.nodes[node_index];

		if (node.is_input) {
			continue;
		}

		switch (res) {
			case NOT_PROCESSED:
				continue;

			case SKIPPABLE: {
				const Span<const uint16_t> outputs = get_outputs_from_op_address(operations, node.op_address);

				for (unsigned int output_index = 0; output_index < outputs.size(); ++output_index) {
					const uint16_t output_address = outputs[output_index];
					const Buffer &buffer = state.get_buffer(output_address);

					if (buffer.is_constant) {
						// Already assigned at prepare-time
						continue;
					}

					CRASH_COND(buffer.is_binding);

					// The node is considered skippable, which means its outputs are either locally constant or unused.
					// Unused buffers can be left as-is, but local constants must be filled in.
					if (buffer.local_users_count > 0) {
						const math::Interval range = state.ranges[output_address];
						// If this interval is not a single value then the node should not have been skippable
						CRASH_COND(!range.is_single_value());
						const float v = range.min;
						for (unsigned int j = 0; j < buffer.size; ++j) {
							buffer.data[j] = v;
						}
					}
				}
			} break;

			case REQUIRED:
				if (xzy_start_not_assigned && node.op_address >= program.xzy_start_op_address) {
					// This should be correct as long as the list of nodes in the graph follows the same re-ordered
					// optimization done in `compile()` such that all nodes not depending on Y come first
					execution_map.xzy_start_index = execution_map.operation_adresses.size();
					xzy_start_not_assigned = false;
				}
				execution_map.operation_adresses.push_back(node.op_address);
				break;

			default:
				CRASH_NOW();
				break;
		}
	}
}

void VoxelGraphRuntime::generate_single(State &state, Vector3f position_f, const ExecutionMap *execution_map) const {
	generate_set(state, Span<float>(&position_f.x, 1), Span<float>(&position_f.y, 1), Span<float>(&position_f.z, 1),
			false, execution_map);
}

void VoxelGraphRuntime::prepare_state(State &state, unsigned int buffer_size, bool with_profiling) const {
	const unsigned int old_buffer_count = state.buffers.size();
	if (_program.buffer_count > state.buffers.size()) {
		state.buffers.resize(_program.buffer_count);
	}

	// Note: this must be after we resize the vector
	Span<Buffer> buffers(state.buffers, 0, state.buffers.size());
	state.buffer_size = buffer_size;

	for (auto it = _program.buffer_specs.cbegin(); it != _program.buffer_specs.cend(); ++it) {
		const BufferSpec &buffer_spec = *it;
		Buffer &buffer = buffers[buffer_spec.address];

		if (buffer_spec.is_binding) {
			if (buffer.is_binding) {
				// Forgot to unbind?
				CRASH_COND(buffer.data != nullptr);
			} else if (buffer.data != nullptr) {
				// Deallocate this buffer if it wasnt a binding and contained something
				memfree(buffer.data);
			}
		}

		buffer.is_binding = buffer_spec.is_binding;
	}

	// Allocate more buffers if needed
	if (old_buffer_count < state.buffers.size()) {
		for (size_t buffer_index = old_buffer_count; buffer_index < buffers.size(); ++buffer_index) {
			Buffer &buffer = buffers[buffer_index];
			// TODO Put all bindings at the beginning. This would avoid the branch.
			if (buffer.is_binding) {
				// These are supposed to be setup already
				continue;
			}
			// We don't expect previous stuff in those buffers since we just created their slots
			CRASH_COND(buffer.data != nullptr);
			// TODO Use pool?
			// New buffers get an up-to-date size, but must also comply with common capacity
			const unsigned int bs = math::max(state.buffer_capacity, buffer_size);
			buffer.data = reinterpret_cast<float *>(memalloc(bs * sizeof(float)));
			buffer.capacity = bs;
		}
	}

	// Make old buffers larger if needed
	if (state.buffer_capacity < buffer_size) {
		for (size_t buffer_index = 0; buffer_index < old_buffer_count; ++buffer_index) {
			Buffer &buffer = buffers[buffer_index];
			if (buffer.is_binding) {
				continue;
			}
			if (buffer.data == nullptr) {
				buffer.data = reinterpret_cast<float *>(memalloc(buffer_size * sizeof(float)));
			} else {
				buffer.data = reinterpret_cast<float *>(memrealloc(buffer.data, buffer_size * sizeof(float)));
			}
			buffer.capacity = buffer_size;
		}
		state.buffer_capacity = buffer_size;
	}
	for (auto it = state.buffers.begin(); it != state.buffers.end(); ++it) {
		Buffer &buffer = *it;
		buffer.size = buffer_size;
		buffer.is_constant = false;
	}

	state.ranges.resize(_program.buffer_count);

	// Always reset constants because we don't know if we'll run the same program as before...
	for (auto it = _program.buffer_specs.cbegin(); it != _program.buffer_specs.cend(); ++it) {
		const BufferSpec &bs = *it;
		Buffer &buffer = buffers[bs.address];
		if (bs.is_constant) {
			buffer.is_constant = true;
			buffer.constant_value = bs.constant_value;
			CRASH_COND(buffer.size > buffer.capacity);
			for (unsigned int j = 0; j < buffer_size; ++j) {
				buffer.data[j] = bs.constant_value;
			}
			CRASH_COND(bs.address >= state.ranges.size());
			state.ranges[bs.address] = math::Interval::from_single_value(bs.constant_value);
		}
	}

	// Puts sentinel values in buffers to detect non-initialized ones
	// #ifdef DEBUG_ENABLED
	// 	for (unsigned int i = 0; i < state.buffers.size(); ++i) {
	// 		Buffer &buffer = state.buffers[i];
	// 		if (!buffer.is_constant && !buffer.is_binding) {
	// 			CRASH_COND(buffer.data == nullptr);
	// 			for (unsigned int j = 0; j < buffer.size; ++j) {
	// 				buffer.data[j] = -969696.f;
	// 			}
	// 		}
	// 	}
	// #endif

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

	state.debug_profiler_times.clear();
	if (with_profiling) {
		// Give maximum size
		state.debug_profiler_times.resize(_program.dependency_graph.nodes.size());
	}
}

static inline Span<const uint8_t> read_params(Span<const uint16_t> operations, unsigned int &pc) {
	const uint16_t params_size_in_words = operations[pc];
	++pc;
	Span<const uint8_t> params;
	if (params_size_in_words > 0) {
		const size_t params_offset_in_words = operations[pc];
		// Seek to aligned position where params start
		pc += params_offset_in_words;
		params = operations.sub(pc, params_size_in_words).reinterpret_cast_to<const uint8_t>();
		pc += params_size_in_words;
	}
	return params;
}

void VoxelGraphRuntime::generate_set(State &state, Span<float> in_x, Span<float> in_y, Span<float> in_z, bool skip_xz,
		const ExecutionMap *execution_map) const {
	// I don't like putting private helper functions in headers.
	struct L {
		static inline void bind_buffer(Span<Buffer> buffers, int a, Span<float> d) {
			Buffer &buffer = buffers[a];
			CRASH_COND(!buffer.is_binding);
			buffer.data = d.data();
			buffer.size = d.size();
		}

		static inline void unbind_buffer(Span<Buffer> buffers, int a) {
			Buffer &buffer = buffers[a];
			CRASH_COND(!buffer.is_binding);
			buffer.data = nullptr;
		}
	};

	ZN_PROFILE_SCOPE();

#ifdef DEBUG_ENABLED
	// Each array must have the same size
	CRASH_COND(!(in_x.size() == in_y.size() && in_y.size() == in_z.size()));
#endif

#ifdef TOOLS_ENABLED
	const unsigned int buffer_size = in_x.size();
	ERR_FAIL_COND(state.buffers.size() < _program.buffer_count);
	ERR_FAIL_COND(state.buffers.size() == 0);
	ERR_FAIL_COND(state.buffer_size < buffer_size);
	ERR_FAIL_COND(state.buffers[0].size < buffer_size);
#ifdef DEBUG_ENABLED
	for (size_t i = 0; i < state.buffers.size(); ++i) {
		const Buffer &b = state.buffers[i];
		CRASH_COND(b.size < buffer_size);
		CRASH_COND(b.size > state.buffer_capacity);
		CRASH_COND(b.size != state.buffer_size);
		if (!b.is_binding) {
			CRASH_COND(b.size > b.capacity);
		}
	}
#endif
#endif

	Span<Buffer> buffers(state.buffers, 0, state.buffers.size());

	// Bind inputs
	if (_program.x_input_address != -1) {
		L::bind_buffer(buffers, _program.x_input_address, in_x);
	}
	if (_program.y_input_address != -1) {
		L::bind_buffer(buffers, _program.y_input_address, in_y);
	}
	if (_program.z_input_address != -1) {
		L::bind_buffer(buffers, _program.z_input_address, in_z);
	}

	const Span<const uint16_t> operations(_program.operations.data(), 0, _program.operations.size());

	Span<const uint16_t> op_adresses = execution_map != nullptr
			? to_span_const(execution_map->operation_adresses)
			: to_span_const(_program.default_execution_map.operation_adresses);
	if (skip_xz && op_adresses.size() > 0) {
		const unsigned int offset = execution_map != nullptr ? execution_map->xzy_start_index
															 : _program.default_execution_map.xzy_start_index;
		op_adresses = op_adresses.sub(offset);
	}

#ifdef TOOLS_ENABLED
	ProfilingClock profiling_clock;
	const bool profile = state.debug_profiler_times.size() > 0;
#endif

	for (unsigned int execution_map_index = 0; execution_map_index < op_adresses.size(); ++execution_map_index) {
		unsigned int pc = op_adresses[execution_map_index];

		const uint16_t opid = operations[pc++];
		const VoxelGraphNodeDB::NodeType &node_type = VoxelGraphNodeDB::get_singleton().get_type(opid);

		const uint32_t inputs_count = node_type.inputs.size();
		const uint32_t outputs_count = node_type.outputs.size();

		const Span<const uint16_t> inputs = operations.sub(pc, inputs_count);
		pc += inputs_count;
		const Span<const uint16_t> outputs = operations.sub(pc, outputs_count);
		pc += outputs_count;

		Span<const uint8_t> params = read_params(operations, pc);

		// TODO Buffers will stay bound if this error occurs!
		ERR_FAIL_COND(node_type.process_buffer_func == nullptr);
		ProcessBufferContext ctx(inputs, outputs, params, buffers, execution_map != nullptr);
		node_type.process_buffer_func(ctx);

#ifdef TOOLS_ENABLED
		if (profile) {
			const uint32_t elapsed_microseconds = profiling_clock.get_elapsed_microseconds();
			state.add_execution_time(execution_map_index, elapsed_microseconds);
			profiling_clock.restart();
		}
#endif
	}

	// Unbind buffers
	if (_program.x_input_address != -1) {
		L::unbind_buffer(buffers, _program.x_input_address);
	}
	if (_program.y_input_address != -1) {
		L::unbind_buffer(buffers, _program.y_input_address);
	}
	if (_program.z_input_address != -1) {
		L::unbind_buffer(buffers, _program.z_input_address);
	}
}

// TODO Accept float bounds
void VoxelGraphRuntime::analyze_range(State &state, Vector3i min_pos, Vector3i max_pos) const {
	ZN_PROFILE_SCOPE();

#ifdef TOOLS_ENABLED
	ERR_FAIL_COND(state.ranges.size() != _program.buffer_count);
#endif

	Span<math::Interval> ranges(state.ranges, 0, state.ranges.size());
	Span<Buffer> buffers(state.buffers, 0, state.buffers.size());

	// Reset users count, as they might be decreased during the analysis
	for (auto it = _program.buffer_specs.cbegin(); it != _program.buffer_specs.cend(); ++it) {
		const BufferSpec &bs = *it;
		Buffer &b = buffers[bs.address];
		b.local_users_count = bs.users_count;
	}

	ranges[_program.x_input_address] = math::Interval(min_pos.x, max_pos.x);
	ranges[_program.y_input_address] = math::Interval(min_pos.y, max_pos.y);
	ranges[_program.z_input_address] = math::Interval(min_pos.z, max_pos.z);

	const Span<const uint16_t> operations(_program.operations.data(), 0, _program.operations.size());

	// Here operations must all be analyzed, because we do this as a broad-phase.
	// Only narrow-phase may skip some operations eventually.
	uint32_t pc = 0;
	while (pc < operations.size()) {
		const uint16_t opid = operations[pc++];
		const VoxelGraphNodeDB::NodeType &node_type = VoxelGraphNodeDB::get_singleton().get_type(opid);

		const uint32_t inputs_count = node_type.inputs.size();
		const uint32_t outputs_count = node_type.outputs.size();

		const Span<const uint16_t> inputs = operations.sub(pc, inputs_count);
		pc += inputs_count;
		const Span<const uint16_t> outputs = operations.sub(pc, outputs_count);
		pc += outputs_count;

		Span<const uint8_t> params = read_params(operations, pc);

		ERR_FAIL_COND(node_type.range_analysis_func == nullptr);
		RangeAnalysisContext ctx(inputs, outputs, params, ranges, buffers);
		node_type.range_analysis_func(ctx);

#ifdef VOXEL_DEBUG_GRAPH_PROG_SENTINEL
		// If this fails, the program is ill-formed
		CRASH_COND(read<uint16_t>(_program, pc) != VOXEL_DEBUG_GRAPH_PROG_SENTINEL);
#endif
	}
}

bool VoxelGraphRuntime::try_get_output_port_address(ProgramGraph::PortLocation port, uint16_t &out_address) const {
	auto port_it = _program.user_port_to_expanded_port.find(port);
	if (port_it != _program.user_port_to_expanded_port.end()) {
		port = port_it->second;
	}
	auto aptr_it = _program.output_port_addresses.find(port);
	if (aptr_it == _program.output_port_addresses.end()) {
		// This port did not take part of the compiled result
		return false;
	}
	out_address = aptr_it->second;
	return true;
}

} // namespace zylann::voxel
