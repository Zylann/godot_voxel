#ifndef VOXEL_INSTANCER_STAT_VIEW_H
#define VOXEL_INSTANCER_STAT_VIEW_H

#include "../../util/godot/classes/v_box_container.h"
#include "../../util/macros.h"
#include <unordered_map>

ZN_GODOT_FORWARD_DECLARE(class Tree)

namespace zylann::voxel {

class VoxelInstancer;

class VoxelInstancerStatView : public VBoxContainer {
	GDCLASS(VoxelInstancerStatView, VBoxContainer)
public:
	VoxelInstancerStatView();

	void set_instancer(const VoxelInstancer *instancer);

private:
	void _notification(int p_what);

	void process();

	// When compiling with GodotCpp, `_bind_methods` is not optional.
	static void _bind_methods() {}

	Tree *_tree = nullptr;
	const VoxelInstancer *_instancer = nullptr;
	std::unordered_map<uint32_t, uint32_t> _count_per_layer;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCER_STAT_VIEW_H
