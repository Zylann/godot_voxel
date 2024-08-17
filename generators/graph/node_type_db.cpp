#include "node_type_db.h"
#include "../../util/godot/core/array.h"
#include "../../util/macros.h"
#include "../../util/string/format.h"
#include "image_range_grid.h"
#include "nodes/curve.h"
#include "nodes/image.h"
#include "nodes/inputs.h"
#include "nodes/math_funcs.h"
#include "nodes/math_ops.h"
#include "nodes/math_vectors.h"
#include "nodes/misc.h"
#include "nodes/noise.h"
#include "nodes/outputs.h"
#include "nodes/sdf.h"

namespace zylann::voxel::pg {

namespace {
NodeTypeDB *g_node_type_db = nullptr;
}

const NodeTypeDB &NodeTypeDB::get_singleton() {
	CRASH_COND(g_node_type_db == nullptr);
	return *g_node_type_db;
}

void NodeTypeDB::create_singleton() {
	CRASH_COND(g_node_type_db != nullptr);
	g_node_type_db = ZN_NEW(NodeTypeDB());
}

void NodeTypeDB::destroy_singleton() {
	CRASH_COND(g_node_type_db == nullptr);
	ZN_DELETE(g_node_type_db);
	g_node_type_db = nullptr;
}

const char *get_category_name(Category category) {
	switch (category) {
		case CATEGORY_INPUT:
			return "Input";
		case CATEGORY_OUTPUT:
			return "Output";
		case CATEGORY_MATH:
			return "Math";
		case CATEGORY_CONVERT:
			return "Convert";
		case CATEGORY_GENERATE:
			return "Generate";
		case CATEGORY_SDF:
			return "Sdf";
		case CATEGORY_DEBUG:
			return "Debug";
		case CATEGORY_FUNCTIONS:
			return "Functions";
		case CATEGORY_RELAY:
			return "Relay";
		case CATEGORY_CONSTANT:
			return "Constant";
		default:
			CRASH_NOW_MSG("Unhandled category");
	}
	return "";
}

NodeTypeDB::NodeTypeDB() {
	Span<NodeType> types = to_span(_types);

	// TODO Most operations need SIMD support

	// SUGG the program could be a list of pointers to polymorphic heap-allocated classes...
	// but I find that the data struct approach is kinda convenient too?

	register_curve_node(types);
	register_image_nodes(types);
	register_input_nodes(types);
	register_output_nodes(types);
	register_math_func_nodes(types);
	register_math_ops_nodes(types);
	register_math_vector_nodes(types);
	register_misc_nodes(types);
	register_noise_nodes(types);
	register_sdf_nodes(types);

	CRASH_COND(_expression_functions.size() > 0);

	for (unsigned int i = 0; i < _types.size(); ++i) {
		NodeType &t = _types[i];
		ZN_ASSERT(!t.name.is_empty());

		_type_name_to_id.insert({ t.name, (VoxelGraphFunction::NodeTypeID)i });

		for (uint32_t param_index = 0; param_index < t.params.size(); ++param_index) {
			NodeType::Param &p = t.params[param_index];
			t.param_name_to_index.insert({ p.name, param_index });
			p.index = param_index;

			switch (p.type) {
				case Variant::FLOAT:
					if (p.default_value.get_type() == Variant::NIL) {
						p.default_value = 0.f;
					}
					break;

				case Variant::INT:
					if (p.default_value.get_type() == Variant::NIL) {
						p.default_value = 0;
					}
					break;

				case Variant::OBJECT:
				case Variant::STRING:
					break;

				default:
					CRASH_NOW();
					break;
			}
		}

		for (uint32_t input_index = 0; input_index < t.inputs.size(); ++input_index) {
			const NodeType::Port &p = t.inputs[input_index];
			t.input_name_to_index.insert({ p.name, input_index });
		}

		if (t.expression_func != nullptr) {
			CRASH_COND(t.expression_func_name == nullptr);
			ExpressionParser::Function f;
			f.argument_count = t.inputs.size();
			f.name = t.expression_func_name;
			f.func = t.expression_func;
			f.id = i;
			_expression_functions.push_back(f);
		}
	}
}

Dictionary NodeTypeDB::get_type_info_dict(uint32_t id) const {
	const NodeType &type = _types[id];

	Dictionary type_dict;
	type_dict["name"] = type.name;

	Array inputs;
	inputs.resize(type.inputs.size());
	for (size_t i = 0; i < type.inputs.size(); ++i) {
		const NodeType::Port &input = type.inputs[i];
		Dictionary d;
		d["name"] = input.name;
		inputs[i] = d;
	}

	Array outputs;
	outputs.resize(type.outputs.size());
	for (size_t i = 0; i < type.outputs.size(); ++i) {
		const NodeType::Port &output = type.outputs[i];
		Dictionary d;
		d["name"] = output.name;
		outputs[i] = d;
	}

	Array params;
	params.resize(type.params.size());
	for (size_t i = 0; i < type.params.size(); ++i) {
		const NodeType::Param &p = type.params[i];
		Dictionary d;
		d["name"] = p.name;
		d["type"] = p.type;
		d["class_name"] = p.class_name;
		d["default_value"] = p.default_value;
		params[i] = d;
	}

	type_dict["inputs"] = inputs;
	type_dict["outputs"] = outputs;
	type_dict["params"] = params;

	return type_dict;
}

bool NodeTypeDB::try_get_type_id_from_name(const String &name, VoxelGraphFunction::NodeTypeID &out_type_id) const {
	auto it = _type_name_to_id.find(name);
	if (it == _type_name_to_id.end()) {
		return false;
	}
	out_type_id = it->second;
	return true;
}

bool NodeTypeDB::try_get_param_index_from_name(uint32_t type_id, const String &name, uint32_t &out_param_index) const {
	ZN_ASSERT_RETURN_V(type_id < _types.size(), false);
	const NodeType &t = _types[type_id];
	auto it = t.param_name_to_index.find(name);
	if (it == t.param_name_to_index.end()) {
		return false;
	}
	out_param_index = it->second;
	return true;
}

bool NodeTypeDB::try_get_input_index_from_name(uint32_t type_id, const String &name, uint32_t &out_input_index) const {
	ZN_ASSERT_RETURN_V(type_id < _types.size(), false);
	const NodeType &t = _types[type_id];
	auto it = t.input_name_to_index.find(name);
	if (it == t.input_name_to_index.end()) {
		return false;
	}
	out_input_index = it->second;
	return true;
}

VoxelGraphFunction::Port make_port_from_io_node(const ProgramGraph::Node &node, const NodeType &type) {
	ZN_ASSERT(type.category == CATEGORY_INPUT || type.category == CATEGORY_OUTPUT);

	VoxelGraphFunction::Port port;
	port.type = VoxelGraphFunction::NodeTypeID(node.type_id);

	switch (port.type) {
		case VoxelGraphFunction::NODE_CUSTOM_INPUT:
		case VoxelGraphFunction::NODE_CUSTOM_OUTPUT:
			port.name = node.name;
			break;

		case VoxelGraphFunction::NODE_OUTPUT_WEIGHT:
			ZN_ASSERT(node.params.size() >= 1);
			port.sub_index = node.params[0];
			ZN_ASSERT(type.outputs.size() == 1);
			port.name = type.outputs[0].name + String("_") + String::num_int64(port.sub_index);
			break;

		default:
			if (type.category == CATEGORY_INPUT) {
				ZN_ASSERT(type.outputs.size() == 1);
				port.name = type.outputs[0].name;
			} else {
				ZN_ASSERT(type.inputs.size() == 1);
				port.name = type.inputs[0].name;
			}
			break;
	}

	return port;
}

bool is_node_matching_port(const ProgramGraph::Node &node, const VoxelGraphFunction::Port &port) {
	if (node.type_id != port.type) {
		return false;
	}

	if (port.is_custom()) {
		return port.name == node.name;
	}

	if (node.type_id == VoxelGraphFunction::NODE_OUTPUT_WEIGHT) {
		ZN_ASSERT(node.params.size() >= 1);
		const unsigned int sub_index = node.params[0];
		if (sub_index != port.sub_index) {
			return false;
		}
	}

	return true;
}

} // namespace zylann::voxel::pg
