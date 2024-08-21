#include "control_sizer.h"
#include "../../util/errors.h"
#include "../../util/godot/classes/input_event_mouse_button.h"
#include "../../util/godot/classes/input_event_mouse_motion.h"
#include "../../util/godot/core/input_enums.h"
#include "../../util/godot/editor_scale.h"
#include "../../util/math/funcs.h"

namespace zylann {

ZN_ControlSizer::ZN_ControlSizer() {
	set_default_cursor_shape(Control::CURSOR_VSIZE);
	const real_t editor_scale = EDSCALE;
	set_custom_minimum_size(Vector2(0, editor_scale * 5));
}

void ZN_ControlSizer::set_target_control(Control *control) {
	_target_control.set(control);
}

#ifdef ZN_GODOT
void ZN_ControlSizer::gui_input(const Ref<InputEvent> &p_event) {
#elif defined(ZN_GODOT_EXTENSION)
void ZN_ControlSizer::_gui_input(const Ref<InputEvent> &p_event) {
#endif

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		Control *target_control = _target_control.get();
		ZN_ASSERT_RETURN(target_control != nullptr);

		if (mb->is_pressed()) {
			if (mb->get_button_index() == ::godot::MOUSE_BUTTON_LEFT) {
				_dragging = true;
			}
		} else {
			_dragging = false;
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		if (_dragging) {
			Control *target_control = _target_control.get();
			ZN_ASSERT_RETURN(target_control != nullptr);

			const Vector2 ms = target_control->get_custom_minimum_size();
			// Assuming the UI is not scaled
			const Vector2 rel = mm->get_relative();
			// Assuming vertical for now
			// TODO Clamp min_size to `target.get_minimum_size()`?
			target_control->set_custom_minimum_size(
					Vector2(ms.x, math::clamp<real_t>(ms.y + rel.y, _min_size, _max_size))
			);
		}
	}
}

void ZN_ControlSizer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_MOUSE_ENTER: {
			_mouse_inside = true;
			queue_redraw();
		} break;

		case NOTIFICATION_MOUSE_EXIT: {
			_mouse_inside = false;
			queue_redraw();
		} break;

		case NOTIFICATION_ENTER_TREE:
			cache_theme();
			break;

		case NOTIFICATION_THEME_CHANGED:
			cache_theme();
			break;

		case NOTIFICATION_DRAW: {
			if (_dragging || _mouse_inside) {
				draw_texture(_hover_icon, (get_size() - _hover_icon->get_size()) / 2);
			}
		} break;
	}
}

void ZN_ControlSizer::cache_theme() {
	// TODO I'd like to cache this, but `BIND_THEME_ITEM_CUSTOM` is not exposed to GDExtension...
	// TODO Have a framework-level StringName cache singleton
	_hover_icon = get_theme_icon("v_grabber", "SplitContainer");
}

void ZN_ControlSizer::_bind_methods() {}

} // namespace zylann
