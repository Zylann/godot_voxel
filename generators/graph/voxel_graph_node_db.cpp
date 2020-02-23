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
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_CONSTANT];
		t.outputs.push_back(Port("Value"));
		t.params.push_back(Param("Value"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_INPUT_X];
		t.outputs.push_back(Port("Value"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_INPUT_Y];
		t.outputs.push_back(Port("Value"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_INPUT_Z];
		t.outputs.push_back(Port("Value"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_OUTPUT_SDF];
		t.inputs.push_back(Port("Value"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_ADD];
		t.inputs.push_back(Port("A"));
		t.inputs.push_back(Port("B"));
		t.outputs.push_back(Port("Sum"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_SUBTRACT];
		t.inputs.push_back(Port("A"));
		t.inputs.push_back(Port("B"));
		t.outputs.push_back(Port("Sum"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_MULTIPLY];
		t.inputs.push_back(Port("A"));
		t.inputs.push_back(Port("B"));
		t.outputs.push_back(Port("Product"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_SINE];
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_FLOOR];
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_ABS];
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_SQRT];
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_DISTANCE_2D];
		t.inputs.push_back(Port("X0"));
		t.inputs.push_back(Port("Y0"));
		t.inputs.push_back(Port("X1"));
		t.inputs.push_back(Port("Y1"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_DISTANCE_3D];
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
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
		t.params.push_back(Param("Min", -1.f));
		t.params.push_back(Param("Max", 1.f));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_MIX];
		t.inputs.push_back(Port("A"));
		t.inputs.push_back(Port("B"));
		t.inputs.push_back(Port("Ratio"));
		t.outputs.push_back(Port("Result"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_REMAP];
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
		t.params.push_back(Param("Min0", -1.f));
		t.params.push_back(Param("Max0", 1.f));
		t.params.push_back(Param("Min1", -1.f));
		t.params.push_back(Param("Max1", 1.f));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_CURVE];
		t.inputs.push_back(Port("X"));
		t.outputs.push_back(Port("Result"));
		t.params.push_back(Param("Curve"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_NOISE_2D];
		t.inputs.push_back(Port("X"));
		t.inputs.push_back(Port("Y"));
		t.outputs.push_back(Port("Result"));
		t.params.push_back(Param("Noise"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_NOISE_3D];
		t.inputs.push_back(Port("X"));
		t.inputs.push_back(Port("Y"));
		t.inputs.push_back(Port("Z"));
		t.outputs.push_back(Port("Result"));
		t.params.push_back(Param("Noise"));
	}
	{
		NodeType &t = types[VoxelGeneratorGraph::NODE_IMAGE_2D];
		t.inputs.push_back(Port("X"));
		t.inputs.push_back(Port("Y"));
		t.outputs.push_back(Port("Result"));
		t.params.push_back(Param("Image"));
	}
}
