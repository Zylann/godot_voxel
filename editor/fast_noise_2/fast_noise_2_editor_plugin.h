#ifndef FAST_NOISE_2_EDITOR_PLUGIN_H
#define FAST_NOISE_2_EDITOR_PLUGIN_H

#include "../../util/godot/classes/editor_plugin.h"

namespace zylann {

class NoiseAnalysisWindow;

class FastNoise2EditorPlugin : public EditorPlugin {
	GDCLASS(FastNoise2EditorPlugin, EditorPlugin)
public:
	FastNoise2EditorPlugin();

	String get_name() const override {
		return "FastNoise2";
	}

private:
	void init();
	void _notification(int p_what);

	NoiseAnalysisWindow *_noise_analysis_window = nullptr;
};

} // namespace zylann

#endif // FAST_NOISE_2_EDITOR_PLUGIN_H
