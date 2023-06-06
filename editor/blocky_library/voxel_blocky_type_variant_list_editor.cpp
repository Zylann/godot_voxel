#include "voxel_blocky_type_variant_list_editor.h"
#include "../../../constants/voxel_string_names.h"
#include "../../../util/godot/classes/editor_resource_picker.h"
#include "../../../util/godot/classes/editor_undo_redo_manager.h"
#include "../../../util/godot/classes/grid_container.h"
#include "../../../util/godot/classes/label.h"
#include "../../../util/godot/core/callable.h"
#include "../../../util/godot/core/string.h"

namespace zylann::voxel {

VoxelBlockyTypeVariantListEditor::VoxelBlockyTypeVariantListEditor() {
	Label *label = memnew(Label);
	label->set_text(ZN_TTR("Variant models"));
	add_child(label);

	_grid_container = memnew(GridContainer);
	_grid_container->set_columns(2);
	add_child(_grid_container);
}

void VoxelBlockyTypeVariantListEditor::set_type(Ref<VoxelBlockyType> type) {
	if (_type.is_valid()) {
		_type->disconnect(VoxelStringNames::get_singleton().changed,
				ZN_GODOT_CALLABLE_MP(this, VoxelBlockyTypeVariantListEditor, _on_type_changed));
	}

	_type = type;

	if (_type.is_valid()) {
		_type->connect(VoxelStringNames::get_singleton().changed,
				ZN_GODOT_CALLABLE_MP(this, VoxelBlockyTypeVariantListEditor, _on_type_changed));
	}

	update_list();
}

void VoxelBlockyTypeVariantListEditor::update_list() {
	for (VariantEditor &ed : _variant_editors) {
		ed.key_label->queue_free();
		ed.resource_picker->queue_free();
	}

	_variant_editors.clear();

	ZN_ASSERT_RETURN(_type.is_valid());

	std::vector<VoxelBlockyType::VariantKey> keys;
	_type->generate_keys(keys, false);

	GridContainer &container = *_grid_container;

	for (const VoxelBlockyType::VariantKey &key : keys) {
		VariantEditor ed;
		ed.key = key;

		ed.key_label = memnew(Label);
		ed.key_label->set_text(ed.key.to_string());

		Ref<VoxelBlockyModel> model = _type->get_variant(key);

		const int editor_index = _variant_editors.size();

		ed.resource_picker = memnew(EditorResourcePicker);
		ed.resource_picker->set_base_type(VoxelBlockyModel::get_class_static());
		// TODO It is currently not possible to integrate a sub-EditorInspector because it is not exposed to extensions.
		// So users will have to leave the heavily-nested type inspector just to edit models from here, and use previous
		// buttons to (hopefully) come back where they were
		ed.resource_picker->set_edited_resource(model);
		ed.resource_picker->set_h_size_flags(Control::SIZE_EXPAND_FILL);
#if defined(ZN_GODOT)
		ed.resource_picker->connect("resource_changed",
				ZN_GODOT_CALLABLE_MP(this, VoxelBlockyTypeVariantListEditor, _on_model_changed).bind(editor_index));
		ed.resource_picker->connect("resource_selected",
				ZN_GODOT_CALLABLE_MP(this, VoxelBlockyTypeVariantListEditor, _on_model_picker_selected)
						.bind(editor_index));
#elif defined(ZN_GODOT_EXTENSION)
		// TODO GDX: `Callable::bind()` isn't implemented in GodotCpp
		ZN_PRINT_ERROR("`Callable::bind()` isn't working in GodotCpp! Can't handle selecting variant models");
#endif

		container.add_child(ed.key_label);
		container.add_child(ed.resource_picker);
	}
}

void VoxelBlockyTypeVariantListEditor::_on_type_changed() {
	// This is in case the list of variants changes. We don't actually care about other changes...
	update_list();
}

void VoxelBlockyTypeVariantListEditor::_on_model_changed(Ref<VoxelBlockyModel> model, int editor_index) {
	ZN_ASSERT_RETURN(_type.is_valid());

	EditorUndoRedoManager *urm = EditorUndoRedoManager::get_singleton();

	const VoxelBlockyType::VariantKey &key = _variant_editors[editor_index].key;
	Ref<VoxelBlockyModel> prev_model = _type->get_variant(key);

	const Array key_as_array = key.to_array();

	urm->create_action("Set variant model");

	urm->add_do_method(_type.ptr(), "set_variant_model", key_as_array, model);
	urm->add_undo_method(_type.ptr(), "set_variant_model", key_as_array, prev_model);

	urm->commit_action();
}

void VoxelBlockyTypeVariantListEditor::_on_model_picker_selected(
		Ref<VoxelBlockyModel> model, bool inspect, int editor_index) {
	// TODO Inspect?
}

void VoxelBlockyTypeVariantListEditor::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_type_changed"), &VoxelBlockyTypeVariantListEditor::_on_type_changed);
	ClassDB::bind_method(D_METHOD("_on_model_changed", "model", "editor_index"),
			&VoxelBlockyTypeVariantListEditor::_on_model_changed);
	ClassDB::bind_method(D_METHOD("_on_model_picker_selected", "model", "inspect", "editor_index"),
			&VoxelBlockyTypeVariantListEditor::_on_model_picker_selected);
#endif
}

} // namespace zylann::voxel
