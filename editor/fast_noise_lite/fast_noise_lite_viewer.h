#ifndef ZN_FAST_NOISE_LITE_VIEWER_H
#define ZN_FAST_NOISE_LITE_VIEWER_H

#include "../../util/godot/classes/control.h"
#include "../../util/macros.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"

ZN_GODOT_FORWARD_DECLARE(class TextureRect)

namespace zylann {

class ZN_FastNoiseLiteViewer : public Control {
	GDCLASS(ZN_FastNoiseLiteViewer, Control)
public:
	static const int PREVIEW_WIDTH = 300;
	static const int PREVIEW_HEIGHT = 150;

	ZN_FastNoiseLiteViewer();

	void set_noise(Ref<ZN_FastNoiseLite> noise);
	void set_noise_gradient(Ref<ZN_FastNoiseLiteGradient> noise_gradient);

private:
	void _on_noise_changed();
	void _notification(int p_what);

	void update_preview();

	static void _bind_methods();

	Ref<ZN_FastNoiseLite> _noise;
	Ref<ZN_FastNoiseLiteGradient> _noise_gradient;
	float _time_before_update = -1.f;
	TextureRect *_texture_rect = nullptr;
};

} // namespace zylann

#endif // ZN_FAST_NOISE_LITE_VIEWER_H
