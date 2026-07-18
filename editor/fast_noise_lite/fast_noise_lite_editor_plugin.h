#ifndef ZN_FAST_NOISE_LITE_EDITOR_PLUGIN_H
#define ZN_FAST_NOISE_LITE_EDITOR_PLUGIN_H

#include "../../util/godot/classes/editor_plugin.h"

namespace zylann {

class ZN_NoiseAnalysisWindow;

class ZN_FastNoiseLiteEditorPlugin : public zylann::godot::ZN_EditorPlugin {
	GDCLASS(ZN_FastNoiseLiteEditorPlugin, zylann::godot::ZN_EditorPlugin)
public:
	ZN_FastNoiseLiteEditorPlugin();

protected:
	String _zn_get_plugin_name() const override;

private:
	void _notification(int p_what);

	// When compiling with GodotCpp, `_bind_methods` is not optional
	static void _bind_methods() {}

	ZN_NoiseAnalysisWindow *_noise_analysis_window = nullptr;
};

} // namespace zylann

#endif // ZN_FAST_NOISE_LITE_EDITOR_PLUGIN_H
