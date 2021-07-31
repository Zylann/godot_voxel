#ifndef VOX_EDITOR_PLUGIN_H
#define VOX_EDITOR_PLUGIN_H

#include <editor/editor_plugin.h>

class VoxEditorPlugin : public EditorPlugin {
	GDCLASS(VoxEditorPlugin, EditorPlugin)
public:
	VoxEditorPlugin(EditorNode *p_node);
};

#endif // VOX_EDITOR_PLUGIN_H
