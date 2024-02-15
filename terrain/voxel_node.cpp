#include "voxel_node.h"
#include "../edition/voxel_tool.h"
#include "../generators/voxel_generator.h"
#include "../meshers/voxel_mesher.h"
#include "../streams/voxel_stream.h"
#include "../util/godot/classes/script.h"
#include "../util/godot/core/string.h"

#ifdef TOOLS_ENABLED
#include "../util/godot/core/packed_arrays.h"
#endif

namespace zylann::voxel {

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
	// Implemented in subclasses
}

void VoxelNode::remesh_all_blocks() {
	// Implemented in subclasses
}

VolumeID VoxelNode::get_volume_id() const {
	ZN_PRINT_ERROR("Not implemented");
	// Implemented in subclasses
	return VolumeID();
}

std::shared_ptr<StreamingDependency> VoxelNode::get_streaming_dependency() const {
	ZN_PRINT_ERROR("Not implemented");
	// Implemented in subclasses
	return nullptr;
}

Ref<VoxelTool> VoxelNode::get_voxel_tool() {
	ZN_PRINT_ERROR("Not implemented");
	// Implemented in subclasses
	return Ref<VoxelTool>();
}

#ifdef TOOLS_ENABLED

#if defined(ZN_GODOT)
#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 2
PackedStringArray VoxelNode::get_configuration_warnings() const {
	PackedStringArray warnings;
	get_configuration_warnings(warnings);
	return warnings;
}
#else
Array VoxelNode::get_configuration_warnings() const {
	PackedStringArray warnings;
	get_configuration_warnings(warnings);
	// TODO Eventually make use of new features introduced in Godot 4.3
	return to_array(warnings);
}
#endif
#elif defined(ZN_GODOT_EXTENSION)
#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 2
PackedStringArray VoxelNode::_get_configuration_warnings() const {
	PackedStringArray warnings;
	get_configuration_warnings(warnings);
	return warnings;
}
#else
Array VoxelNode::_get_configuration_warnings() const {
	PackedStringArray warnings;
	get_configuration_warnings(warnings);
	return to_array(warnings);
}
#endif
#endif

void VoxelNode::get_configuration_warnings(PackedStringArray &warnings) const {
	Ref<VoxelMesher> mesher = get_mesher();
	Ref<VoxelStream> stream = get_stream();
	Ref<VoxelGenerator> generator = get_generator();

	if (mesher.is_null()) {
		warnings.append(ZN_TTR("This node has no mesher assigned, it won't produce any mesh visuals. "
							   "You can assign one on the `mesher` property."));
	}

	if (stream.is_valid()) {
		Ref<Script> stream_script = stream->get_script();

		if (stream_script.is_valid()) {
			if (stream_script->is_tool()) {
				// TODO This is very annoying. Probably needs an issue or proposal in Godot so we can handle this
				// properly?
				warnings.append(ZN_TTR("Careful, don't edit your custom stream while it's running, "
									   "it can cause crashes. Turn off `run_stream_in_editor` before doing so."));
			} else {
				warnings.append(ZN_TTR("The custom stream is not tool, the editor won't be able to use it."));
			}
		}

		if (mesher.is_valid()) {
			const int stream_channels = stream->get_used_channels_mask();
			const int mesher_channels = mesher->get_used_channels_mask();

			if ((stream_channels & mesher_channels) == 0) {
				warnings.append(
						ZN_TTR("The current stream is providing voxel data only on channels that are not used by "
							   "the current mesher. This will result in nothing being visible."));
			}
		}
	}

	if (generator.is_valid()) {
		Ref<Script> generator_script = generator->get_script();
		bool can_check_generator_channels = true;

		if (generator_script.is_valid()) {
			if (generator_script->is_tool()) {
				// TODO This is very annoying. Probably needs an issue or proposal in Godot so we can handle this
				// properly?
				warnings.append(ZN_TTR("Careful, don't edit your custom generator while it's running, "
									   "it can cause crashes. Turn off `run_stream_in_editor` before doing so."));
			} else {
				can_check_generator_channels = false;
				// return ZN_TTR("The custom generator is not tool, the editor won't be able to use it.");
			}
		}

		if (can_check_generator_channels && mesher.is_valid()) {
			const int generator_channels = generator->get_used_channels_mask();
			const int mesher_channels = mesher->get_used_channels_mask();

			if ((generator_channels & mesher_channels) == 0) {
				warnings.append(
						ZN_TTR("The current generator is providing voxel data only on channels that are not used by "
							   "the current mesher. This will result in nothing being visible."));
			}
		}
	}

	if (mesher.is_valid()) {
		mesher->get_configuration_warnings(warnings);
	}
}

#endif

int VoxelNode::get_used_channels_mask() const {
	Ref<VoxelMesher> mesher = get_mesher();
	if (mesher.is_valid()) {
		return mesher->get_used_channels_mask();
	}
	Ref<VoxelGenerator> generator = get_generator();
	if (generator.is_valid()) {
		return generator->get_used_channels_mask();
	}
	Ref<VoxelStream> stream = get_stream();
	if (stream.is_valid()) {
		return stream->get_used_channels_mask();
	}
	return 0;
}

void VoxelNode::set_gi_mode(GeometryInstance3D::GIMode mode) {
	ERR_FAIL_INDEX(mode, GI_MODE_COUNT);
	if (mode != _gi_mode) {
		_gi_mode = mode;
		_on_gi_mode_changed();
	}
}

GeometryInstance3D::GIMode VoxelNode::get_gi_mode() const {
	return _gi_mode;
}

void VoxelNode::set_shadow_casting(GeometryInstance3D::ShadowCastingSetting setting) {
	if (setting != _shadow_casting) {
		_shadow_casting = setting;
		_on_shadow_casting_changed();
	}
}

void VoxelNode::set_render_layers_mask(int mask) {
	if (mask != _render_layers_mask) {
		_render_layers_mask = mask;
		_on_render_layers_mask_changed();
	}
}

int VoxelNode::get_render_layers_mask() const {
	return _render_layers_mask;
}

GeometryInstance3D::ShadowCastingSetting VoxelNode::get_shadow_casting() const {
	return _shadow_casting;
}

void VoxelNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_stream", "stream"), &VoxelNode::_b_set_stream);
	ClassDB::bind_method(D_METHOD("get_stream"), &VoxelNode::_b_get_stream);

	ClassDB::bind_method(D_METHOD("set_generator", "generator"), &VoxelNode::_b_set_generator);
	ClassDB::bind_method(D_METHOD("get_generator"), &VoxelNode::_b_get_generator);

	ClassDB::bind_method(D_METHOD("set_mesher", "mesher"), &VoxelNode::_b_set_mesher);
	ClassDB::bind_method(D_METHOD("get_mesher"), &VoxelNode::_b_get_mesher);

	ClassDB::bind_method(D_METHOD("set_gi_mode", "mode"), &VoxelNode::set_gi_mode);
	ClassDB::bind_method(D_METHOD("get_gi_mode"), &VoxelNode::get_gi_mode);

	ClassDB::bind_method(D_METHOD("set_shadow_casting", "mode"), &VoxelNode::set_shadow_casting);
	ClassDB::bind_method(D_METHOD("get_shadow_casting"), &VoxelNode::get_shadow_casting);

	ClassDB::bind_method(D_METHOD("set_render_layers_mask", "mask"), &VoxelNode::set_render_layers_mask);
	ClassDB::bind_method(D_METHOD("get_render_layers_mask"), &VoxelNode::get_render_layers_mask);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "stream", PROPERTY_HINT_RESOURCE_TYPE, VoxelStream::get_class_static()),
			"set_stream", "get_stream");
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "generator", PROPERTY_HINT_RESOURCE_TYPE, VoxelGenerator::get_class_static()),
			"set_generator", "get_generator");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesher", PROPERTY_HINT_RESOURCE_TYPE, VoxelMesher::get_class_static()),
			"set_mesher", "get_mesher");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "gi_mode", PROPERTY_HINT_ENUM, GI_MODE_ENUM_HINT_STRING), "set_gi_mode",
			"get_gi_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cast_shadow", PROPERTY_HINT_ENUM, CAST_SHADOW_ENUM_HINT_STRING),
			"set_shadow_casting", "get_shadow_casting");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_layers_mask", PROPERTY_HINT_LAYERS_3D_RENDER),
			"set_render_layers_mask", "get_render_layers_mask");
}

} // namespace zylann::voxel
