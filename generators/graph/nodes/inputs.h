#include "../node_type_db.h"

namespace zylann::voxel::pg {

void register_input_nodes(Span<NodeType> types) {
	{
		NodeType &t = types[VoxelGraphFunction::NODE_INPUT_X];
		t.name = "InputX";
		t.category = CATEGORY_INPUT;
		t.outputs.push_back(NodeType::Port("x"));
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_INPUT_Y];
		t.name = "InputY";
		t.category = CATEGORY_INPUT;
		t.outputs.push_back(NodeType::Port("y"));
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_INPUT_Z];
		t.name = "InputZ";
		t.category = CATEGORY_INPUT;
		t.outputs.push_back(NodeType::Port("z"));
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_INPUT_SDF];
		t.name = "InputSDF";
		t.category = CATEGORY_INPUT;
		t.outputs.push_back(NodeType::Port("sdf"));
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_CUSTOM_INPUT];
		t.name = "CustomInput";
		// t.params.push_back(NodeType::Param("binding", Variant::INT, 0));
		t.category = CATEGORY_INPUT;
		t.outputs.push_back(NodeType::Port("value"));
	}
}

} // namespace zylann::voxel::pg
