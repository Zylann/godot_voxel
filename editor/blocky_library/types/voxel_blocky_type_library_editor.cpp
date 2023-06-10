#if 0
#include "voxel_blocky_type_library_editor.h"
#include "../../../util/godot/classes/h_box_container.h"
#include "../../../util/godot/classes/h_separator.h"
#include "../../../util/godot/classes/h_split_container.h"
#include "../../../util/godot/classes/item_list.h"
#include "../../../util/godot/classes/label.h"
#include "../../../util/godot/classes/scroll_container.h"
#include "../../../util/godot/core/string.h"
#include "../../../util/godot/editor_scale.h"
#include "voxel_blocky_type_attribute_combination_selector.h"
#include "voxel_blocky_type_viewer.h"

namespace zylann::voxel {

VoxelBlockyTypeLibraryEditor::VoxelBlockyTypeLibraryEditor() {
	const float editor_scale = EDSCALE;

	{
		HBoxContainer *toolbar_container = memnew(HBoxContainer);
		add_child(toolbar_container);
	}

	HSplitContainer *split_container_1 = memnew(HSplitContainer);
	split_container_1->set_split_offset(200 * editor_scale);

	{
		VBoxContainer *types_container = memnew(VBoxContainer);

		Label *label = memnew(Label);
		label->set_text(ZN_TTR("Types"));
		types_container->add_child(label);

		ItemList *types_item_list = memnew(ItemList);
		types_item_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		types_container->add_child(types_item_list);

		Label *id_space_label = memnew(Label);
		id_space_label->set_text("ID space: -----/-----");
		types_container->add_child(id_space_label);

		split_container_1->add_child(types_container);
	}

	HSplitContainer *split_container_2 = memnew(HSplitContainer);
	split_container_2->set_split_offset(-300 * editor_scale);

	{
		VoxelBlockyTypeViewer *viewer = memnew(VoxelBlockyTypeViewer);
		split_container_2->add_child(viewer);

		VBoxContainer *vcontainer = memnew(VBoxContainer);

		ScrollContainer *scroll_container = memnew(ScrollContainer);
		scroll_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		vcontainer->add_child(scroll_container);

		HSeparator *separator = memnew(HSeparator);
		vcontainer->add_child(separator);

		VoxelBlockyTypeAttributeCombinationSelector *combination_selector =
				memnew(VoxelBlockyTypeAttributeCombinationSelector);
		vcontainer->add_child(combination_selector);

		split_container_2->add_child(vcontainer);
	}

	split_container_1->add_child(split_container_2);

	add_child(split_container_1);
}

void VoxelBlockyTypeLibraryEditor::set_library(Ref<VoxelBlockyTypeLibrary> library) {
	if (_library == library) {
		return;
	}
	_library = library;
	// TODO Refresh
}

void VoxelBlockyTypeLibraryEditor::_bind_methods() {
	// TODO
}

} // namespace zylann::voxel
#endif
