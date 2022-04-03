#ifndef ZN_FAST_NOISE_LITE_EDITOR_PLUGIN_H
#define ZN_FAST_NOISE_LITE_EDITOR_PLUGIN_H

#include <editor/editor_plugin.h>

namespace zylann {

class ZN_FastNoiseLiteEditorPlugin : public EditorPlugin {
	GDCLASS(ZN_FastNoiseLiteEditorPlugin, EditorPlugin)
public:
	String get_name() const override;

	ZN_FastNoiseLiteEditorPlugin();
};

} // namespace zylann

#endif // ZN_FAST_NOISE_LITE_EDITOR_PLUGIN_H
