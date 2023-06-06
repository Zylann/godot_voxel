#ifndef VOXEL_BLOCKY_ATTRIBUTE_CUSTOM_H
#define VOXEL_BLOCKY_ATTRIBUTE_CUSTOM_H

#include "voxel_blocky_attribute.h"

namespace zylann::voxel {

class VoxelBlockyAttributeCustom : public VoxelBlockyAttribute {
	GDCLASS(VoxelBlockyAttributeCustom, VoxelBlockyAttribute)
public:
	VoxelBlockyAttributeCustom();

	void set_attribute_name(StringName p_name);
	void set_value_count(int count);
	void set_value_name(int index, StringName p_name);
	void set_default_value(int v);

	// Not exposing rotation like that, because we can't automate this properly at the moment (in the case of rails,
	// there is rotation, but it is uneven as several values have the same rotation but different models). Users
	// will have to rotate the model manually using editor tools, which gives more control.
	// An easier approach is to separate rotation from rail shape as two attributes, but it will waste a few model IDs
	// for straight shapes.
	//
	// void set_is_rotation(bool is_rotation);
	// void set_value_ortho_rotation(int index, int ortho_rotation_index);

private:
	void update_values();

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	static void _bind_methods();
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_ATTRIBUTE_CUSTOM_H
