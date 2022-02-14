#ifndef VOX_EDITOR_PLUGIN_H
#define VOX_EDITOR_PLUGIN_H

#include <editor/editor_plugin.h>

namespace zylann::voxel::magica {

class VoxEditorPlugin : public EditorPlugin {
	GDCLASS(VoxEditorPlugin, EditorPlugin)
public:
	VoxEditorPlugin();
};

} // namespace zylann::voxel::magica

#endif // VOX_EDITOR_PLUGIN_H
