#include "voxel_graph_runtime.h"
#include "../../util/container_funcs.h"
#include "../../util/godot/core/string.h"
#include "../../util/log.h"
#include "../../util/macros.h"
#include "../../util/profiling.h"
#include "../../util/string_funcs.h"
#ifdef TOOLS_ENABLED
#include "../../util/profiling_clock.h"
#endif
#include "node_type_db.h"
#include "voxel_generator_graph.h"

#include <unordered_set>

//#ifdef DEBUG_ENABLED
//#define VOXEL_DEBUG_GRAPH_PROG_SENTINEL uint16_t(12345) // 48, 57 (base 10)
//#endif

namespace zylann::voxel::pg {

Runtime::Runtime() {
	clear();
}

Runtime::~Runtime() {
	clear();
}

void Runtime::clear() {
	_program.clear();
}

static Span<const uint16_t> get_outputs_from_op_address(Span<const uint16_t> operations, uint16_t op_address) {
	const uint16_t opid = operations[op_address];
	const NodeType &node_type = NodeTypeDB::get_singleton().get_type(opid);

	const uint32_t inputs_count = node_type.inputs.size();
	const uint32_t outputs_count = node_type.outputs.size();

	// The +1 is for `opid`
	return operations.sub(op_address + 1 + inputs_count, outputs_count);
}

bool Runtime::is_operation_constant(const State &state, uint16_t op_address) const {
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

void Runtime::generate_optimized_execution_map(const State &state, ExecutionMap &execution_map, bool debug) const {
	FixedArray<unsigned int, MAX_OUTPUTS> all_outputs;
	for (unsigned int i = 0; i < _program.outputs_count; ++i) {
		all_outputs[i] = i;
	}
	generate_optimized_execution_map(state, execution_map, to_span_const(all_outputs, _program.outputs_count), debug);
}

const Runtime::ExecutionMap &Runtime::get_default_execution_map() const {
	return _program.default_execution_map;
}

// Generates a list of adresses for the operations to execute,
// skipping those that are deemed constant by the last range analysis.
// If a non-constant operation only contributes to a constant one, it will also be skipped.
// This has the effect of optimizing locally at runtime without relying on explicit conditionals.
// It can be useful for biomes, where some branches become constant when not used in the final blending.
void Runtime::generate_optimized_execution_map(
		const State &state, ExecutionMap &execution_map, Span<const unsigned int> required_outputs, bool debug) const {
	ZN_PROFILE_SCOPE();

	// Range analysis results must have been computed
	ZN_ASSERT_RETURN(state.ranges.size() != 0);

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
		ZN_ASSERT(node_index < graph.nodes.size());
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
		ZN_ASSERT(debug_nodes.size() == 0);

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
	bool inner_group_start_not_assigned = true;

	static thread_local std::vector<ExecutionMap::ConstantFill> tls_constant_fills;
	tls_constant_fills.clear();

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

					ZN_ASSERT(!buffer.is_binding);

					// The node is considered skippable, which means its outputs are either locally constant or unused.
					// Unused buffers can be left as-is, but local constants must be filled in.
					if (buffer.local_users_count > 0) {
						const math::Interval range = state.ranges[output_address];
						// If this interval is not a single value then the node should not have been skippable
						ZN_ASSERT(range.is_single_value());
						const float v = range.min;
						// When we re-use buffer data in multiple nodes, this optimization cannot work reliably
						// if we were to fill constant data here. It is possible that the data pointer
						// is written to by another operation that re-uses it prior to the skippable node.
						// This will overwrite the values we were expecting.
						// To avoid this problem defer the filling to run just before the first node reading the buffer.
						// The reason we do it is to avoid having to rewrite operations for every
						// combination of constant arguments vs buffers.
						ZN_ASSERT(buffer.data != nullptr);
						tls_constant_fills.push_back(ExecutionMap::ConstantFill{ buffer.data, v });
					}
				}
			} break;

			case REQUIRED:
				if (inner_group_start_not_assigned && node.op_address >= program.inner_group_start_op_index) {
					// This should be correct as long as the list of nodes in the graph follows the same re-ordered
					// optimization done in `compile()` such that all nodes not depending on Y come first
					execution_map.inner_group_start_index = execution_map.operations.size();
					inner_group_start_not_assigned = false;
				}

				execution_map.operations.push_back(
						ExecutionMap::OperationInfo{ node.op_address, uint16_t(tls_constant_fills.size()) });

				// TODO Only do constant fills that actually get used
				// The following approach isn't optimal. If 50% of a graph gets skipped and the remaining nodes don't
				// use constant outputs from the skipped part, they will end up being filled anyways...

				// Make it so previously skipped nodes' locally constant outputs are filled, as if they had run.
				// They won't necessarily be used by the current node, but according to buffer lifetime rules,
				// they should remain valid until all users have read them.
				for (const ExecutionMap::ConstantFill &cf : tls_constant_fills) {
					execution_map.constant_fills.push_back(cf);
				}
				tls_constant_fills.clear();

				break;

			default:
				CRASH_NOW();
				break;
		}
	}
}

void Runtime::generate_single(State &state, Span<float> inputs, const ExecutionMap *execution_map) const {
	FixedArray<Span<float>, MAX_INPUTS> input_bindings;
	ZN_ASSERT_RETURN_MSG(inputs.size() < input_bindings.size(), "Too many inputs, not supported");
	for (unsigned int i = 0; i < inputs.size(); ++i) {
		input_bindings[i] = Span<float>(&inputs[i], 1);
	}
	generate_set(state, to_span(input_bindings, inputs.size()), false, execution_map);
}

void Runtime::prepare_state(State &state, unsigned int buffer_size, bool with_profiling) const {
	// Allocate memory

	const unsigned int old_buffer_data_count = state.buffer_datas.size();
	if (state.buffer_datas.size() < _program.buffer_data_count) {
		// Create more buffer datas.
		state.buffer_datas.resize(_program.buffer_data_count);
		for (unsigned int i = old_buffer_data_count; i < state.buffer_datas.size(); ++i) {
			BufferData &bd = state.buffer_datas[i];
			ZN_ASSERT(bd.data == nullptr);
			// These are new items, we always allocate.
			bd.data = reinterpret_cast<float *>(memalloc(buffer_size * sizeof(float)));
			bd.capacity = buffer_size;
		}
	}

	if (state.buffer_size < buffer_size) {
		// Make existing buffer datas larger.
		for (unsigned int i = 0; i < old_buffer_data_count; ++i) {
			BufferData &bd = state.buffer_datas[i];
			ZN_ASSERT(bd.data != nullptr);
			if (bd.capacity < buffer_size) {
				// These are existing items, we always realloc.
				bd.data = reinterpret_cast<float *>(memrealloc(bd.data, buffer_size * sizeof(float)));
				bd.capacity = buffer_size;
			}
		}
		// TODO Not sure if worth keeping capacity at state level. Buffer datas can have varying capacities depending on
		// which multiple graphs were prepared before.
		state.buffer_capacity = buffer_size;
	}

	// Initialize buffers

#if DEBUG_ENABLED
	for (const Buffer &buffer : state.buffers) {
		if (buffer.is_binding) {
			// Forgot to unbind?
			ZN_ASSERT(buffer.data == nullptr);
		}
	}
#endif

	state.buffers.resize(_program.buffer_count);
	state.ranges.resize(_program.buffer_count);
	// Note: this must be after we resize the vector.
	// Doing this mainly because Godot doesn't compile with standard library boundary checks...
	Span<Buffer> buffers = to_span(state.buffers);
	Span<BufferData> buffer_datas = to_span(state.buffer_datas);
	Span<math::Interval> ranges = to_span(state.ranges);

	state.buffer_size = buffer_size;

	for (const BufferSpec &buffer_spec : _program.buffer_specs) {
		Buffer &buffer = buffers[buffer_spec.address];

		if (buffer_spec.has_data) {
			ZN_ASSERT(!buffer_spec.is_binding);
			BufferData &bd = buffer_datas[buffer_spec.data_index];
			ZN_ASSERT(bd.capacity >= buffer_size);
			buffer.data = bd.data;
		} else {
			ZN_ASSERT(buffer_spec.is_binding || buffer_spec.is_constant);
			buffer.data = nullptr;
		}

		buffer.is_binding = buffer_spec.is_binding;
		buffer.is_constant = buffer_spec.is_constant;
		buffer.size = buffer_size;
		buffer.buffer_data_index = buffer_spec.data_index;

		// Always reset constants because we don't know if we'll run the same program as before...
		if (buffer_spec.is_constant) {
			buffer.constant_value = buffer_spec.constant_value;
			// Data can be null if it was determined that the nodes using this port don't require a buffer.
			if (buffer.data != nullptr) {
				for (unsigned int i = 0; i < buffer_size; ++i) {
					buffer.data[i] = buffer_spec.constant_value;
				}
			}
			ranges[buffer_spec.address] = math::Interval::from_single_value(buffer_spec.constant_value);
		}
	}

	// Puts sentinel values in buffers to detect non-initialized ones
	// #ifdef DEBUG_ENABLED
	// 	for (unsigned int i = 0; i < state.buffers.size(); ++i) {
	// 		Buffer &buffer = state.buffers[i];
	// 		if (!buffer.is_constant && !buffer.is_binding) {
	// 			ZN_ASSERT(buffer.data != nullptr);
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

void Runtime::generate_set(
		State &state, Span<Span<float>> p_inputs, bool skip_outer_group, const ExecutionMap *p_execution_map) const {
	// I don't like putting private helper functions in headers.
	struct L {
		static inline void bind_buffer(Span<Buffer> buffers, int a, Span<float> d) {
			Buffer &buffer = buffers[a];
			ZN_ASSERT(buffer.is_binding);
			buffer.data = d.data();
			buffer.size = d.size();
		}

		static inline void unbind_buffer(Span<Buffer> buffers, int a) {
			Buffer &buffer = buffers[a];
			ZN_ASSERT(buffer.is_binding);
			buffer.data = nullptr;
		}
	};

	ZN_PROFILE_SCOPE();

	ZN_ASSERT_RETURN(p_inputs.size() == _program.inputs.size());

#ifdef DEBUG_ENABLED
	// Each array must have the same size
	for (unsigned int i = 1; i < p_inputs.size(); ++i) {
		ZN_ASSERT(p_inputs[0].size() == p_inputs[i].size());
	}
#endif

#ifdef TOOLS_ENABLED
	ZN_ASSERT_RETURN(state.buffers.size() >= _program.buffer_count);
	ZN_ASSERT_RETURN(state.buffers.size() != 0);
	const unsigned int buffer_size = p_inputs.size() > 0 ? p_inputs[0].size() : state.buffer_size;
	ZN_ASSERT_RETURN(state.buffer_size >= buffer_size);
	ZN_ASSERT_RETURN(state.buffers[0].size >= buffer_size);
#ifdef DEBUG_ENABLED
	for (size_t i = 0; i < state.buffers.size(); ++i) {
		const Buffer &b = state.buffers[i];
		ZN_ASSERT(b.size >= buffer_size);
		ZN_ASSERT(b.size <= state.buffer_capacity);
		ZN_ASSERT(b.size == state.buffer_size);
		if (b.data != nullptr && !b.is_binding) {
			ZN_ASSERT(b.buffer_data_index < state.buffer_datas.size());
			const BufferData &bd = state.buffer_datas[b.buffer_data_index];
			ZN_ASSERT(b.size <= bd.capacity);
		}
	}
#endif
#endif

	Span<Buffer> buffers = to_span(state.buffers);

	// Bind inputs
	for (unsigned int i = 0; i < p_inputs.size(); ++i) {
		L::bind_buffer(buffers, _program.inputs[i].buffer_address, p_inputs[i]);
	}

	const Span<const uint16_t> operations(_program.operations.data(), 0, _program.operations.size());

	const ExecutionMap &execution_map = p_execution_map != nullptr ? *p_execution_map : _program.default_execution_map;
	Span<const ExecutionMap::OperationInfo> operation_infos = to_span(execution_map.operations);
	const Span<const ExecutionMap::ConstantFill> constant_fills = to_span(execution_map.constant_fills);

	if (skip_outer_group && operation_infos.size() > 0) {
		const unsigned int offset = execution_map.inner_group_start_index;
		operation_infos = operation_infos.sub(offset);
	}

#ifdef TOOLS_ENABLED
	ProfilingClock profiling_clock;
	const bool profile = state.debug_profiler_times.size() > 0;
#endif

	unsigned int constant_fill_index = 0;

	for (unsigned int execution_map_index = 0; execution_map_index < operation_infos.size(); ++execution_map_index) {
		const ExecutionMap::OperationInfo op_info = operation_infos[execution_map_index];

		for (unsigned int i = 0; i < op_info.constant_fill_count; ++i) {
			const ExecutionMap::ConstantFill &cf = constant_fills[constant_fill_index];
			ZN_ASSERT(cf.data != nullptr);
			for (unsigned int j = 0; j < state.buffer_size; ++j) {
				cf.data[j] = cf.value;
			}
			++constant_fill_index;
		}

		unsigned int pc = op_info.address;

		const uint16_t opid = operations[pc++];
		const NodeType &node_type = NodeTypeDB::get_singleton().get_type(opid);

		const uint32_t inputs_count = node_type.inputs.size();
		const uint32_t outputs_count = node_type.outputs.size();

		const Span<const uint16_t> op_inputs = operations.sub(pc, inputs_count);
		pc += inputs_count;
		const Span<const uint16_t> op_outputs = operations.sub(pc, outputs_count);
		pc += outputs_count;

		Span<const uint8_t> op_params = read_params(operations, pc);

		// TODO Buffers will stay bound if this error occurs!
		ZN_ASSERT_RETURN(node_type.process_buffer_func != nullptr);
		ProcessBufferContext ctx(op_inputs, op_outputs, op_params, buffers, p_execution_map != nullptr);
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
	for (unsigned int i = 0; i < p_inputs.size(); ++i) {
		L::unbind_buffer(buffers, _program.inputs[i].buffer_address);
	}
}

void Runtime::analyze_range(State &state, Span<math::Interval> p_inputs) const {
	ZN_PROFILE_SCOPE();

#ifdef TOOLS_ENABLED
	ERR_FAIL_COND(state.ranges.size() != _program.buffer_count);
#endif

	Span<math::Interval> ranges = to_span(state.ranges);
	Span<Buffer> buffers = to_span(state.buffers);

	// Reset users count, as they might be decreased during the analysis
	for (auto it = _program.buffer_specs.cbegin(); it != _program.buffer_specs.cend(); ++it) {
		const BufferSpec &bs = *it;
		Buffer &b = buffers[bs.address];
		b.local_users_count = bs.users_count;
	}

	for (unsigned int i = 0; i < p_inputs.size(); ++i) {
		const unsigned int bi = _program.inputs[i].buffer_address;
		ranges[bi] = p_inputs[i];
	}

	const Span<const uint16_t> operations(_program.operations.data(), 0, _program.operations.size());

	// Here operations must all be analyzed, because we do this as a broad-phase.
	// Only narrow-phase may skip some operations eventually.
	uint32_t pc = 0;
	while (pc < operations.size()) {
		const uint16_t opid = operations[pc++];
		const NodeType &node_type = NodeTypeDB::get_singleton().get_type(opid);

		const uint32_t inputs_count = node_type.inputs.size();
		const uint32_t outputs_count = node_type.outputs.size();

		const Span<const uint16_t> op_inputs = operations.sub(pc, inputs_count);
		pc += inputs_count;
		const Span<const uint16_t> op_outputs = operations.sub(pc, outputs_count);
		pc += outputs_count;

		Span<const uint8_t> op_params = read_params(operations, pc);

		ZN_ASSERT_RETURN(node_type.range_analysis_func != nullptr);
		RangeAnalysisContext ctx(op_inputs, op_outputs, op_params, ranges, buffers);
		node_type.range_analysis_func(ctx);

#ifdef VOXEL_DEBUG_GRAPH_PROG_SENTINEL
		// If this fails, the program is ill-formed
		ZN_ASSERT(read<uint16_t>(_program, pc) == VOXEL_DEBUG_GRAPH_PROG_SENTINEL);
#endif
	}
}

#ifdef DEBUG_ENABLED

void Runtime::debug_print_operations() {
	const Span<const uint16_t> operations(_program.operations.data(), 0, _program.operations.size());

	std::stringstream ss;
	unsigned int op_index = 0;
	uint32_t pc = 0;
	while (pc < operations.size()) {
		const uint16_t opid = operations[pc++];
		const NodeType &node_type = NodeTypeDB::get_singleton().get_type(opid);

		const uint32_t inputs_count = node_type.inputs.size();
		const uint32_t outputs_count = node_type.outputs.size();

		const Span<const uint16_t> inputs = operations.sub(pc, inputs_count);
		pc += inputs_count;
		const Span<const uint16_t> outputs = operations.sub(pc, outputs_count);
		pc += outputs_count;

		/*Span<const uint8_t> params = */ read_params(operations, pc);

		ss << "[";
		ss << op_index;
		ss << "] op: ";
		ss << node_type.name;
		ss << " in(";
		for (size_t i = 0; i < inputs.size(); ++i) {
			if (i > 0) {
				ss << ", ";
			}
			ss << int(inputs[i]);
		}
		ss << ") out(";
		for (size_t i = 0; i < outputs.size(); ++i) {
			if (i > 0) {
				ss << ", ";
			}
			ss << int(outputs[i]);
		}
		ss << ")\n";

		++op_index;
	}

	println(ss.str());
}

#endif

bool Runtime::try_get_output_port_address(ProgramGraph::PortLocation port, uint16_t &out_address) const {
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

} // namespace zylann::voxel::pg
