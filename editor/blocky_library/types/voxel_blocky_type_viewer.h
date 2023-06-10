#ifndef VOXEL_BLOCKY_TYPE_VIEWER_H
#define VOXEL_BLOCKY_TYPE_VIEWER_H

#include "../../../meshers/blocky/types/voxel_blocky_type.h"
#include "../../../util/godot/classes/h_box_container.h"
#include "../model_viewer.h"

ZN_GODOT_FORWARD_DECLARE(class MeshInstance3D);

namespace zylann::voxel {

class VoxelBlockyTypeAttributeCombinationSelector;

// 3D viewer specialized to inspect blocky types.
class VoxelBlockyTypeViewer : public ZN_ModelViewer {
	GDCLASS(VoxelBlockyTypeViewer, ZN_ModelViewer)
public:
	VoxelBlockyTypeViewer();

	void set_combination_selector(VoxelBlockyTypeAttributeCombinationSelector *selector);
	void set_type(Ref<VoxelBlockyType> type);
	void update_model();

private:
	void _on_type_changed();
	void _on_combination_changed();

	static void _bind_methods();

	Ref<VoxelBlockyType> _type;
	MeshInstance3D *_mesh_instance = nullptr;
	const VoxelBlockyTypeAttributeCombinationSelector *_combination_selector = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_TYPE_VIEWER_H
