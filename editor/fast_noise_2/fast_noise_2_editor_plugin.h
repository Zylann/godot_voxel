#ifndef FAST_NOISE_2_EDITOR_PLUGIN_H
#define FAST_NOISE_2_EDITOR_PLUGIN_H

#include <editor/editor_plugin.h>

namespace zylann {

class NoiseAnalysisWindow;

class FastNoise2EditorPlugin : public EditorPlugin {
	GDCLASS(FastNoise2EditorPlugin, EditorPlugin)
public:
	FastNoise2EditorPlugin();

	virtual String get_name() const {
		return "FastNoise2";
	}

private:
	NoiseAnalysisWindow *_noise_analysis_window = nullptr;
};

} // namespace zylann

#endif // FAST_NOISE_2_EDITOR_PLUGIN_H
