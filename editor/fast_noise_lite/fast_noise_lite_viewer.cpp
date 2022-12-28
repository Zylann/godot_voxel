#include "fast_noise_lite_viewer.h"
#include "../../util/godot/classes/image.h"
#include "../../util/godot/classes/image_texture.h"
#include "../../util/godot/classes/node.h"
#include "../../util/godot/classes/texture_rect.h"
#include "../../util/godot/core/callable.h"
#include "../../util/godot/editor_scale.h"

namespace zylann {

ZN_FastNoiseLiteViewer::ZN_FastNoiseLiteViewer() {
	set_custom_minimum_size(Vector2(0, EDSCALE * PREVIEW_HEIGHT));

	_texture_rect = memnew(TextureRect);
	_texture_rect->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	_texture_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_COVERED);
	add_child(_texture_rect);
}

void ZN_FastNoiseLiteViewer::set_noise(Ref<ZN_FastNoiseLite> noise) {
	if (_noise == noise) {
		return;
	}

	if (_noise.is_valid()) {
		_noise->disconnect("changed", ZN_GODOT_CALLABLE_MP(this, ZN_FastNoiseLiteViewer, _on_noise_changed));
	}

	_noise = noise;

	if (_noise.is_valid()) {
		set_noise_gradient(Ref<ZN_FastNoiseLiteGradient>());
		_noise->connect("changed", ZN_GODOT_CALLABLE_MP(this, ZN_FastNoiseLiteViewer, _on_noise_changed));
		set_process(true);
		update_preview();

	} else {
		set_process(false);
		_time_before_update = -1.f;
	}
}

void ZN_FastNoiseLiteViewer::set_noise_gradient(Ref<ZN_FastNoiseLiteGradient> noise_gradient) {
	if (_noise_gradient == noise_gradient) {
		return;
	}

	if (_noise_gradient.is_valid()) {
		_noise_gradient->disconnect("changed", ZN_GODOT_CALLABLE_MP(this, ZN_FastNoiseLiteViewer, _on_noise_changed));
	}

	_noise_gradient = noise_gradient;

	if (_noise_gradient.is_valid()) {
		set_noise(Ref<ZN_FastNoiseLite>());
		_noise_gradient->connect("changed", ZN_GODOT_CALLABLE_MP(this, ZN_FastNoiseLiteViewer, _on_noise_changed));
		set_process(true);
		update_preview();

	} else {
		set_process(false);
		_time_before_update = -1.f;
	}
}

void ZN_FastNoiseLiteViewer::_on_noise_changed() {
	_time_before_update = 0.5f;
}

void ZN_FastNoiseLiteViewer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS: {
			if (_time_before_update > 0.f) {
				_time_before_update -= get_process_delta_time();
				if (_time_before_update <= 0.f) {
					update_preview();
				}
			}
		} break;
	}
}

// TODO Use thread?
void ZN_FastNoiseLiteViewer::update_preview() {
	const Vector2i preview_size(PREVIEW_WIDTH, PREVIEW_HEIGHT);

	Ref<Image> im = create_empty_image(preview_size.x, preview_size.y, false, Image::FORMAT_RGB8);

	if (_noise.is_valid()) {
		for (int y = 0; y < preview_size.y; ++y) {
			for (int x = 0; x < preview_size.x; ++x) {
				// Assuming -1..1 output. Some noise types can have different range though.
				const float v = _noise->get_noise_2d(x, y);
				const float g = 0.5f * v + 0.5f;
				im->set_pixel(x, y, Color(g, g, g));
			}
		}

	} else if (_noise_gradient.is_valid()) {
		const float amp = _noise_gradient->get_amplitude();
		float m = (amp == 0.f ? 1.f : 1.f / amp);
		for (int y = 0; y < preview_size.y; ++y) {
			for (int x = 0; x < preview_size.x; ++x) {
				const float x0 = x;
				const float y0 = y;
				real_t x1 = x0;
				real_t y1 = y0;
				_noise_gradient->warp_2d(x1, y1);
				const float dx = x1 - x0;
				const float dy = y1 - y0;
				const float r = 0.5f * m * dx + 0.5f;
				const float g = 0.5f * m * dy + 0.5f;
				im->set_pixel(x, y, Color(r, g, 0.0f));
			}
		}
	}

	Ref<ImageTexture> tex = ImageTexture::create_from_image(im);
	_texture_rect->set_texture(tex);
}

void ZN_FastNoiseLiteViewer::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_noise_changed"), &ZN_FastNoiseLiteViewer::_on_noise_changed);
#endif
}

} // namespace zylann
