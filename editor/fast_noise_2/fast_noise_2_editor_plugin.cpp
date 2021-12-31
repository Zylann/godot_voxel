#include "fast_noise_2_editor_plugin.h"
#include "../../util/noise/fast_noise_2.h"

#include <core/core_string_names.h>
#include <editor/editor_scale.h>

class FastNoise2Viewer : public Control {
	GDCLASS(FastNoise2Viewer, Control)
public:
	static const int PREVIEW_WIDTH = 300;
	static const int PREVIEW_HEIGHT = 150;

	FastNoise2Viewer() {
		set_custom_minimum_size(Vector2(0, EDSCALE * PREVIEW_HEIGHT));

		_texture_rect = memnew(TextureRect);
		_texture_rect->set_anchors_and_offsets_preset(Control::PRESET_WIDE);
		_texture_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_COVERED);
		add_child(_texture_rect);
	}

	void set_noise(Ref<FastNoise2> noise) {
		if (_noise == noise) {
			return;
		}

		if (_noise.is_valid()) {
			_noise->disconnect(
					CoreStringNames::get_singleton()->changed, callable_mp(this, &FastNoise2Viewer::_on_noise_changed));
		}

		_noise = noise;

		if (_noise.is_valid()) {
			_noise->connect(
					CoreStringNames::get_singleton()->changed, callable_mp(this, &FastNoise2Viewer::_on_noise_changed));
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
		_noise->update_generator();

		const Vector2i preview_size(PREVIEW_WIDTH, PREVIEW_HEIGHT);

		Ref<Image> im;
		im.instantiate();
		im->create(preview_size.x, preview_size.y, false, Image::FORMAT_RGB8);

		if (_noise.is_valid()) {
			_noise->generate_image(im, false);
		}

		Ref<ImageTexture> tex;
		tex.instantiate();
		tex->create_from_image(im);
		_texture_rect->set_texture(tex);
	}

	Ref<FastNoise2> _noise;
	float _time_before_update = -1.f;
	TextureRect *_texture_rect = nullptr;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FastNoise2EditorInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(FastNoise2EditorInspectorPlugin, EditorInspectorPlugin)
public:
	bool can_handle(Object *p_object) override {
		return Object::cast_to<FastNoise2>(p_object) != nullptr;
	}

	void parse_begin(Object *p_object) override {
		FastNoise2 *noise_ptr = Object::cast_to<FastNoise2>(p_object);
		if (noise_ptr) {
			Ref<FastNoise2> noise(noise_ptr);

			FastNoise2Viewer *viewer = memnew(FastNoise2Viewer);
			viewer->set_noise(noise);
			add_custom_control(viewer);
			return;
		}
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FastNoise2EditorPlugin::FastNoise2EditorPlugin(EditorNode *p_node) {
	Ref<FastNoise2EditorInspectorPlugin> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}
