#ifndef VOXEL_BLOCKY_ATTRIBUTE_AXIS_H
#define VOXEL_BLOCKY_ATTRIBUTE_AXIS_H

#include "voxel_blocky_attribute.h"

namespace zylann::voxel {

// `axis`
class VoxelBlockyAttributeAxis : public VoxelBlockyAttribute {
	GDCLASS(VoxelBlockyAttributeAxis, VoxelBlockyAttribute)
public:
	enum Axis { AXIS_X, AXIS_Y, AXIS_Z, AXIS_COUNT };

	VoxelBlockyAttributeAxis();

	void set_horizontal_only(bool h);
	bool is_horizontal_only() const;

	int from_vec3(Vector3 v) const;

private:
	void update_values();

	static void _bind_methods();

	bool _horizontal_only = false;
	// TODO Corresponding ortho rotations
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelBlockyAttributeAxis::Axis);

#endif // VOXEL_BLOCKY_ATTRIBUTE_AXIS_H
