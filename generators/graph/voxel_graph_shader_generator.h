#ifndef VOXEL_GRAPH_SHADER_GENERATOR_H
#define VOXEL_GRAPH_SHADER_GENERATOR_H

#include "../../engine/compute_shader_resource.h"
#include "../../util/errors.h"
#include "../../util/godot/core/variant.h"
#include "../../util/span.h"
#include "code_gen_helper.h"
#include "voxel_graph_function.h"
#include "voxel_graph_runtime.h"

namespace zylann::voxel::pg {

struct ShaderParameter {
	std::string name;
	ComputeShaderResource resource;
};

// Generates GLSL code from the given graph.
CompilationResult generate_shader(const ProgramGraph &p_graph, Span<const VoxelGraphFunction::Port> input_defs,
		FwdMutableStdString output, std::vector<ShaderParameter> &uniforms);

// Sent as argument to functions implementing generator nodes, in order to generate shader code.
class ShaderGenContext {
public:
	ShaderGenContext(const std::vector<Variant> &params, Span<const char *> input_names,
			Span<const char *> output_names, CodeGenHelper &code_gen, std::vector<ShaderParameter> &uniforms) :
			_params(params),
			_input_names(input_names),
			_output_names(output_names),
			_code_gen(code_gen),
			_uniforms(uniforms) {}

	Variant get_param(size_t i) const {
		ZN_ASSERT(i < _params.size());
		return _params[i];
	}

	const char *get_input_name(unsigned int i) const {
		return _input_names[i];
	}

	const char *get_output_name(unsigned int i) const {
		return _output_names[i];
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

	template <typename... TN>
	void add_format(const char *fmt, const TN &...an) {
		_code_gen.add_format(fmt, an...);
	}

	void require_lib_code(const char *lib_name, const char *code);
	// If the code is too long for a string constant, it can be provided as a list of strings
	void require_lib_code(const char *lib_name, const char **code);

	std::string add_uniform(ComputeShaderResource &&res);

private:
	const std::vector<Variant> &_params;
	Span<const char *> _input_names;
	Span<const char *> _output_names;
	CodeGenHelper &_code_gen;
	String _error_message;
	bool _has_error = false;
	std::vector<ShaderParameter> &_uniforms;
};

typedef void (*ShaderGenFunc)(ShaderGenContext &);

} // namespace zylann::voxel::pg

#endif // VOXEL_GRAPH_SHADER_GENERATOR_H
