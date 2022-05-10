#ifndef VOXEL_MESH_SDF_VIEWER_H
#define VOXEL_MESH_SDF_VIEWER_H

#include "../../edition/voxel_mesh_sdf_gd.h"
#include <scene/gui/box_container.h>

class TextureRect;
class Button;
class SpinBox;

namespace zylann::voxel {

class VoxelMeshSDFViewer : public VBoxContainer {
	GDCLASS(VoxelMeshSDFViewer, VBoxContainer)
public:
	VoxelMeshSDFViewer();

	void set_mesh_sdf(Ref<VoxelMeshSDF> mesh_sdf);

	void update_view();

private:
	void _on_bake_button_pressed();
	void _on_mesh_sdf_baked();
	void _on_slice_spinbox_value_changed(float value);

	void update_bake_button();
	void update_info_label();
	void center_slice_y();
	// void clamp_slice_y();
	void update_slice_spinbox();

	TextureRect *_texture_rect = nullptr;
	Label *_info_label = nullptr;
	SpinBox *_slice_spinbox = nullptr;
	int _slice_y = 0;
	bool _slice_spinbox_ignored = false;
	Button *_bake_button = nullptr;
	Ref<VoxelMeshSDF> _mesh_sdf;
	Vector3i _size_before_baking;
};

} // namespace zylann::voxel

#endif // VOXEL_MESH_SDF_VIEWER_H
