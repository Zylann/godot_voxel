#include "voxel_blocky_type_variant_list_editor.h"
#include "../../../constants/voxel_string_names.h"
#include "../../../util/godot/classes/color_rect.h"
#include "../../../util/godot/classes/editor_inspector.h"
#include "../../../util/godot/classes/editor_interface.h"
#include "../../../util/godot/classes/editor_resource_picker.h"
#include "../../../util/godot/classes/editor_undo_redo_manager.h"
#include "../../../util/godot/classes/grid_container.h"
#include "../../../util/godot/classes/label.h"
#include "../../../util/godot/core/array.h"
#include "../../../util/godot/core/string.h"

namespace zylann::voxel {

VoxelBlockyTypeVariantListEditor::VoxelBlockyTypeVariantListEditor() {
	_header_label = memnew(Label);
	_header_label->set_text(ZN_TTR("Variant models"));
	add_child(_header_label);

	_grid_container = memnew(GridContainer);
	_grid_container->set_columns(2);
	add_child(_grid_container);
}

void VoxelBlockyTypeVariantListEditor::set_type(Ref<VoxelBlockyType> type) {
	if (_type.is_valid()) {
		_type->disconnect(VoxelStringNames::get_singleton().changed,
				callable_mp(this, &VoxelBlockyTypeVariantListEditor::_on_type_changed));
	}

	_type = type;

	if (_type.is_valid()) {
		_type->connect(VoxelStringNames::get_singleton().changed,
				callable_mp(this, &VoxelBlockyTypeVariantListEditor::_on_type_changed));
	}

	update_list();
}

void VoxelBlockyTypeVariantListEditor::set_editor_interface(EditorInterface *ed) {
	_editor_interface = ed;
}

void VoxelBlockyTypeVariantListEditor::set_undo_redo(EditorUndoRedoManager *undo_redo) {
	_undo_redo = undo_redo;
}

void VoxelBlockyTypeVariantListEditor::update_list() {
	for (VariantEditor &ed : _variant_editors) {
		ed.key_label->queue_free();
		ed.resource_picker->queue_free();
	}

	_variant_editors.clear();

	ZN_ASSERT_RETURN(_type.is_valid());

	StdVector<VoxelBlockyType::VariantKey> keys;
	_type->generate_keys(keys, false);

	const int displayed_count = keys.size() <= 1 ? 0 : keys.size();
	_header_label->set_text(ZN_TTR("Variant models ({0})").format(varray(displayed_count)));

	if (keys.size() <= 1) {
		return;
	}

	StdVector<Ref<VoxelBlockyAttribute>> attributes;
	_type->get_checked_attributes(attributes);

	GridContainer &container = *_grid_container;

	for (const VoxelBlockyType::VariantKey &key : keys) {
		VariantEditor ed;
		ed.key = key;

		ed.key_label = memnew(Label);
		ed.key_label->set_text(ed.key.to_string(to_span(attributes)));

		Ref<VoxelBlockyModel> model = _type->get_variant(key);

		const int editor_index = _variant_editors.size();

		ed.resource_picker = memnew(EditorResourcePicker);
		ed.resource_picker->set_base_type(VoxelBlockyModel::get_class_static());
		// TODO It is currently not possible to integrate a sub-EditorInspector because it is not exposed to extensions.
		// So users will have to leave the heavily-nested type inspector just to edit models from here, and use previous
		// buttons to (hopefully) come back where they were
		ed.resource_picker->set_edited_resource(model);
		ed.resource_picker->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		ed.resource_picker->connect("resource_changed",
				callable_mp(this, &VoxelBlockyTypeVariantListEditor::_on_model_changed).bind(editor_index));
		ed.resource_picker->connect(
				"resource_selected", callable_mp(this, &VoxelBlockyTypeVariantListEditor::_on_model_picker_selected));

		container.add_child(ed.key_label);
		container.add_child(ed.resource_picker);

		_variant_editors.push_back(ed);
	}
}

void VoxelBlockyTypeVariantListEditor::_on_type_changed() {
	// This is in case the list of variants changes. We don't actually care about other changes...
	update_list();
}

void VoxelBlockyTypeVariantListEditor::_on_model_changed(Ref<VoxelBlockyModel> model, int editor_index) {
	ZN_ASSERT_RETURN(_type.is_valid());
	ZN_ASSERT_RETURN(_undo_redo != nullptr);

	EditorUndoRedoManager *urm = _undo_redo;

	const VoxelBlockyType::VariantKey &key = _variant_editors[editor_index].key;
	Ref<VoxelBlockyModel> prev_model = _type->get_variant(key);

	const Array key_as_array = key.to_array();

	urm->create_action("Set variant model");

	urm->add_do_method(_type.ptr(), "set_variant_model", key_as_array, model);
	urm->add_undo_method(_type.ptr(), "set_variant_model", key_as_array, prev_model);

	urm->commit_action();
}

void VoxelBlockyTypeVariantListEditor::_on_model_picker_selected(Ref<VoxelBlockyModel> model, bool inspect) {
	if (model.is_null()) {
		return;
	}
	// TODO Can't unfold as a sub-inspector, Godot does not expose it to extensions and it seems to be a fairly
	// complicated logic (it's far from just a "create inspector and unfold" inside the selected signal)... This is one
	// reason why this sole feature would require an entirely separated editor, which is incredibly frustrating
	ZN_ASSERT_RETURN(_editor_interface != nullptr);
	// Can't call this directly because somehow it crashes Godot later in `_physics_process`???
	// The current method isn't even in the call stack when this happens... so why would call_deferred even be proven to
	// fix it? Nevertheless, it seems to workaround it...
	//_editor_interface->inspect_object(model.ptr());
	_editor_interface->call_deferred("inspect_object", model);
}

void VoxelBlockyTypeVariantListEditor::_bind_methods() {}

} // namespace zylann::voxel
