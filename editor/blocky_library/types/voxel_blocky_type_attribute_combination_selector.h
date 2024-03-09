#ifndef VOXEL_BLOCKY_TYPE_ATTRIBUTE_COMBINATION_SELECTOR_H
#define VOXEL_BLOCKY_TYPE_ATTRIBUTE_COMBINATION_SELECTOR_H

#include "../../../meshers/blocky/types/voxel_blocky_type.h"
#include "../../../util/containers/std_vector.h"
#include "../../../util/godot/classes/grid_container.h"

ZN_GODOT_FORWARD_DECLARE(class OptionButton);
ZN_GODOT_FORWARD_DECLARE(class Label);

namespace zylann::voxel {

// Editor with the list of attributes from a specific VoxelBlockyType, allowing to choose a combination
// parametrically.
class VoxelBlockyTypeAttributeCombinationSelector : public GridContainer {
	GDCLASS(VoxelBlockyTypeAttributeCombinationSelector, GridContainer)
public:
	static const char *SIGNAL_COMBINATION_CHANGED;

	VoxelBlockyTypeAttributeCombinationSelector();

	void set_type(Ref<VoxelBlockyType> type);

	VoxelBlockyType::VariantKey get_variant_key() const;

private:
	bool get_preview_attribute_value(const StringName &attrib_name, uint8_t &out_value) const;
	void remove_attribute_editor(unsigned int index);
	void update_attribute_editors();

	bool get_attribute_editor_index(const StringName &attrib_name, unsigned int &out_index) const;

	void _on_type_changed();
	void _on_attribute_editor_value_selected(int value_index, int editor_index);

	static void _bind_methods();

	struct AttributeEditor {
		StringName name;
		uint8_t value;
		Label *label = nullptr;
		OptionButton *selector = nullptr;
		Ref<VoxelBlockyAttribute> attribute_copy;
	};

	Ref<VoxelBlockyType> _type;
	StdVector<AttributeEditor> _attribute_editors;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_TYPE_ATTRIBUTE_COMBINATION_SELECTOR_H
