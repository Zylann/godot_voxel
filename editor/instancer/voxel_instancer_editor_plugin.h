#ifndef VOXEL_INSTANCER_EDITOR_PLUGIN_H
#define VOXEL_INSTANCER_EDITOR_PLUGIN_H

#include <editor/editor_plugin.h>

class VoxelInstancer;

class VoxelInstancerEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelInstancerEditorPlugin, EditorPlugin)
public:
	VoxelInstancerEditorPlugin(EditorNode *p_node);

	bool handles(Object *p_object) const override;
	void edit(Object *p_object) override;
	void make_visible(bool visible) override;

private:
	VoxelInstancer *_node = nullptr;
};

#endif // VOXEL_INSTANCER_EDITOR_PLUGIN_H
