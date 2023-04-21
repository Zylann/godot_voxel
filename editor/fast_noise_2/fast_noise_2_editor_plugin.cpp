#include "fast_noise_2_editor_plugin.h"
#include "../../util/godot/classes/editor_inspector_plugin.h"
#include "../../util/godot/classes/editor_interface.h"
#include "../../util/godot/classes/popup_menu.h"
#include "../../util/godot/classes/texture_rect.h"
#include "../../util/noise/fast_noise_2.h"
#include "noise_analysis_window.h"

#include <core/core_string_names.h>
#include <editor/editor_scale.h>

namespace zylann {

class FastNoise2Viewer : public Control {
	GDCLASS(FastNoise2Viewer, Control)
public:
	static const int PREVIEW_WIDTH = 300;
	static const int PREVIEW_HEIGHT = 150;

	enum ContextMenuActions { //
		MENU_ANALYZE = 0
	};

	FastNoise2Viewer() {
		set_custom_minimum_size(Vector2(0, EDSCALE * PREVIEW_HEIGHT));

		_texture_rect = memnew(TextureRect);
		_texture_rect->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
		_texture_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_COVERED);
		add_child(_texture_rect);

		_context_menu = memnew(PopupMenu);
		_context_menu->add_item("Analyze...", MENU_ANALYZE);
		// TODO Add dialog to generate a texture?
		_context_menu->connect("id_pressed", callable_mp(this, &FastNoise2Viewer::_on_context_menu_id_pressed));
		add_child(_context_menu);

		// TODO SIMD level indicator
	}

	void gui_input(const Ref<InputEvent> &p_event) override {
		Ref<InputEventMouseButton> mb = p_event;
		if (mb.is_valid()) {
			if (mb->get_button_index() == MouseButton::RIGHT && mb->is_pressed()) {
				_context_menu->set_position(mb->get_global_position());
				_context_menu->popup();
			}
		}
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

	void set_noise_analysis_window(NoiseAnalysisWindow *win) {
		_noise_analysis_window = win;
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

	void _on_context_menu_id_pressed(int id) {
		switch (id) {
			case MENU_ANALYZE:
				ERR_FAIL_COND(_noise_analysis_window == nullptr);
				ERR_FAIL_COND(_noise.is_null());
				_noise_analysis_window->set_noise(_noise);
				_noise_analysis_window->popup_centered();
				break;

			default:
				ERR_PRINT(String("Unknown ID pressed: {0}").format(varray(id)));
				break;
		}
	}

	// TODO Use thread?
	void update_preview() {
		_noise->update_generator();

		const Vector2i preview_size(PREVIEW_WIDTH, PREVIEW_HEIGHT);

		Ref<Image> im = Image::create_empty(preview_size.x, preview_size.y, false, Image::FORMAT_RGB8);

		if (_noise.is_valid()) {
			_noise->generate_image(im, false);
		}

		Ref<ImageTexture> tex = ImageTexture::create_from_image(im);
		_texture_rect->set_texture(tex);
	}

	Ref<FastNoise2> _noise;
	float _time_before_update = -1.f;
	TextureRect *_texture_rect = nullptr;
	PopupMenu *_context_menu = nullptr;
	NoiseAnalysisWindow *_noise_analysis_window = nullptr;
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
			viewer->set_noise_analysis_window(_noise_analysis_window);
			add_custom_control(viewer);
			return;
		}
	}

	void set_noise_analysis_window(NoiseAnalysisWindow *noise_analysis_window) {
		_noise_analysis_window = noise_analysis_window;
	}

private:
	NoiseAnalysisWindow *_noise_analysis_window = nullptr;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FastNoise2EditorPlugin::FastNoise2EditorPlugin() {
	Control *base_control = get_editor_interface()->get_base_control();

	_noise_analysis_window = memnew(NoiseAnalysisWindow);
	base_control->add_child(_noise_analysis_window);

	Ref<FastNoise2EditorInspectorPlugin> plugin;
	plugin.instantiate();
	plugin->set_noise_analysis_window(_noise_analysis_window);
	add_inspector_plugin(plugin);
}

} // namespace zylann
