#ifndef ZN_FAST_NOISE_LITE_EDITOR_INSPECTOR_PLUGIN_H
#define ZN_FAST_NOISE_LITE_EDITOR_INSPECTOR_PLUGIN_H

#include "../../util/godot/classes/editor_inspector_plugin.h"
#include "../../util/macros.h"
#include "fast_noise_lite_viewer.h"

namespace zylann {

class ZN_NoiseAnalysisWindow;

class ZN_FastNoiseLiteEditorInspectorPlugin : public zylann::godot::ZN_EditorInspectorPlugin {
	GDCLASS(ZN_FastNoiseLiteEditorInspectorPlugin, zylann::godot::ZN_EditorInspectorPlugin)
public:
	void set_noise_analysis_window(ZN_NoiseAnalysisWindow *noise_analysis_window) {
		_noise_analysis_window = noise_analysis_window;
	}

protected:
	bool _zn_can_handle(const Object *p_object) const override;
	void _zn_parse_begin(Object *p_object) override;

private:
	// When compiling with GodotCpp, `_bind_methods` isn't optional.
	static void _bind_methods() {}

	ZN_NoiseAnalysisWindow *_noise_analysis_window = nullptr;
};

} // namespace zylann

#endif // ZN_FAST_NOISE_LITE_EDITOR_INSPECTOR_PLUGIN_H
