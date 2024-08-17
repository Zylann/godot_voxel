#ifndef VOXEL_GRAPH_COMPILER_H
#define VOXEL_GRAPH_COMPILER_H

#include "../../util/containers/std_vector.h"
#include "voxel_graph_function.h"
#include "voxel_graph_runtime.h"
#include <type_traits>

namespace zylann::voxel::pg {

struct PortRemap {
	ProgramGraph::PortLocation original;
	// Can have null ID, means the port expanded into nothing.
	ProgramGraph::PortLocation expanded;
};

struct ExpandedNodeRemap {
	uint32_t expanded_node_id;
	uint32_t original_node_id;
};

struct GraphRemappingInfo {
	// Used for debug output previews in the editor
	StdVector<PortRemap> user_to_expanded_ports;
	// Used for error reporting
	StdVector<ExpandedNodeRemap> expanded_to_user_node_ids;
};

// Pre-processes the graph and applies some optimizations before doing the main compilation pass.
// This can involve some nodes getting removed or replaced with new ones.
CompilationResult expand_graph(const ProgramGraph &graph, ProgramGraph &expanded_graph,
		Span<const VoxelGraphFunction::Port> input_defs, StdVector<uint32_t> *input_node_ids, const NodeTypeDB &type_db,
		GraphRemappingInfo *remap_info);

// Functions usable by node implementations during the compilation stage
class CompileContext {
public:
	CompileContext(/*const ProgramGraph::Node &node,*/ StdVector<uint16_t> &program,
			StdVector<Runtime::HeapResource> &heap_resources, StdVector<Variant> &params) :
			/*_node(node),*/ _program(program), _heap_resources(heap_resources), _params(params) {}

	Variant get_param(size_t i) const {
		CRASH_COND(i > _params.size());
		return _params[i];
	}

	// Stores compile-time parameters the node will need. T must be a POD struct.
	template <typename T>
	void set_params(T params) {
		static_assert(std::is_standard_layout_v<T> == true);
		static_assert(std::is_trivial_v<T> == true);

		// Can be called only once per node
		CRASH_COND(_params_added);
		// We will need to align memory, so the struct will not be immediately stored here.
		// Instead we put a header that tells how much to advance in order to reach the beginning of the struct,
		// which will be at an aligned position.
		// We align to the maximum alignment between the struct,
		// and the type of word we store inside the program buffer, which is uint16.
		const size_t params_alignment = math::max(alignof(T), alignof(uint16_t));
		const size_t params_offset_index = _program.size();
		// Prepare space to store the offset (at least 1 since that header is one word)
		_program.push_back(1);
		// Align memory for the struct.
		// Note, we index with words, not bytes.
		const size_t struct_offset =
				math::alignup(_program.size() * sizeof(uint16_t), params_alignment) / sizeof(uint16_t);
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
	void add_delete_cleanup(T *ptr) {
		static_assert(!std::is_base_of<Object, T>::value);
		Runtime::HeapResource hr;
		hr.ptr = ptr;
		hr.deleter = [](void *p) {
			T *tp = reinterpret_cast<T *>(p);
			ZN_DELETE(tp);
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
	// const ProgramGraph::Node &_node;
	StdVector<uint16_t> &_program;
	StdVector<Runtime::HeapResource> &_heap_resources;
	StdVector<Variant> &_params;
	String _error_message;
	size_t _params_size_in_words = 0;
	bool _has_error = false;
	bool _params_added = false;
};

typedef void (*CompileFunc)(CompileContext &);

} // namespace zylann::voxel::pg

#endif // VOXEL_GRAPH_COMPILER_H
