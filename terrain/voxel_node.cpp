#include "voxel_node.h"
#include "../generators/voxel_generator.h"
#include "../meshers/voxel_mesher.h"
#include "../streams/voxel_stream.h"

void VoxelNode::set_mesher(Ref<VoxelMesher> mesher) {
	// Implemented in subclasses
}

Ref<VoxelMesher> VoxelNode::get_mesher() const {
	// Implemented in subclasses
	return Ref<VoxelMesher>();
}

void VoxelNode::set_stream(Ref<VoxelStream> stream) {
	// Implemented in subclasses
}

Ref<VoxelStream> VoxelNode::get_stream() const {
	// Implemented in subclasses
	return Ref<VoxelStream>();
}

void VoxelNode::set_generator(Ref<VoxelGenerator> generator) {
	// Implemented in subclasses
}

Ref<VoxelGenerator> VoxelNode::get_generator() const {
	// Implemented in subclasses
	return Ref<VoxelGenerator>();
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
	Ref<VoxelGenerator> generator = get_generator();

	if (mesher.is_null()) {
		return TTR("This node has no mesher assigned, it wont produce any mesh visuals. "
				   "You can assign one on the `mesher` property.");
	}

	if (stream.is_valid()) {
		Ref<Script> script = stream->get_script();

		if (script.is_valid()) {
			if (script->is_tool()) {
				// TODO This is very annoying. Probably needs an issue or proposal in Godot so we can handle this properly?
				return TTR("Careful, don't edit your custom stream while it's running, "
						   "it can cause crashes. Turn off `run_stream_in_editor` before doing so.");
			} else {
				return TTR("The custom stream is not tool, the editor won't be able to use it.");
			}
		}

		const int stream_channels = stream->get_used_channels_mask();
		const int mesher_channels = mesher->get_used_channels_mask();

		if ((stream_channels & mesher_channels) == 0) {
			return TTR("The current stream is providing voxel data only on channels that are not used by "
					   "the current mesher. This will result in nothing being visible.");
		}
	}

	if (generator.is_valid()) {
		Ref<Script> script = generator->get_script();
		bool can_check_generator_channels = true;

		if (script.is_valid()) {
			if (script->is_tool()) {
				// TODO This is very annoying. Probably needs an issue or proposal in Godot so we can handle this properly?
				return TTR("Careful, don't edit your custom generator while it's running, "
						   "it can cause crashes. Turn off `run_stream_in_editor` before doing so.");
			} else {
				can_check_generator_channels = false;
				// return TTR("The custom generator is not tool, the editor won't be able to use it.");
			}
		}

		if (can_check_generator_channels) {
			const int generator_channels = generator->get_used_channels_mask();
			const int mesher_channels = mesher->get_used_channels_mask();

			if ((generator_channels & mesher_channels) == 0) {
				return TTR("The current generator is providing voxel data only on channels that are not used by "
						   "the current mesher. This will result in nothing being visible.");
			}
		}
	}

	return String();
}

int VoxelNode::get_used_channels_mask() const {
	Ref<VoxelGenerator> generator = get_generator();
	Ref<VoxelStream> stream = get_stream();
	int used_channels_mask = 0;
	if (generator.is_valid()) {
		used_channels_mask |= generator->get_used_channels_mask();
	}
	if (stream.is_valid()) {
		used_channels_mask |= stream->get_used_channels_mask();
	}
	return used_channels_mask;
}

void VoxelNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_stream", "stream"), &VoxelNode::_b_set_stream);
	ClassDB::bind_method(D_METHOD("get_stream"), &VoxelNode::_b_get_stream);

	ClassDB::bind_method(D_METHOD("set_generator", "generator"), &VoxelNode::_b_set_generator);
	ClassDB::bind_method(D_METHOD("get_generator"), &VoxelNode::_b_get_generator);

	ClassDB::bind_method(D_METHOD("set_mesher", "mesher"), &VoxelNode::_b_set_mesher);
	ClassDB::bind_method(D_METHOD("get_mesher"), &VoxelNode::_b_get_mesher);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "stream", PROPERTY_HINT_RESOURCE_TYPE, "VoxelStream"),
			"set_stream", "get_stream");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "generator", PROPERTY_HINT_RESOURCE_TYPE, "VoxelGenerator"),
			"set_generator", "get_generator");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesher", PROPERTY_HINT_RESOURCE_TYPE, "VoxelMesher"),
			"set_mesher", "get_mesher");
}
