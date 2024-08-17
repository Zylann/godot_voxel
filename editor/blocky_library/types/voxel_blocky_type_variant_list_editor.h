#ifndef VOXEL_BLOCKY_TYPE_VARIANT_LIST_EDITOR_H
#define VOXEL_BLOCKY_TYPE_VARIANT_LIST_EDITOR_H

#include "../../../meshers/blocky/types/voxel_blocky_type.h"
#include "../../../util/containers/std_vector.h"
#include "../../../util/godot/classes/v_box_container.h"

ZN_GODOT_FORWARD_DECLARE(class Label);
ZN_GODOT_FORWARD_DECLARE(class EditorResourcePicker);
ZN_GODOT_FORWARD_DECLARE(class GridContainer);
ZN_GODOT_FORWARD_DECLARE(class EditorInterface);
ZN_GODOT_FORWARD_DECLARE(class EditorUndoRedoManager);

namespace zylann::voxel {

// Allows to edit a map of attribute combination and associated models.
// This cannot be exposed as regular properties, therefore it is a custom comtrol.
class VoxelBlockyTypeVariantListEditor : public VBoxContainer {
	GDCLASS(VoxelBlockyTypeVariantListEditor, VBoxContainer)
public:
	VoxelBlockyTypeVariantListEditor();

	void set_type(Ref<VoxelBlockyType> type);
	void set_editor_interface(EditorInterface *ed);
	void set_undo_redo(EditorUndoRedoManager *undo_redo);

private:
	void update_list();

	void _on_type_changed();
	void _on_model_changed(Ref<VoxelBlockyModel> model, int editor_index);
	void _on_model_picker_selected(Ref<VoxelBlockyModel> model, bool inspect);

	static void _bind_methods();

	Ref<VoxelBlockyType> _type;
	Label *_header_label = nullptr;

	struct VariantEditor {
		Label *key_label = nullptr;
		EditorResourcePicker *resource_picker = nullptr;
		VoxelBlockyType::VariantKey key;
	};

	StdVector<VariantEditor> _variant_editors;
	GridContainer *_grid_container = nullptr;
	EditorInterface *_editor_interface = nullptr;
	EditorUndoRedoManager *_undo_redo = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_TYPE_VARIANT_LIST_EDITOR_H
