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
	float generate_single(Vector3 position);

	void generate_set(ArraySlice<float> in_x, ArraySlice<float> in_y, ArraySlice<float> in_z,
			ArraySlice<float> out_sdf, bool skip_xz);

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
		unsigned int size;
		float constant_value;
		bool is_constant;
		bool is_binding = false;
	};

	struct HeapResource {
		void *ptr;
		void (*deleter)(void *p);
	};

	class CompileContext {
	public:
		CompileContext(const ProgramGraph::Node &node, std::vector<uint8_t> &program,
				std::vector<HeapResource> &heap_resources) :
				_node(node),
				_program(program),
				_heap_resources(heap_resources),
				_offset(program.size()) {}

		const ProgramGraph::Node &get_node() const {
			return _node;
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
		String _error_message;
		bool _has_error = false;
	};

	class _ProcessContext {
	public:
		inline _ProcessContext(const ArraySlice<uint16_t> inputs, ArraySlice<uint16_t> outputs,
				const ArraySlice<uint8_t> params) :
				_inputs(inputs),
				_outputs(outputs),
				_params(params) {}

		template <typename T>
		inline const T &get_params() const {
			return *reinterpret_cast<const T *>(_params.data());
		}

	protected:
		inline uint32_t get_input_address(uint32_t i) const {
			return _inputs[i];
		}
		inline uint32_t get_output_address(uint32_t i) const {
			return _outputs[i];
		}

	private:
		const ArraySlice<uint16_t> _inputs;
		const ArraySlice<uint16_t> _outputs;
		ArraySlice<uint8_t> _params;
	};

	class ProcessBufferContext : public _ProcessContext {
	public:
		inline ProcessBufferContext(const ArraySlice<uint16_t> inputs, ArraySlice<uint16_t> outputs,
				const ArraySlice<uint8_t> params,
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
		inline RangeAnalysisContext(const ArraySlice<uint16_t> inputs, ArraySlice<uint16_t> outputs,
				const ArraySlice<uint8_t> params, ArraySlice<Interval> ranges) :
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
	bool _compile(const ProgramGraph &graph, bool debug);

	std::vector<uint8_t> _program;
	std::vector<Interval> _ranges;
	std::vector<Buffer> _buffers;
	int _buffer_size = 0;

	std::vector<HeapResource> _heap_resources;

	uint32_t _xzy_program_start;
	int _x_input_address = -1;
	int _y_input_address = -1;
	int _z_input_address = -1;
	int _sdf_output_address = -1;
	CompilationResult _compilation_result;

	HashMap<ProgramGraph::PortLocation, uint16_t, ProgramGraph::PortLocationHasher> _output_port_addresses;
};

#endif // VOXEL_GRAPH_RUNTIME_H
