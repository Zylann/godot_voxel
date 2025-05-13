#ifndef VOXEL_PROCEDURAL_CUBEMAP_VIEWER_H
#define VOXEL_PROCEDURAL_CUBEMAP_VIEWER_H

#include "../../generators/graph/procedural_cubemap.h"
#include "../../util/godot/classes/shader_material.h"
#include "../../util/godot/classes/v_box_container.h"

ZN_GODOT_NAMESPACE_BEGIN
class EditorFileDialog;
ZN_GODOT_NAMESPACE_END

namespace zylann::voxel {

class VoxelProceduralCubemapViewer : public VBoxContainer {
	GDCLASS(VoxelProceduralCubemapViewer, VBoxContainer)
public:
	VoxelProceduralCubemapViewer();

	void set_cubemap(Ref<VoxelProceduralCubemap> cm);

private:
	void on_cubemap_updated();
	void on_update_button_pressed();
	void on_save_button_pressed();
	void on_save_dialog_file_selected(const String &fpath);

	static void _bind_methods();

	Ref<VoxelProceduralCubemap> _cubemap;
	Ref<ShaderMaterial> _material;
	EditorFileDialog *_save_dialog = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_PROCEDURAL_CUBEMAP_VIEWER_H
