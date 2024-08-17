#include "spot_noise_viewer.h"
#include "../../util/godot/classes/button.h"
#include "../../util/godot/classes/image.h"
#include "../../util/godot/classes/image_texture.h"
#include "../../util/godot/classes/node.h"
#include "../../util/godot/classes/texture_rect.h"
#include "../../util/godot/editor_scale.h"

namespace zylann {

ZN_SpotNoiseViewer::ZN_SpotNoiseViewer() {
	set_custom_minimum_size(Vector2(0, EDSCALE * PREVIEW_HEIGHT));

	_texture_rect = memnew(TextureRect);
	_texture_rect->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	_texture_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_COVERED);
	add_child(_texture_rect);
}

void ZN_SpotNoiseViewer::set_noise(Ref<ZN_SpotNoise> noise) {
	if (_noise == noise) {
		return;
	}

	if (_noise.is_valid()) {
		_noise->disconnect("changed", callable_mp(this, &ZN_SpotNoiseViewer::_on_noise_changed));
	}

	_noise = noise;

	if (_noise.is_valid()) {
		_noise->connect("changed", callable_mp(this, &ZN_SpotNoiseViewer::_on_noise_changed));
		set_process(true);
		update_preview();

	} else {
		set_process(false);
		_time_before_update = -1.f;
	}
}

void ZN_SpotNoiseViewer::_on_noise_changed() {
	_time_before_update = 0.5f;
}

void ZN_SpotNoiseViewer::_notification(int p_what) {
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

namespace {

void blend_rect(Image &im, Rect2i p_rect, Color color) {
	ZN_ASSERT_RETURN(!im.is_compressed());

	const Rect2i rect = p_rect.intersection(Rect2i(Vector2i(), im.get_size()));
	const Vector2i end = rect.get_end();

	for (int y = rect.position.y; y < end.y; ++y) {
		for (int x = rect.position.x; x < end.x; ++x) {
			Color c = im.get_pixel(x, y);
			c = c.blend(color);
			im.set_pixel(x, y, c);
		}
	}
}

void draw_grid(Image &im, float cell_size, Color color) {
	const Vector2i im_size = im.get_size();

	const int cells_x = im_size.x / cell_size + 1;
	const int cells_y = im_size.y / cell_size + 1;

	for (int cy = 0; cy < cells_y; ++cy) {
		const int y = static_cast<float>(cy) * cell_size;
		blend_rect(im, Rect2i(Vector2i(0, y), Vector2i(im_size.x, 1)), color);
	}

	for (int cx = 0; cx < cells_x; ++cx) {
		const int x = static_cast<float>(cx) * cell_size;
		blend_rect(im, Rect2i(Vector2i(x, 0), Vector2i(1, im_size.y)), color);
	}
}

} // namespace

// TODO Use thread?
void ZN_SpotNoiseViewer::update_preview() {
	const Vector2i preview_size(PREVIEW_WIDTH, PREVIEW_HEIGHT);

	Ref<Image> im = godot::create_empty_image(preview_size.x, preview_size.y, false, Image::FORMAT_RGB8);

	if (_noise.is_valid()) {
		for (int y = 0; y < preview_size.y; ++y) {
			for (int x = 0; x < preview_size.x; ++x) {
				const float v = _noise->get_noise_2d(x, y);
				im->set_pixel(x, y, Color(v, v, v));
			}
		}

		const float cell_size = _noise->get_cell_size();
		if (cell_size >= 10.f) {
			draw_grid(**im, cell_size, Color(0.5, 0.5, 1.0, 0.25));
		}
	}

	Ref<ImageTexture> tex = ImageTexture::create_from_image(im);
	_texture_rect->set_texture(tex);
}

void ZN_SpotNoiseViewer::_bind_methods() {}

} // namespace zylann
