#include "voxel_graph_shader_generator.h"
#include "../../engine/compute_shader_parameters.h"
#include "../../engine/compute_shader_resource.h"
#include "../../util/godot/core/array.h" // for `varray` in GDExtension builds
#include "../../util/profiling.h"
#include "../../util/string_funcs.h"
#include "node_type_db.h"
#include "voxel_graph_compiler.h"

namespace zylann::voxel::pg {

void ShaderGenContext::require_lib_code(const char *lib_name, const char *code) {
	_code_gen.require_lib_code(lib_name, code);
}

void ShaderGenContext::require_lib_code(const char *lib_name, const char **code) {
	_code_gen.require_lib_code(lib_name, code);
}

std::string ShaderGenContext::add_uniform(ComputeShaderResource &&res) {
	std::string name = format("u_vg_resource_{}", _uniforms.size());
	_uniforms.push_back(ShaderParameter());
	ShaderParameter &sp = _uniforms.back();
	sp.name = name;
	sp.resource = std::move(res);
	return name;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CompilationResult generate_shader(const ProgramGraph &p_graph, Span<const VoxelGraphFunction::Port> input_defs,
		FwdMutableStdString output, std::vector<ShaderParameter> &shader_params) {
	ZN_PROFILE_SCOPE();

	const NodeTypeDB &type_db = NodeTypeDB::get_singleton();

	ProgramGraph expanded_graph;
	const CompilationResult expand_result =
			expand_graph(p_graph, expanded_graph, input_defs, nullptr, type_db, nullptr);
	if (!expand_result.success) {
		return expand_result;
	}

	std::vector<uint32_t> order;
	std::vector<uint32_t> terminal_nodes;

	// Only getting SDF for now, as this is the first use case I want to test this feature with
	expanded_graph.for_each_node_const([&terminal_nodes](const ProgramGraph::Node &node) {
		if (node.type_id == VoxelGraphFunction::NODE_OUTPUT_SDF) {
			terminal_nodes.push_back(node.id);
		}
	});

	if (terminal_nodes.size() == 0) {
		return CompilationResult::make_error("The graph must contain an SDF output.");
	}

	// Exclude debug nodes
	// unordered_remove_if(terminal_nodes, [&expanded_graph, &type_db](uint32_t node_id) {
	// 	const ProgramGraph::Node &node = expanded_graph.get_node(node_id);
	// 	const NodeType &type = type_db.get_type(node.type_id);
	// 	return type.debug_only;
	// });

	expanded_graph.find_dependencies(terminal_nodes, order);

	std::stringstream main_ss;
	std::stringstream lib_ss;
	CodeGenHelper codegen(main_ss, lib_ss);

	// This function name is chosen to match the expected signature when used with normalmap baking compute shaders.
	codegen.add("float get_sd(vec3 pos) {\n");
	codegen.indent();

	std::unordered_map<ProgramGraph::PortLocation, std::string> port_to_var;
	FixedArray<const char *, 8> input_names;
	FixedArray<const char *, 8> output_names;

	for (const uint32_t node_id : order) {
		const ProgramGraph::Node &node = expanded_graph.get_node(node_id);
		const NodeType node_type = type_db.get_type(node.type_id);

		switch (node.type_id) {
			case VoxelGraphFunction::NODE_INPUT_X: {
				ZN_ASSERT(node.outputs.size() == 1);
				const ProgramGraph::PortLocation output_port{ node_id, 0 };
				port_to_var.insert({ output_port, "pos.x" });
				continue;
			}
			case VoxelGraphFunction::NODE_INPUT_Y: {
				ZN_ASSERT(node.outputs.size() == 1);
				const ProgramGraph::PortLocation output_port{ node_id, 0 };
				port_to_var.insert({ output_port, "pos.y" });
				continue;
			}
			case VoxelGraphFunction::NODE_INPUT_Z: {
				ZN_ASSERT(node.outputs.size() == 1);
				const ProgramGraph::PortLocation output_port{ node_id, 0 };
				port_to_var.insert({ output_port, "pos.z" });
				continue;
			}
			case VoxelGraphFunction::NODE_CONSTANT: {
				ZN_ASSERT(node.outputs.size() == 1);
				const ProgramGraph::PortLocation output_port{ node_id, 0 };
				std::string name;
				codegen.generate_var_name(name);
				port_to_var.insert({ output_port, name });
				ZN_ASSERT(node.params.size() == 1);
				codegen.add_format("float {} = {};\n", name, float(node.params[0]));
				continue;
			}
			case VoxelGraphFunction::NODE_OUTPUT_SDF: {
				ZN_ASSERT(node.outputs.size() == 1);
				const ProgramGraph::Port &input_port = node.inputs[0];
				if (input_port.connections.size() > 0) {
					ZN_ASSERT(input_port.connections.size() == 1);
					auto it = port_to_var.find(input_port.connections[0]);
					ZN_ASSERT(it != port_to_var.end());
					codegen.add_format("return {};\n", it->second);
				} else {
					codegen.add("return 0.0;\n");
				}
				continue;
			}
			// TODO Custom inputs
			// TODO Custom outputs
			default:
				break;
		}

		if (node_type.shader_gen_func == nullptr) {
			CompilationResult error;
			error.message =
					String("The node {0} does not support conversion to shader.").format(varray(node_type.name));
			error.node_id = node_id;
			return error;
		}

		for (unsigned int port_index = 0; port_index < node.inputs.size(); ++port_index) {
			const ProgramGraph::Port &input_port = node.inputs[port_index];
			if (input_port.connections.size() > 0) {
				ZN_ASSERT(input_port.connections.size() == 1);
				auto it = port_to_var.find(input_port.connections[0]);
				ZN_ASSERT(it != port_to_var.end());
				input_names[port_index] = it->second.c_str();
			} else {
				std::string var_name;
				codegen.generate_var_name(var_name);
				auto p = port_to_var.insert({ { node_id, port_index }, var_name });
				ZN_ASSERT(p.second);
				const std::string &name = p.first->second;
				input_names[port_index] = name.c_str();
				codegen.add_format("float {} = {};\n", name, float(node.default_inputs[port_index]));
			}
		}

		for (unsigned int port_index = 0; port_index < node.outputs.size(); ++port_index) {
			std::string var_name;
			codegen.generate_var_name(var_name);
			auto p = port_to_var.insert({ { node_id, port_index }, var_name });
			ZN_ASSERT(p.second);
			output_names[port_index] = p.first->second.c_str();
			codegen.add_format("float {};\n", var_name.c_str());
		}

		codegen.add("{\n");
		codegen.indent();

		ShaderGenContext ctx(node.params, to_span(input_names, node.inputs.size()),
				to_span(output_names, node.outputs.size()), codegen, shader_params);
		node_type.shader_gen_func(ctx);

		if (ctx.has_error()) {
			CompilationResult result;
			result.success = false;
			result.message = ctx.get_error_message();
			result.node_id = node_id;
			return result;
		}

		codegen.dedent();
		codegen.add("}\n");
	}

	codegen.dedent();
	codegen.add("}\n");

	codegen.print(output);

	CompilationResult result;
	result.success = true;
	return result;
}

} // namespace zylann::voxel::pg
