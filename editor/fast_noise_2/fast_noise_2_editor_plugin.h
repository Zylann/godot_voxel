#ifndef FAST_NOISE_2_EDITOR_PLUGIN_H
#define FAST_NOISE_2_EDITOR_PLUGIN_H

#include "../../util/godot/classes/editor_plugin.h"

namespace zylann {

class NoiseAnalysisWindow;

class FastNoise2EditorPlugin : public zylann::godot::ZN_EditorPlugin {
	GDCLASS(FastNoise2EditorPlugin, zylann::godot::ZN_EditorPlugin)
public:
	FastNoise2EditorPlugin();

protected:
	String _zn_get_plugin_name() const override {
		return "FastNoise2";
	}

private:
	void init();
	void _notification(int p_what);

	static void _bind_methods() {}

	NoiseAnalysisWindow *_noise_analysis_window = nullptr;
};

} // namespace zylann

#endif // FAST_NOISE_2_EDITOR_PLUGIN_H
