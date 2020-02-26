#include "voxel_graph_node_db.h"

namespace {
VoxelGraphNodeDB *g_node_type_db = nullptr;
}

VoxelGraphNodeDB *VoxelGraphNodeDB::get_singleton() {
	CRASH_COND(g_node_type_db == nullptr);
	return g_node_type_db;
}

void VoxelGraphNodeDB::create_singleton() {
	CRASH_COND(g_node_type_db != nullptr);
	g_node_type_db = memnew(VoxelGraphNodeDB());
}

void VoxelGraphNodeDB::destroy_singleton() {
	CRASH_COND(g_node_type_db == nullptr);
	memdelete(g_node_type_db);
	g_node_type_db = nullptr;
}

VoxelGraphNodeDB::VoxelGraphNodeDB() {
	FixedArray<NodeType, VoxelGeneratorGraph::NODE_TYPE_COUNT> &types = _types;

	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_CONSTANT];
		t.name = "Constant";
		t.outputs.push_back(Port("Value"));
		t.params.push_back(Param("Value", Variant::REAL));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_INPUT_X];
		t.name = "Input X";
		t.outputs.push_back(Port("Value"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_INPUT_Y];
		t.name = "Input Y";
		t.outputs.push_back(Port("Value"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_INPUT_Z];
		t.name = "Input Z";
		t.outputs.push_back(Port("Value"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_OUTPUT_SDF];
		t.name = "Output SDF";
		t.inputs.push_back(Port("Value"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_ADD];
		t.name = "Add";
		t.inputs.push_back(Port("A"));
		t.inputs.push_back(Port("B"));
		t.outputs.push_back(Port("Sum"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_SUBTRACT];
		t.name = "Subtract";
		t.inputs.push_back(Port("A"));
		t.inputs.push_back(Port("B"));
		t.outputs.push_back(Port("Sum"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_MULTIPLY];
		t.name = "Multiply";
		t.inputs.push_back(Port("A"));
		t.inputs.push_back(Port("B"));
		t.outputs.push_back(Port("Product"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_SINE];
		t.name = "Sine";
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_FLOOR];
		t.name = "Floor";
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_ABS];
		t.name = "Abs";
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_SQRT];
		t.name = "Sqrt";
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_DISTANCE_2D];
		t.name = "Distance 2D";
		t.inputs.push_back(Port("X0"));
		t.inputs.push_back(Port("Y0"));
		t.inputs.push_back(Port("X1"));
		t.inputs.push_back(Port("Y1"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_DISTANCE_3D];
		t.name = "Distance 3D";
		t.inputs.push_back(Port("X0"));
		t.inputs.push_back(Port("Y0"));
		t.inputs.push_back(Port("Y0"));
		t.inputs.push_back(Port("X1"));
		t.inputs.push_back(Port("Y1"));
		t.inputs.push_back(Port("Z1"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_CLAMP];
		t.name = "Clamp";
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
		t.params.push_back(Param("Min", Variant::REAL, -1.f));
		t.params.push_back(Param("Max", Variant::REAL, 1.f));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_MIX];
		t.name = "Mix";
		t.inputs.push_back(Port("A"));
		t.inputs.push_back(Port("B"));
		t.inputs.push_back(Port("Ratio"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_REMAP];
		t.name = "Remap";
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
		t.params.push_back(Param("Min0", Variant::REAL, -1.f));
		t.params.push_back(Param("Max0", Variant::REAL, 1.f));
		t.params.push_back(Param("Min1", Variant::REAL, -1.f));
		t.params.push_back(Param("Max1", Variant::REAL, 1.f));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_CURVE];
		t.name = "Curve";
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
		t.params.push_back(Param("Curve", "Curve"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_NOISE_2D];
		t.name = "Noise 2D";
		t.inputs.push_back(Port("X"));
		t.inputs.push_back(Port("Y"));
		t.outputs.push_back(Port("Result"));
		t.params.push_back(Param("Noise", "OpenSimplexNoise"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_NOISE_3D];
		t.name = "Noise 3D";
		t.inputs.push_back(Port("X"));
		t.inputs.push_back(Port("Y"));
		t.inputs.push_back(Port("Z"));
		t.outputs.push_back(Port("Result"));
		t.params.push_back(Param("Noise", "OpenSimplexNoise"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_IMAGE_2D];
		t.name = "Image";
		t.inputs.push_back(Port("X"));
		t.inputs.push_back(Port("Y"));
		t.outputs.push_back(Port("Result"));
		t.params.push_back(Param("Image", "Image"));
	}
}

Dictionary VoxelGraphNodeDB::get_type_info_dict(int id) const {
	const NodeType &type = _types[id];

	Dictionary type_dict;
	type_dict["name"] = type.name;

	Array inputs;
	inputs.resize(type.inputs.size());
	for (size_t i = 0; i < type.inputs.size(); ++i) {
		const Port &input = type.inputs[i];
		Dictionary d;
		d["name"] = input.name;
		inputs[i] = d;
	}

	Array outputs;
	outputs.resize(type.outputs.size());
	for (size_t i = 0; i < type.outputs.size(); ++i) {
		const Port &output = type.outputs[i];
		Dictionary d;
		d["name"] = output.name;
		outputs[i] = d;
	}

	Array params;
	params.resize(type.params.size());
	for (size_t i = 0; i < type.params.size(); ++i) {
		const Param &p = type.params[i];
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
