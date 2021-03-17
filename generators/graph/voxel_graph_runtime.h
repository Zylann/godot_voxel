#ifndef VOXEL_GRAPH_RUNTIME_H
#define VOXEL_GRAPH_RUNTIME_H

#include "../../util/array_slice.h"
#include "../../util/math/interval.h"
#include "../../util/math/vector3i.h"
#include "program_graph.h"
#include <core/reference.h>

class ImageRangeGrid;

// CPU VM to execute a voxel graph generator
class VoxelGraphRuntime {
public:
	struct CompilationResult {
		bool success = false;
		int node_id = -1;
		String message;
	};

	struct Buffer {
		// TODO Consider wrapping this in debug mode. It is one of the rare cases I didnt do it.
		// I spent an hour debugging memory corruption which originated from an overrun while accessing this data.
		float *data = nullptr;
		// This size is not the allocated count, it's an available count below capacity.
		// All buffers have the same available count, size is here only for convenience.
		unsigned int size;
		unsigned int capacity;
		float constant_value;
		bool is_constant;
		bool is_binding = false;
	};

	// Contains the data the program will modify while it runs.
	// The same state can be re-used with multiple programs, but it should be prepared before doing that.
	class State {
	public:
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

	VoxelGraphRuntime();
	~VoxelGraphRuntime();

	void clear();
	CompilationResult compile(const ProgramGraph &graph, bool debug);

	// Call this before you use a state with generation functions.
	// You need to call it once, until you want to use a different graph, buffer size or buffer count.
	// If none of these change, you can keep re-using it.
	void prepare_state(State &state, unsigned int buffer_size) const;

	float generate_single(State &state, Vector3 position) const;

	void generate_set(State &state, ArraySlice<float> in_x, ArraySlice<float> in_y, ArraySlice<float> in_z,
			ArraySlice<float> out_sdf, bool skip_xz) const;

	Interval analyze_range(State &state, Vector3i min_pos, Vector3i max_pos) const;

	inline bool has_output() const {
		return _program.sdf_output_address != -1;
	}

	bool try_get_output_port_address(ProgramGraph::PortLocation port, uint16_t &out_address) const;

	struct HeapResource {
		void *ptr;
		void (*deleter)(void *p);
	};

	class CompileContext {
	public:
		CompileContext(const ProgramGraph::Node &node, std::vector<uint8_t> &program,
				std::vector<HeapResource> &heap_resources,
				std::vector<Variant> &params) :
				_node(node),
				_offset(program.size()),
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
			CRASH_COND(_offset != _program.size());
			_program.resize(_program.size() + sizeof(T));
			T &p = *reinterpret_cast<T *>(&_program[_offset]);
			p = params;
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

	private:
		const ProgramGraph::Node &_node;
		const size_t _offset;
		std::vector<uint8_t> &_program;
		std::vector<HeapResource> &_heap_resources;
		std::vector<Variant> &_params;
		String _error_message;
		bool _has_error = false;
	};

	class _ProcessContext {
	public:
		inline _ProcessContext(
				const ArraySlice<const uint16_t> inputs,
				const ArraySlice<const uint16_t> outputs,
				const ArraySlice<const uint8_t> params) :
				_inputs(inputs),
				_outputs(outputs),
				_params(params) {}

		template <typename T>
		inline const T &get_params() const {
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
		const ArraySlice<const uint16_t> _inputs;
		const ArraySlice<const uint16_t> _outputs;
		const ArraySlice<const uint8_t> _params;
	};

	class ProcessBufferContext : public _ProcessContext {
	public:
		inline ProcessBufferContext(
				const ArraySlice<const uint16_t> inputs,
				const ArraySlice<const uint16_t> outputs,
				const ArraySlice<const uint8_t> params,
				ArraySlice<Buffer> buffers) :
				_ProcessContext(inputs, outputs, params),
				_buffers(buffers) {}

		inline const Buffer &get_input(uint32_t i) const {
			const uint32_t address = get_input_address(i);
			return _buffers[address];
		}

		inline Buffer &get_output(uint32_t i) {
			const uint32_t address = get_output_address(i);
			return _buffers[address];
		}

	private:
		ArraySlice<Buffer> _buffers;
	};

	class RangeAnalysisContext : public _ProcessContext {
	public:
		inline RangeAnalysisContext(
				const ArraySlice<const uint16_t> inputs,
				const ArraySlice<const uint16_t> outputs,
				const ArraySlice<const uint8_t> params,
				ArraySlice<Interval> ranges) :
				_ProcessContext(inputs, outputs, params),
				_ranges(ranges) {}

		inline const Interval get_input(uint32_t i) const {
			const uint32_t address = get_input_address(i);
			return _ranges[address];
		}

		inline void set_output(uint32_t i, const Interval r) {
			const uint32_t address = get_output_address(i);
			_ranges[address] = r;
		}

	private:
		ArraySlice<Interval> _ranges;
	};

	typedef void (*CompileFunc)(CompileContext &);
	typedef void (*ProcessBufferFunc)(ProcessBufferContext &);
	typedef void (*RangeAnalysisFunc)(RangeAnalysisContext &);

private:
	CompilationResult _compile(const ProgramGraph &graph, bool debug);

	struct Constant {
		unsigned int address;
		float value = 0;
	};

	// Remains constant and read-only after compilation.
	struct Program {
		std::vector<uint8_t> operations;
		std::vector<HeapResource> heap_resources;
		std::vector<Ref<Reference> > ref_resources;
		std::vector<Constant> constants;
		std::vector<uint16_t> bindings;
		uint32_t xzy_start;
		int x_input_address = -1;
		int y_input_address = -1;
		int z_input_address = -1;
		int sdf_output_address = -1;
		unsigned int buffer_count = 0;
		HashMap<ProgramGraph::PortLocation, uint16_t, ProgramGraph::PortLocationHasher> output_port_addresses;
		CompilationResult compilation_result;

		void clear() {
			operations.clear();
			constants.clear();
			bindings.clear();
			// Address in the program from which operations will depend on Y.
			xzy_start = 0;
			output_port_addresses.clear();
			sdf_output_address = -1;
			x_input_address = -1;
			y_input_address = -1;
			z_input_address = -1;
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
