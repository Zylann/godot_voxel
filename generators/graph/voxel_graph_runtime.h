#ifndef VOXEL_GRAPH_RUNTIME_H
#define VOXEL_GRAPH_RUNTIME_H

#include "../../util/math/interval.h"
#include "../../util/math/vector3i.h"
#include "../../util/span.h"
#include "program_graph.h"

#include <core/reference.h>

// CPU VM to execute a voxel graph generator.
// This is a more generic class implementing the core of a 3D expression processing system.
// Some of the logic dedicated to voxel data is moved in other classes.
class VoxelGraphRuntime {
public:
	static const unsigned int MAX_OUTPUTS = 24;

	struct CompilationResult {
		bool success = false;
		int node_id = -1;
		String message;
	};

	// Contains values of a node output
	struct Buffer {
		// Values of the buffer. Must contain at least `size` values.
		// TODO Consider wrapping this in debug mode. It is one of the rare cases I didnt do it.
		// I spent an hour debugging memory corruption which originated from an overrun while accessing this data.
		float *data = nullptr;
		// This size is not the allocated count, it's an available count below capacity.
		// All buffers have the same available count, size is here only for convenience.
		unsigned int size;
		unsigned int capacity;
		// Constant value of the buffer, if it is a compile-time constant
		float constant_value;
		// Is the buffer holding a compile-time constant
		bool is_constant;
		// Is the buffer a user input/output
		bool is_binding = false;
		// How many operations are using this buffer as input.
		// This value is only relevant when using optimized execution mapping.
		unsigned int local_users_count;
	};

	// Contains a list of adresses to the operations to execute for a given query.
	// If no local optimization is done, this can remain the same for any position lists.
	// If local optimization is used, it may be recomputed before each query.
	struct ExecutionMap {
		// TODO Typo?
		std::vector<uint16_t> operation_adresses;
		// Stores node IDs referring to the user-facing graph
		std::vector<int> debug_nodes;
		// From which index in the adress list operations will start depending on Y
		unsigned int xzy_start_index = 0;

		void clear() {
			operation_adresses.clear();
			debug_nodes.clear();
			xzy_start_index = 0;
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

		inline const Interval get_range(uint16_t address) const {
			// TODO Just for convenience because STL bound checks aren't working in Godot 3
			CRASH_COND(address >= buffers.size());
			return ranges[address];
		}

		void clear() {
			buffer_size = 0;
			buffer_capacity = 0;
			for (auto it = buffers.begin(); it != buffers.end(); ++it) {
				Buffer &b = *it;
				if (b.data != nullptr && !b.is_binding) {
					memfree(b.data);
				}
			}
			buffers.clear();
			ranges.clear();
		}

	private:
		friend class VoxelGraphRuntime;

		std::vector<Interval> ranges;
		std::vector<Buffer> buffers;

		unsigned int buffer_size = 0;
		unsigned int buffer_capacity = 0;
	};

	// Info about a terminal node of the graph
	struct OutputInfo {
		unsigned int buffer_address;
		unsigned int dependency_graph_node_index;
		unsigned int node_id;
	};

	VoxelGraphRuntime();
	~VoxelGraphRuntime();

	void clear();
	CompilationResult compile(const ProgramGraph &graph, bool debug);

	// Call this before you use a state with generation functions.
	// You need to call it once, until you want to use a different graph, buffer size or buffer count.
	// If none of these change, you can keep re-using it.
	void prepare_state(State &state, unsigned int buffer_size) const;

	// Convenience for set generation with only one value
	void generate_single(State &state, Vector3 position, const ExecutionMap *execution_map) const;

	void generate_set(State &state, Span<float> in_x, Span<float> in_y, Span<float> in_z,
			bool skip_xz, const ExecutionMap *execution_map) const;

	inline unsigned int get_output_count() const {
		return _program.outputs_count;
	}

	inline const OutputInfo &get_output_info(unsigned int i) const {
		return _program.outputs[i];
	}

	// Analyzes a specific region of inputs to find out what ranges of outputs we can expect.
	// It can be used to speed up calls to `generate_set` thanks to execution mapping,
	// so that operations can be optimized out if they don't contribute to the result.
	void analyze_range(State &state, Vector3i min_pos, Vector3i max_pos) const;

	// Call this after `analyze_range` if you intend to actually generate a set or single values in the area.
	// This allows to use the execution map optimization, until you choose another area.
	// (i.e when using this, querying values outside of the analyzed area may be invalid)
	void generate_optimized_execution_map(const State &state, ExecutionMap &execution_map,
			Span<const unsigned int> required_outputs, bool debug) const;

	// Convenience function to require all outputs
	void generate_optimized_execution_map(const State &state, ExecutionMap &execution_map, bool debug) const;

	// Gets the buffer address of a specific output port
	bool try_get_output_port_address(ProgramGraph::PortLocation port, uint16_t &out_address) const;

	struct HeapResource {
		void *ptr;
		void (*deleter)(void *p);
	};

	// Functions usable by node implementations during the compilation stage
	class CompileContext {
	public:
		CompileContext(const ProgramGraph::Node &node, std::vector<uint16_t> &program,
				std::vector<HeapResource> &heap_resources,
				std::vector<Variant> &params) :
				_node(node),
				_program(program),
				_heap_resources(heap_resources),
				_params(params) {}

		Variant get_param(size_t i) const {
			CRASH_COND(i > _params.size());
			return _params[i];
		}

		// Typical use is to pass a struct containing all compile-time arguments the operation will need
		template <typename T>
		void set_params(T params) {
			// Can be called only once per node
			CRASH_COND(_params_added);
			// We will need to align memory, so the struct will not be immediately stored here.
			// Instead we put a header that tells how much to advance in order to reach the beginning of the struct,
			// which will be at an aligned position.
			// We align to the maximum alignment between the struct,
			// and the type of word we store inside the program buffer, which is uint16.
			const size_t params_alignment = max(alignof(T), alignof(uint16_t));
			const size_t params_offset_index = _program.size();
			// Prepare space to store the offset (at least 1 since that header is one word)
			_program.push_back(1);
			// Align memory for the struct.
			// Note, we index with words, not bytes.
			const size_t struct_offset =
					alignup(_program.size() * sizeof(uint16_t), params_alignment) / sizeof(uint16_t);
			if (struct_offset > _program.size()) {
				_program.resize(struct_offset);
			}
			// Write offset in header
			_program[params_offset_index] = struct_offset - params_offset_index;
			// Allocate space for the struct. It is measured in words, so it can be up to 1 byte larger.
			_params_size_in_words = (sizeof(T) + sizeof(uint16_t) - 1) / sizeof(uint16_t);
			_program.resize(_program.size() + _params_size_in_words);
			// Write struct
			T &p = *reinterpret_cast<T *>(&_program[struct_offset]);
			p = params;

			_params_added = true;
		}

		// In case the compilation step produces a resource to be deleted
		template <typename T>
		void add_memdelete_cleanup(T *ptr) {
			HeapResource hr;
			hr.ptr = ptr;
			hr.deleter = [](void *p) {
				// TODO We have no guarantee it was allocated with memnew :|
				T *tp = reinterpret_cast<T *>(p);
				memdelete(tp);
			};
			_heap_resources.push_back(hr);
		}

		void make_error(String message) {
			_error_message = message;
			_has_error = true;
		}

		bool has_error() const {
			return _has_error;
		}

		const String &get_error_message() const {
			return _error_message;
		}

		size_t get_params_size_in_words() const {
			return _params_size_in_words;
		}

	private:
		const ProgramGraph::Node &_node;
		std::vector<uint16_t> &_program;
		std::vector<HeapResource> &_heap_resources;
		std::vector<Variant> &_params;
		String _error_message;
		size_t _params_size_in_words = 0;
		bool _has_error = false;
		bool _params_added = false;
	};

	class _ProcessContext {
	public:
		inline _ProcessContext(
				const Span<const uint16_t> inputs,
				const Span<const uint16_t> outputs,
				const Span<const uint8_t> params) :
				_inputs(inputs),
				_outputs(outputs),
				_params(params) {}

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
		inline ProcessBufferContext(
				const Span<const uint16_t> inputs,
				const Span<const uint16_t> outputs,
				const Span<const uint8_t> params,
				Span<Buffer> buffers,
				bool using_execution_map) :
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
		inline RangeAnalysisContext(
				const Span<const uint16_t> inputs,
				const Span<const uint16_t> outputs,
				const Span<const uint8_t> params,
				Span<Interval> ranges,
				Span<Buffer> buffers) :
				_ProcessContext(inputs, outputs, params),
				_ranges(ranges),
				_buffers(buffers) {}

		inline const Interval get_input(uint32_t i) const {
			const uint32_t address = get_input_address(i);
			return _ranges[address];
		}

		inline void set_output(uint32_t i, const Interval r) {
			const uint32_t address = get_output_address(i);
			_ranges[address] = r;
		}

		inline void ignore_input(uint32_t i) {
			const uint32_t address = get_input_address(i);
			Buffer &b = _buffers[address];
			--b.local_users_count;
		}

	private:
		Span<Interval> _ranges;
		Span<Buffer> _buffers;
	};

	typedef void (*CompileFunc)(CompileContext &);
	typedef void (*ProcessBufferFunc)(ProcessBufferContext &);
	typedef void (*RangeAnalysisFunc)(RangeAnalysisContext &);

private:
	CompilationResult _compile(const ProgramGraph &graph, bool debug);

	bool is_operation_constant(const State &state, uint16_t op_address) const;

	struct BufferSpec {
		// Index the buffer should be stored at
		uint16_t address;
		// How many nodes use this buffer as input
		uint16_t users_count;
		// Value of the compile-time constant, if any
		float constant_value;
		// Is the buffer constant at compile time
		bool is_constant;
		// Is the buffer a user input/output
		bool is_binding;
	};

	struct DependencyGraph {
		struct Node {
			uint16_t first_dependency;
			uint16_t end_dependency;
			uint16_t op_address;
			bool is_input;
			int debug_node_id;
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

	// Precalculated program data.
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
		std::vector<Ref<Reference>> ref_resources;

		// Describes the list of buffers to prepare in `State` before the program can be run
		std::vector<BufferSpec> buffer_specs;

		// Address in `operations` from which operations will depend on Y. Operations before never depend on it.
		// It is used to optimize away calculations that would otherwise be the same in planar terrain use cases.
		uint32_t xzy_start_op_address;

		// Note: the following buffers are allocated by the user.
		// They are mapped temporarily into the same array of buffers inside `State`,
		// so we won't need specific code to handle them. This requires knowing at which index they are reserved.
		// They must be all assigned for the program to run correctly.
		//
		// Address within the State's array of buffers where the X input may be.
		int x_input_address = -1;
		// Address within the State's array of buffers where the Y input may be.
		int y_input_address = -1;
		// Address within the State's array of buffers where the Z input may be.
		int z_input_address = -1;

		FixedArray<OutputInfo, MAX_OUTPUTS> outputs;
		unsigned int outputs_count = 0;

		// Maximum amount of buffers this program will need to do a full run.
		// Buffers are needed to hold values of arguments and outputs for each operation.
		unsigned int buffer_count = 0;

		// Associates a high-level port to its corresponding address within the compiled program.
		// This is used for debugging intermediate values.
		HashMap<ProgramGraph::PortLocation, uint16_t, ProgramGraph::PortLocationHasher> output_port_addresses;

		// Result of the last compilation attempt. The program should not be run if it failed.
		CompilationResult compilation_result;

		void clear() {
			operations.clear();
			buffer_specs.clear();
			xzy_start_op_address = 0;
			default_execution_map.clear();
			output_port_addresses.clear();
			dependency_graph.clear();
			x_input_address = -1;
			y_input_address = -1;
			z_input_address = -1;
			outputs_count = 0;
			compilation_result = CompilationResult();
			for (auto it = heap_resources.begin(); it != heap_resources.end(); ++it) {
				HeapResource &r = *it;
				CRASH_COND(r.deleter == nullptr);
				CRASH_COND(r.ptr == nullptr);
				r.deleter(r.ptr);
			}
			heap_resources.clear();
			unlock_images();
			ref_resources.clear();
			buffer_count = 0;
		}

		void lock_images();
		void unlock_images();
	};

	Program _program;
};

#endif // VOXEL_GRAPH_RUNTIME_H
