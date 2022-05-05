#ifndef VOXEL_MESH_SDF_VIEWER_H
#define VOXEL_MESH_SDF_VIEWER_H

#include "../../edition/voxel_mesh_sdf_gd.h"
#include <scene/gui/box_container.h>

class TextureRect;
class Button;

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

	void update_bake_button();
	void update_info_label();

	TextureRect *_texture_rect = nullptr;
	Label *_info_label = nullptr;
	Button *_bake_button = nullptr;
	Ref<VoxelMeshSDF> _mesh_sdf;
};

} // namespace zylann::voxel

#endif // VOXEL_MESH_SDF_VIEWER_H
