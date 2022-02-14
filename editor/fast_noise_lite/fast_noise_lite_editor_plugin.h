#ifndef FAST_NOISE_LITE_EDITOR_PLUGIN_H
#define FAST_NOISE_LITE_EDITOR_PLUGIN_H

#include <editor/editor_plugin.h>

namespace zylann {

class FastNoiseLiteEditorPlugin : public EditorPlugin {
	GDCLASS(FastNoiseLiteEditorPlugin, EditorPlugin)
public:
	String get_name() const override {
		return "FastNoiseLite";
	}

	FastNoiseLiteEditorPlugin();
};

} // namespace zylann

#endif // FAST_NOISE_LITE_EDITOR_PLUGIN_H
