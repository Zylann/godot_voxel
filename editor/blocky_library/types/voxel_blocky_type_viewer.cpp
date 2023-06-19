#include "voxel_blocky_type_viewer.h"
#include "../../../constants/voxel_string_names.h"
#include "../../../util/godot/classes/label.h"
#include "../../../util/godot/classes/mesh_instance_3d.h"
#include "../../../util/godot/classes/option_button.h"
#include "../../../util/godot/core/callable.h"
#include "../../../util/godot/editor_scale.h"
#include "voxel_blocky_type_attribute_combination_selector.h"

namespace zylann::voxel {

VoxelBlockyTypeViewer::VoxelBlockyTypeViewer() {
	const float editor_scale = EDSCALE;

	ZN_ModelViewer *model_viewer = this;
	model_viewer->set_h_size_flags(Container::SIZE_EXPAND_FILL);
	model_viewer->set_v_size_flags(Container::SIZE_EXPAND_FILL);
	model_viewer->set_custom_minimum_size(Vector2(100, 150 * editor_scale));
	model_viewer->set_camera_distance(1.9f);
	Node *viewer_root = model_viewer->get_viewer_root_node();

	_mesh_instance = memnew(MeshInstance3D);
	viewer_root->add_child(_mesh_instance);
}

void VoxelBlockyTypeViewer::set_combination_selector(VoxelBlockyTypeAttributeCombinationSelector *selector) {
	// Supposed to be setup only once.
	ZN_ASSERT_RETURN(_combination_selector == nullptr);
	selector->connect(VoxelBlockyTypeAttributeCombinationSelector::SIGNAL_COMBINATION_CHANGED,
			ZN_GODOT_CALLABLE_MP(this, VoxelBlockyTypeViewer, _on_combination_changed));
	_combination_selector = selector;
}

void VoxelBlockyTypeViewer::set_type(Ref<VoxelBlockyType> type) {
	if (_type.is_valid()) {
		_type->disconnect(VoxelStringNames::get_singleton().changed,
				ZN_GODOT_CALLABLE_MP(this, VoxelBlockyTypeViewer, _on_type_changed));
	}

	_type = type;

	if (_type.is_valid()) {
		_type->connect(VoxelStringNames::get_singleton().changed,
				ZN_GODOT_CALLABLE_MP(this, VoxelBlockyTypeViewer, _on_type_changed));
	}

	update_model();
}

void VoxelBlockyTypeViewer::update_model() {
	ZN_ASSERT_RETURN(_combination_selector != nullptr);
	const VoxelBlockyType::VariantKey key = _combination_selector->get_variant_key();
	// The mesh can be null
	Ref<Mesh> mesh = _type->get_preview_mesh(key);
	_mesh_instance->set_mesh(mesh);
}

void VoxelBlockyTypeViewer::_on_type_changed() {
	update_model();
}

void VoxelBlockyTypeViewer::_on_combination_changed() {
	update_model();
}

void VoxelBlockyTypeViewer::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_type_changed"), &VoxelBlockyTypeViewer::_on_type_changed);
	ClassDB::bind_method(D_METHOD("_on_combination_changed"), &VoxelBlockyTypeViewer::_on_combination_changed);
#endif
}

} // namespace zylann::voxel
