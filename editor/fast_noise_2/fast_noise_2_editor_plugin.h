#ifndef FAST_NOISE_2_EDITOR_PLUGIN_H
#define FAST_NOISE_2_EDITOR_PLUGIN_H

#include <editor/editor_plugin.h>

class FastNoise2EditorPlugin : public EditorPlugin {
	GDCLASS(FastNoise2EditorPlugin, EditorPlugin)
public:
	virtual String get_name() const {
		return "FastNoise2";
	}

	FastNoise2EditorPlugin(EditorNode *p_node);
};

#endif // FAST_NOISE_2_EDITOR_PLUGIN_H
