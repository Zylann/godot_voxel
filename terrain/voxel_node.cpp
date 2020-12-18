#include "voxel_node.h"
#include "../meshers/voxel_mesher.h"
#include "../streams/voxel_stream.h"

void VoxelNode::set_mesher(Ref<VoxelMesher> mesher) {
	// Not implemented
}

Ref<VoxelMesher> VoxelNode::get_mesher() const {
	// Not implemented
	return Ref<VoxelMesher>();
}

void VoxelNode::set_stream(Ref<VoxelStream> stream) {
	// Not implemented
}

Ref<VoxelStream> VoxelNode::get_stream() const {
	// Not implemented
	return Ref<VoxelStream>();
}

void VoxelNode::restart_stream() {
	// Not implemented
}

void VoxelNode::remesh_all_blocks() {
	// Not implemented
}

String VoxelNode::get_configuration_warning() const {
	Ref<VoxelMesher> mesher = get_mesher();
	Ref<VoxelStream> stream = get_stream();

	if (mesher.is_null()) {
		return TTR("This node has no mesher assigned, it wont produce any mesh visuals. "
				   "You can assign one on the `mesher` property.");
	}

	if (stream.is_valid()) {
		Ref<Script> script = stream->get_script();

		if (script.is_valid()) {
			if (script->is_tool()) {
				// TODO This is very annoying. Probably needs an issue or proposal in Godot so we can handle this properly?
				return TTR("Be careful! Don't edit your custom stream while it's running, "
						   "it can cause crashes. Turn off `run_stream_in_editor` before doing so.");
			} else {
				return TTR("The custom stream is not tool, the editor won't be able to use it.");
			}
		}

		const int stream_channels = stream->get_used_channels_mask();
		const int mesher_channels = mesher->get_used_channels_mask();

		if ((stream_channels & mesher_channels) == 0) {
			return TTR("The current stream is providing voxel data only on channels that are not used by the current mesher. "
					   "This will result in nothing being visible.");
		}
	}

	return String();
}

void VoxelNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_stream", "stream"), &VoxelNode::_b_set_stream);
	ClassDB::bind_method(D_METHOD("get_stream"), &VoxelNode::_b_get_stream);

	ClassDB::bind_method(D_METHOD("set_mesher", "mesher"), &VoxelNode::_b_set_mesher);
	ClassDB::bind_method(D_METHOD("get_mesher"), &VoxelNode::_b_get_mesher);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "stream", PROPERTY_HINT_RESOURCE_TYPE, "VoxelStream"),
			"set_stream", "get_stream");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesher", PROPERTY_HINT_RESOURCE_TYPE, "VoxelMesher"),
			"set_mesher", "get_mesher");
}
