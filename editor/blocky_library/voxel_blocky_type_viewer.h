#ifndef VOXEL_BLOCKY_TYPE_VIEWER_H
#define VOXEL_BLOCKY_TYPE_VIEWER_H

#include "../../meshers/blocky/types/voxel_blocky_type.h"
#include "../../util/godot/classes/h_box_container.h"
#include <utility>

ZN_GODOT_FORWARD_DECLARE(class MeshInstance3D);
ZN_GODOT_FORWARD_DECLARE(class GridContainer);
ZN_GODOT_FORWARD_DECLARE(class Label);
ZN_GODOT_FORWARD_DECLARE(class OptionButton);

namespace zylann::voxel {

class VoxelBlockyTypeViewer : public HBoxContainer {
	GDCLASS(VoxelBlockyTypeViewer, HBoxContainer)
public:
	VoxelBlockyTypeViewer();

	void set_type(Ref<VoxelBlockyType> type);

private:
	void update_model();
	void update_model(Span<const Ref<VoxelBlockyAttribute>> attributes);

	bool get_preview_attribute_value(const StringName &attrib_name, uint8_t &out_value) const;
	void remove_attribute_editor(unsigned int index);
	void update_attribute_editors();

	struct AttributeEditor {
		StringName name;
		uint8_t value;
		Label *label = nullptr;
		OptionButton *selector = nullptr;
		Ref<VoxelBlockyAttribute> attribute_copy;
	};

	bool get_attribute_editor_index(const StringName &attrib_name, unsigned int &out_index) const;

	void _on_type_changed();
	void _on_attribute_editor_value_selected(int value_index, int editor_index);

	static void _bind_methods();

	Ref<VoxelBlockyType> _type;
	MeshInstance3D *_mesh_instance = nullptr;

	std::vector<AttributeEditor> _attribute_editors;
	GridContainer *_attributes_container;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_TYPE_VIEWER_H
