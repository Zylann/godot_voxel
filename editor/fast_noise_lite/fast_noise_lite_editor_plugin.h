#ifndef FAST_NOISE_LITE_EDITOR_PLUGIN_H
#define FAST_NOISE_LITE_EDITOR_PLUGIN_H

#include <editor/editor_plugin.h>

class FastNoiseLiteEditorPlugin : public EditorPlugin {
	GDCLASS(FastNoiseLiteEditorPlugin, EditorPlugin)
public:
	virtual String get_name() const { return "FastNoiseLite"; }

	FastNoiseLiteEditorPlugin(EditorNode *p_node);
};

#endif // FAST_NOISE_LITE_EDITOR_PLUGIN_H
