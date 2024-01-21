#ifndef ZN_SPOT_NOISE_VIEWER_H
#define ZN_SPOT_NOISE_VIEWER_H

#include "../../util/godot/classes/control.h"
#include "../../util/godot/macros.h"
#include "../../util/noise/spot_noise_gd.h"

ZN_GODOT_FORWARD_DECLARE(class TextureRect)

namespace zylann {

class ZN_SpotNoiseViewer : public Control {
	GDCLASS(ZN_SpotNoiseViewer, Control)
public:
	static const int PREVIEW_WIDTH = 300;
	static const int PREVIEW_HEIGHT = 150;

	ZN_SpotNoiseViewer();

	void set_noise(Ref<ZN_SpotNoise> noise);

private:
	void _on_noise_changed();
	void _notification(int p_what);

	void update_preview();

	static void _bind_methods();

	Ref<ZN_SpotNoise> _noise;
	float _time_before_update = -1.f;
	TextureRect *_texture_rect = nullptr;
};

} // namespace zylann

#endif // ZN_SPOT_NOISE_VIEWER_H
