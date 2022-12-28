#ifndef VOXEL_GRAPH_RUNTIME_H
#define VOXEL_GRAPH_RUNTIME_H

#include "../../util/godot/classes/ref_counted.h"
#include "../../util/math/interval.h"
#include "../../util/math/vector3f.h"
#include "../../util/math/vector3i.h"
#include "../../util/span.h"
#include "program_graph.h"

namespace zylann::voxel::pg {

class VoxelGraphFunction;
class NodeTypeDB;

struct CompilationResult {
	bool success = false;
	int node_id = -1;
	int expanded_nodes_count = 0; // For testing and debugging
	String message;

	static CompilationResult make_success() {
		CompilationResult res;
		res.success = true;
		return res;
	}

	static CompilationResult make_error(const char *p_message, int p_node_id = -1) {
		CompilationResult res;
		res.success = false;
		res.node_id = p_node_id;
		res.message = p_message;
		return res;
	}
};

// CPU VM to execute a voxel graph generator.
// This is a more generic class implementing the core of a 3D expression processing system.
// Some of the logic dedicated to voxel data is moved in other classes.
class Runtime {
public:
	static const unsigned int MAX_INPUTS = 8;
	static const unsigned int MAX_OUTPUTS = 24;

	struct BufferData {
		// Owns the data.
		float *data = nullptr;
		unsigned int capacity = 0;
	};

	// Contains values of a node output
	struct Buffer {
		// Access to buffer data associated with this port. Must contain at least `size` values.
		// This data is not owned by the port. Multiple ports can use the same buffer.
		// TODO Consider wrapping this in debug mode. It is one of the rare cases I didnt do it.
		// I spent an hour debugging memory corruption which originated from an overrun while accessing this data.
		float *data = nullptr;
		// This size is not the allocated count, it's an available count below capacity.
		// All buffers have the same available count, size is here only for convenience.
		unsigned int size;
		// unsigned int capacity;
		// Constant value of the buffer, if it is a compile-time constant
		float constant_value;
		// Is the buffer holding a compile-time constant
		bool is_constant;
		// Is the buffer a user input/output
		bool is_binding = false;
		// How many operations are using this buffer as input.
		// This value is only relevant when using optimized execution mapping.
		uint16_t local_users_count;
		// Index of the data in the pool of BufferData. This is mainly used for debugging.
		uint16_t buffer_data_index;
	};

	// Contains a list of adresses to the operations to execute for a given query.
	// If no local optimization is done, this can remain the same for any position lists.
	// If local optimization is used, it may be recomputed before each query.
	struct ExecutionMap {
		struct OperationInfo {
			uint16_t address = 0;
			// How many constant fills to execute before this operation.
			uint16_t constant_fill_count = 0;
		};

		std::vector<OperationInfo> operations;

		// Stores node IDs referring to the user-facing graph.
		// Each index corresponds to operation indices.
		// The same node can appear twice, because sometimes a user-facing node compiles as multiple nodes.
		// It can also include some nodes not explicitely present in the user graph (like auto-inputs).
		std::vector<uint32_t> debug_nodes;

		// Every operation before this index in the `operations` list will only depend on inputs tagged as "outer
		// group". This is the index from which operations won't only depend on the outer group.
		unsigned int inner_group_start_index = 0;

		struct ConstantFill {
			float *data = nullptr;
			float value = 0;
		};

		// List of buffers to fill with constants when using local optimization.
		// This list must be read using an advancing cursor, that moves up by the amount specified in `OperationInfo`.
		// Note, it is preferable to use this for dynamic optimizations. Compile-time constants should use pinned
		// buffers, or better, single values.
		std::vector<ConstantFill> constant_fills;

		void clear() {
			operations.clear();
			debug_nodes.clear();
			inner_group_start_index = 0;
			constant_fills.clear();
		}
	};

	// Contains the data the program will modify while it runs.
	// The same state can be re-used with multiple programs, but it should be prepared before doing that.
	class State {
	public:
		~State() {
			clear();
		}

		inline const Buffer &get_buffer(uint16_t address) const {
			// TODO Just for convenience because STL bound checks aren't working in Godot 3
			CRASH_COND(address >= buffers.size());
			return buffers[address];
		}

		inline const math::Interval get_range(uint16_t address) const {
			// TODO Just for convenience because STL bound checks aren't working in Godot 3
			CRASH_COND(address >= buffers.size());
			return ranges[address];
		}

		void clear() {
			buffer_size = 0;
			// buffer_capacity = 0;
			for (BufferData &bd : buffer_datas) {
				ZN_ASSERT(bd.data != nullptr);
				memfree(bd.data);
			}
			buffer_datas.clear();
			buffers.clear();
			ranges.clear();
			debug_profiler_times.clear();
		}

		inline void add_execution_time(uint32_t execution_map_index, uint32_t time) {
#if DEBUG_ENABLED
			CRASH_COND(execution_map_index >= debug_profiler_times.size());
#endif
			debug_profiler_times[execution_map_index] += time;
		}

		inline uint32_t get_execution_time(uint32_t execution_map_index) const {
#if DEBUG_ENABLED
			CRASH_COND(execution_map_index >= debug_profiler_times.size());
#endif
			return debug_profiler_times[execution_map_index];
		}

	private:
		friend class Runtime; // TODO Why is friend needed? This class is nested inside

		std::vector<math::Interval> ranges;
		std::vector<Buffer> buffers;
		std::vector<BufferData> buffer_datas;
		// [execution_map_index] => microseconds
		std::vector<uint32_t> debug_profiler_times;

		unsigned int buffer_size = 0;
		unsigned int buffer_capacity = 0;
	};

	struct InputInfo {
		// Note: the following buffer are allocated by the user.
		// They are mapped temporarily into the same array of buffers inside `State`,
		// so we won't need specific code to handle them. This requires knowing at which index they are reserved.
		// They must be all assigned for the program to run correctly.
		unsigned int buffer_address = 0;
	};

	// Info about a terminal node of the graph
	struct OutputInfo {
		unsigned int buffer_address;
		unsigned int dependency_graph_node_index;
		unsigned int node_id;
	};

	Runtime();
	~Runtime();

	void clear();
	CompilationResult compile(const VoxelGraphFunction &function, bool debug);

	// Call this before you use a state with generation functions.
	// You need to call it once, until you want to use a different graph, buffer size or buffer count.
	// If none of these change, you can keep re-using it.
	void prepare_state(State &state, unsigned int buffer_size, bool with_profiling) const;

	// Convenience for set generation with only one value
	// TODO Evaluate needs for double-precision in pg::Runtime
	void generate_single(State &state, Span<float> inputs, const ExecutionMap *execution_map) const;

	void generate_set(
			State &state, Span<Span<float>> p_inputs, bool skip_outer_group, const ExecutionMap *p_execution_map) const;

#ifdef DEBUG_ENABLED
	void debug_print_operations();
#endif

	// bool has_input(unsigned int node_type) const;

	inline unsigned int get_input_count() const {
		return _program.inputs.size();
	}

	inline unsigned int get_output_count() const {
		return _program.outputs_count;
	}

	inline const OutputInfo &get_output_info(unsigned int i) const {
		return _program.outputs[i];
	}

	// Analyzes a specific region of inputs to find out what ranges of outputs we can expect.
	// It can be used to speed up calls to `generate_set` thanks to execution mapping,
	// so that operations can be optimized out if they don't contribute to the result.
	void analyze_range(State &state, Span<math::Interval> p_inputs) const;

	// Call this after `analyze_range` if you intend to actually generate a set or single values in the area.
	// This allows to use the execution map optimization, until you choose another area.
	// (i.e when using this, querying values outside of the analyzed area may be invalid)
	void generate_optimized_execution_map(const State &state, ExecutionMap &execution_map,
			Span<const unsigned int> required_outputs, bool debug) const;

	// Convenience function to require all outputs
	void generate_optimized_execution_map(const State &state, ExecutionMap &execution_map, bool debug) const;

	const ExecutionMap &get_default_execution_map() const;

	// Gets the buffer address of a specific output port
	bool try_get_output_port_address(ProgramGraph::PortLocation port, uint16_t &out_address) const;

	uint64_t get_program_hash() const;

	struct HeapResource {
		void *ptr;
		void (*deleter)(void *p);
	};

	class _ProcessContext {
	public:
		inline _ProcessContext(const Span<const uint16_t> inputs, const Span<const uint16_t> outputs,
				const Span<const uint8_t> params) :
				_inputs(inputs), _outputs(outputs), _params(params) {}

		template <typename T>
		inline const T &get_params() const {
#ifdef DEBUG_ENABLED
			CRASH_COND(sizeof(T) > _params.size());
#endif
			return *reinterpret_cast<const T *>(_params.data());
		}

		inline uint32_t get_input_address(uint32_t i) const {
			return _inputs[i];
		}

	protected:
		inline uint32_t get_output_address(uint32_t i) const {
			return _outputs[i];
		}

	private:
		const Span<const uint16_t> _inputs;
		const Span<const uint16_t> _outputs;
		const Span<const uint8_t> _params;
	};

	// Functions usable by node implementations during execution
	class ProcessBufferContext : public _ProcessContext {
	public:
		inline ProcessBufferContext(const Span<const uint16_t> inputs, const Span<const uint16_t> outputs,
				const Span<const uint8_t> params, Span<Buffer> buffers, bool using_execution_map) :
				_ProcessContext(inputs, outputs, params),
				_buffers(buffers),
				_using_execution_map(using_execution_map) {}

		inline const Buffer &get_input(uint32_t i) const {
			const uint32_t address = get_input_address(i);
#ifdef DEBUG_ENABLED
			// When using optimized execution mapping,
			// If a buffer is marked as having no users during range analysis, then it should really not be used,
			// because it won't be filled with relevant data. If it is still used,
			// then the result can be completely different from what the range analysis predicted.
			const Buffer &b = _buffers[address];
			ERR_FAIL_COND_V_MSG(_using_execution_map && !b.is_binding && b.local_users_count == 0, b,
					"buffer marked as 'ignored' is still being used");
#endif
			return _buffers[address];
		}

		inline Buffer &get_output(uint32_t i) {
			const uint32_t address = get_output_address(i);
			return _buffers[address];
		}

		// Different signature to force the coder to acknowledge the condition
		inline const Buffer &try_get_input(uint32_t i, bool &ignored) {
			const uint32_t address = get_input_address(i);
			const Buffer &b = _buffers[address];
			ignored = _using_execution_map && !b.is_binding && b.local_users_count == 0;
			return b;
		}

	private:
		Span<Buffer> _buffers;
		bool _using_execution_map;
	};

	// Functions usable by node implementations during range analysis
	class RangeAnalysisContext : public _ProcessContext {
	public:
		inline RangeAnalysisContext(const Span<const uint16_t> inputs, const Span<const uint16_t> outputs,
				const Span<const uint8_t> params, Span<math::Interval> ranges, Span<Buffer> buffers) :
				_ProcessContext(inputs, outputs, params), _ranges(ranges), _buffers(buffers) {}

		inline const math::Interval get_input(uint32_t i) const {
			const uint32_t address = get_input_address(i);
			return _ranges[address];
		}

		inline void set_output(uint32_t i, const math::Interval r) {
			const uint32_t address = get_output_address(i);
			_ranges[address] = r;
		}

		inline void ignore_input(uint32_t i) {
			const uint32_t address = get_input_address(i);
			Buffer &b = _buffers[address];
			--b.local_users_count;
		}

	private:
		Span<math::Interval> _ranges;
		Span<Buffer> _buffers;
	};

	typedef void (*ProcessBufferFunc)(ProcessBufferContext &);
	typedef void (*RangeAnalysisFunc)(RangeAnalysisContext &);

private:
	struct Program;

	static CompilationResult compile_preprocessed_graph(Program &program, const ProgramGraph &graph,
			unsigned int input_count, Span<const uint32_t> input_node_ids, bool debug, const NodeTypeDB &type_db);

	bool is_operation_constant(const State &state, uint16_t op_address) const;

	struct BufferSpec {
		// Index the buffer should be stored at
		uint16_t address = 0;
		// Index where the data of the buffer should be stored at.
		// This can be the same for multiple buffer, unless pinned.
		uint16_t data_index = 0;
		// How many nodes use this buffer as input
		uint16_t users_count = 0;
		// Value of the compile-time constant, if any
		float constant_value = 0;
		// Is the buffer constant at compile time
		bool is_constant = false;
		// Is the buffer a user input/output
		bool is_binding = false;
		// Will be `true` only if `data_index` actually refers to something.
		// Buffers without data are bindings or constants not requiring buffers.
		bool has_data = false;
		// If true, the port will be assigned unique buffer data.
		// If false, the port might share the same buffer data with other ports.
		// TODO Rename `has_unique_data`?
		bool is_pinned = false;
	};

	// Pre-processed, read-only graph used for runtime optimizations.
	struct DependencyGraph {
		struct Node {
			uint16_t first_dependency;
			uint16_t end_dependency;
			uint16_t op_address;
			bool is_input;
			// Node ID from the expanded ProgramGraph (non user-provided, so may need remap)
			uint32_t debug_node_id;
		};

		// Indexes to the `nodes` array
		std::vector<uint16_t> dependencies;
		// Nodes in the same order they would be in the default execution map (but indexes may not match)
		std::vector<Node> nodes;

		inline void clear() {
			dependencies.clear();
			nodes.clear();
		}
	};

	// Compiled program data.
	// Remains constant and read-only after compilation.
	struct Program {
		// Serialized operations and arguments, aligned at minimum with uint16.
		// They come up as series of:
		//
		// - uint16 opid
		// - uint16 inputs[0..*]
		// - uint16 outputs[0..*]
		// - uint16 parameters_size
		// - uint16 parameters_offset // how much to advance from here to reach the beginning of `parameters`
		// - <optional padding>
		// - T parameters, where T could be any struct
		// - <optional padding to keep alignment with uint16>
		//
		// They should be laid out in the same order they will be run in, although it's not absolutely required.
		// It's better to have it ordered because memory access will be more predictable.
		std::vector<uint16_t> operations;

		// Describes dependencies between operations. It is generated at compile time.
		// It is used to perform dynamic optimization in case some operations can be predicted as constant.
		DependencyGraph dependency_graph;

		// List of indexes within `operations` describing which order they should be run into by default.
		// It's used because sometimes we may want to override with a simplified execution map dynamically.
		// When we don't, we use the default one so the code doesn't have to change.
		ExecutionMap default_execution_map;

		// Heap-allocated parameters data, when too large to fit in `operations`.
		// We keep a reference to them so they can be freed when the program is cleared.
		std::vector<HeapResource> heap_resources;

		// Heap-allocated parameters data, when too large to fit in `operations`.
		// We keep a reference to them so they won't be freed until the program is cleared.
		std::vector<Ref<RefCounted>> ref_resources;

		// Describes the list of buffers to prepare in `State` before the program can be run
		std::vector<BufferSpec> buffer_specs;

		// Address in `operations` from which operations will start to not only depend on inputs tagged as "outer
		// group". It is used to optimize away calculations that would otherwise be the same in planar terrain use
		// cases.
		uint32_t inner_group_start_op_index;

		std::vector<InputInfo> inputs;

		FixedArray<OutputInfo, MAX_OUTPUTS> outputs;
		unsigned int outputs_count = 0;

		// Maximum amount of buffers this program will need to do a full run.
		// Buffers are needed to hold values of arguments and outputs for each operation.
		unsigned int buffer_count = 0;
		// Maximum amount of buffer datas this program will need to do a full run.
		unsigned int buffer_data_count = 0;

		// Associates a port from the expanded graph to its corresponding address within the compiled program.
		// This is used for debugging intermediate values.
		std::unordered_map<ProgramGraph::PortLocation, uint16_t> output_port_addresses;

		// If you have a port location from the original user graph, before querying `output_port_addresses`, remap
		// it first, in case it got expanded to different nodes during compilation.
		std::unordered_map<ProgramGraph::PortLocation, ProgramGraph::PortLocation> user_port_to_expanded_port;

		// Associates expanded graph ID to user graph node IDs.
		std::unordered_map<uint32_t, uint32_t> expanded_node_id_to_user_node_id;

		// Result of the last compilation attempt. The program should not be run if it failed.
		CompilationResult compilation_result;

		void clear() {
			operations.clear();
			buffer_specs.clear();
			inner_group_start_op_index = 0;
			default_execution_map.clear();
			output_port_addresses.clear();
			user_port_to_expanded_port.clear();
			expanded_node_id_to_user_node_id.clear();
			dependency_graph.clear();
			inputs.clear();
			outputs_count = 0;
			compilation_result = CompilationResult();
			for (auto it = heap_resources.begin(); it != heap_resources.end(); ++it) {
				HeapResource &r = *it;
				CRASH_COND(r.deleter == nullptr);
				CRASH_COND(r.ptr == nullptr);
				r.deleter(r.ptr);
			}
			heap_resources.clear();
			ref_resources.clear();
			buffer_count = 0;
			buffer_data_count = 0;
		}
	};

	Program _program;
};

} // namespace zylann::voxel::pg

#endif // VOXEL_GRAPH_RUNTIME_H
