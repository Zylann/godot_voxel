#include "fast_noise_lite_editor_plugin.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite_gradient.h"

#include <core/core_string_names.h>
#include <editor/editor_scale.h>

namespace zylann {

class ZN_FastNoiseLiteViewer : public Control {
	GDCLASS(ZN_FastNoiseLiteViewer, Control)
public:
	static const int PREVIEW_WIDTH = 300;
	static const int PREVIEW_HEIGHT = 150;

	ZN_FastNoiseLiteViewer() {
		set_custom_minimum_size(Vector2(0, EDSCALE * PREVIEW_HEIGHT));

		_texture_rect = memnew(TextureRect);
		_texture_rect->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
		_texture_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_COVERED);
		add_child(_texture_rect);
	}

	// ~ZN_FastNoiseLiteViewer() {
	// }

	void set_noise(Ref<ZN_FastNoiseLite> noise) {
		if (_noise == noise) {
			return;
		}

		if (_noise.is_valid()) {
			_noise->disconnect(CoreStringNames::get_singleton()->changed,
					callable_mp(this, &ZN_FastNoiseLiteViewer::_on_noise_changed));
		}

		_noise = noise;

		if (_noise.is_valid()) {
			set_noise_gradient(Ref<ZN_FastNoiseLiteGradient>());
			_noise->connect(CoreStringNames::get_singleton()->changed,
					callable_mp(this, &ZN_FastNoiseLiteViewer::_on_noise_changed));
			set_process(true);
			update_preview();

		} else {
			set_process(false);
			_time_before_update = -1.f;
		}
	}

	void set_noise_gradient(Ref<ZN_FastNoiseLiteGradient> noise_gradient) {
		if (_noise_gradient == noise_gradient) {
			return;
		}

		if (_noise_gradient.is_valid()) {
			_noise_gradient->disconnect(CoreStringNames::get_singleton()->changed,
					callable_mp(this, &ZN_FastNoiseLiteViewer::_on_noise_changed));
		}

		_noise_gradient = noise_gradient;

		if (_noise_gradient.is_valid()) {
			set_noise(Ref<ZN_FastNoiseLite>());
			_noise_gradient->connect(CoreStringNames::get_singleton()->changed,
					callable_mp(this, &ZN_FastNoiseLiteViewer::_on_noise_changed));
			set_process(true);
			update_preview();

		} else {
			set_process(false);
			_time_before_update = -1.f;
		}
	}

private:
	void _on_noise_changed() {
		_time_before_update = 0.5f;
	}

	void _notification(int p_what) {
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
	void update_preview() {
		const Vector2i preview_size(PREVIEW_WIDTH, PREVIEW_HEIGHT);

		Ref<Image> im;
		im.instantiate();
		im->create(preview_size.x, preview_size.y, false, Image::FORMAT_RGB8);

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

	static void _bind_methods() {
		// ClassDB::bind_method(D_METHOD("_on_noise_changed"), &FastNoiseLiteViewer::_on_noise_changed);
	}

	Ref<ZN_FastNoiseLite> _noise;
	Ref<ZN_FastNoiseLiteGradient> _noise_gradient;
	float _time_before_update = -1.f;
	TextureRect *_texture_rect = nullptr;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class ZN_FastNoiseLiteEditorInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(ZN_FastNoiseLiteEditorInspectorPlugin, EditorInspectorPlugin)
public:
	bool can_handle(Object *p_object) override {
		return Object::cast_to<ZN_FastNoiseLite>(p_object) != nullptr ||
				Object::cast_to<ZN_FastNoiseLiteGradient>(p_object);
	}

	void parse_begin(Object *p_object) override {
		ZN_FastNoiseLite *noise_ptr = Object::cast_to<ZN_FastNoiseLite>(p_object);
		if (noise_ptr) {
			Ref<ZN_FastNoiseLite> noise(noise_ptr);

			ZN_FastNoiseLiteViewer *viewer = memnew(ZN_FastNoiseLiteViewer);
			viewer->set_noise(noise);
			add_custom_control(viewer);
			return;
		}
		ZN_FastNoiseLiteGradient *noise_gradient_ptr = Object::cast_to<ZN_FastNoiseLiteGradient>(p_object);
		if (noise_gradient_ptr) {
			Ref<ZN_FastNoiseLiteGradient> noise_gradient(noise_gradient_ptr);

			ZN_FastNoiseLiteViewer *viewer = memnew(ZN_FastNoiseLiteViewer);
			viewer->set_noise_gradient(noise_gradient);
			add_custom_control(viewer);
			return;
		}
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ZN_FastNoiseLiteEditorPlugin::ZN_FastNoiseLiteEditorPlugin() {
	Ref<ZN_FastNoiseLiteEditorInspectorPlugin> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}

String ZN_FastNoiseLiteEditorPlugin::get_name() const {
	return ZN_FastNoiseLite::get_class_static();
}

} // namespace zylann
