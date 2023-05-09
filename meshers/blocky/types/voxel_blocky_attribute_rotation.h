#ifndef VOXEL_BLOCKY_ATTRIBUTE_ROTATION_H
#define VOXEL_BLOCKY_ATTRIBUTE_ROTATION_H

#include "voxel_blocky_attribute.h"

namespace zylann::voxel {

// `rotation`
class VoxelBlockyAttributeRotation : public VoxelBlockyAttribute {
	GDCLASS(VoxelBlockyAttributeRotation, VoxelBlockyAttribute)
public:
	VoxelBlockyAttributeRotation();

	void set_horizontal_roll_enabled(bool enable);
	bool is_horizontal_roll_enabled() const;

private:
	void update_values();

	static void _bind_methods();

	bool _horizontal_roll_enabled = false;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_ATTRIBUTE_ROTATION_H
