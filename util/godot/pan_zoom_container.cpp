#include "pan_zoom_container.h"
#include "../math/funcs.h"
#include "classes/grid_container.h"
#include "classes/h_scroll_bar.h"
#include "classes/input.h"
#include "classes/input_event_mouse_button.h"
#include "classes/input_event_mouse_motion.h"
#include "classes/margin_container.h"
#include "classes/v_scroll_bar.h"
#include "core/mouse_button.h"

namespace zylann {

ZN_PanZoomContainer::ZN_PanZoomContainer() {
	set_clip_contents(true);

	MarginContainer *mc = memnew(MarginContainer);
	// mc->add_theme_constant_override("margin_left", 4);
	// mc->add_theme_constant_override("margin_right", 4);
	// mc->add_theme_constant_override("margin_top", 4);
	// mc->add_theme_constant_override("margin_bottom", 4);
	mc->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	// We have to let the mouse pass through to controls behind (and apparently, MOUSE_FILTER_PASS is NOT doing that)
	// because this container wraps the top layer, which we use in front to place overlays such as scrollbars
	mc->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);

	GridContainer *top_layer = memnew(GridContainer);
	top_layer->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	top_layer->set_columns(2);
	top_layer->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	mc->add_child(top_layer);
	// This control is always last even when adding extra controls as child
	add_child(mc, false, Node::INTERNAL_MODE_BACK);

	//                | ^
	//    Spacer      | S
	//                | v
	// ---------------+--
	// <---- S -----> |

	Control *spacer = memnew(Control);
	spacer->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	spacer->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	top_layer->add_child(spacer);

	_v_scroll_bar = memnew(VScrollBar);
	_v_scroll_bar->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	_v_scroll_bar->connect("value_changed", callable_mp(this, &ZN_PanZoomContainer::on_v_scroll_bar_value_changed));
	top_layer->add_child(_v_scroll_bar);

	_h_scroll_bar = memnew(HScrollBar);
	_h_scroll_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	_h_scroll_bar->connect("value_changed", callable_mp(this, &ZN_PanZoomContainer::on_h_scroll_bar_value_changed));
	top_layer->add_child(_h_scroll_bar);

	_zoom_scales.resize(6);
	_zoom_scales[0] = 1.f;
	_zoom_scales[1] = 2.f;
	_zoom_scales[2] = 3.f;
	_zoom_scales[3] = 4.f;
	_zoom_scales[4] = 6.f;
	_zoom_scales[5] = 8.f;
}

void ZN_PanZoomContainer::set_content_rect(const Rect2 rect) {
	_content_rect = rect;
	clamp_view_rect();
}

void ZN_PanZoomContainer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_RESIZED: {
			update_scrollbars();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_background_style = get_theme_stylebox("panel", "GraphEdit");
		} break;

		case NOTIFICATION_DRAW: {
			if (_background_style.is_valid()) {
				draw_style_box(_background_style, Rect2(Vector2(), get_size()));
			}
		} break;
	}
}

#if defined(ZN_GODOT)
void ZN_PanZoomContainer::gui_input(const Ref<InputEvent> &p_event) {
#elif defined(ZN_GODOT_EXTENSION)
void ZN_PanZoomContainer::_gui_input(const Ref<InputEvent> &p_event) {
#endif
	Ref<InputEventMouseButton> mouse_button_event = p_event;
	if (mouse_button_event.is_valid()) {
		if (mouse_button_event->is_pressed()) {
			switch (mouse_button_event->get_button_index()) {
				case ZN_GODOT_MouseButton_WHEEL_UP:
					set_zoom_level(static_cast<int>(_zoom_level) + 1, mouse_button_event->get_position());
					break;
				case ZN_GODOT_MouseButton_WHEEL_DOWN:
					set_zoom_level(static_cast<int>(_zoom_level) - 1, mouse_button_event->get_position());
					break;
				default:
					break;
			}
		}
		return;
	}

	Ref<InputEventMouseMotion> mouse_move_event = p_event;
	if (mouse_move_event.is_valid()) {
		const Input &input = *Input::get_singleton();
		if (input.is_mouse_button_pressed(ZN_GODOT_MouseButton_MIDDLE)) {
			// Pan
			const float zoom_scale = _zoom_scales[_zoom_level];
			const Vector2 motion = mouse_move_event->get_relative() / zoom_scale;
			_view_pos -= motion;
			clamp_view_rect();
			update_scrollbars();
			update_content_root_transform();
		}
		return;
	}
}

Control *ZN_PanZoomContainer::get_content_root() const {
	const int child_count = get_child_count(false);
	for (int i = 0; i < child_count; ++i) {
		Node *child = get_child(i, false);
		Control *child_control = Object::cast_to<Control>(child);
		if (child_control != nullptr) {
			return child_control;
		}
	}
	return nullptr;
}

Rect2 ZN_PanZoomContainer::get_view_rect() const {
	const float zoom_scale = _zoom_scales[_zoom_level];
	const Vector2 size = get_size();
	return Rect2(_view_pos, size / zoom_scale);
}

Rect2 ZN_PanZoomContainer::get_boundary_rect() const {
	return _content_rect.grow(_content_margin);
}

inline bool is_range_full(const Range &range) {
	return range.get_max() - range.get_min() <= range.get_page();
}

void ZN_PanZoomContainer::update_scrollbars() {
	const Rect2 boundary_rect = get_boundary_rect();
	const Rect2 view_rect = get_view_rect();

	_h_scroll_bar->set_min(boundary_rect.position.x);
	_h_scroll_bar->set_max(boundary_rect.position.x + boundary_rect.size.x);
	_h_scroll_bar->set_page(view_rect.size.x);
	_h_scroll_bar->set_value_no_signal(view_rect.position.x);

	_v_scroll_bar->set_min(boundary_rect.position.y);
	_v_scroll_bar->set_max(boundary_rect.position.y + boundary_rect.size.y);
	_v_scroll_bar->set_page(view_rect.size.y);
	_v_scroll_bar->set_value_no_signal(view_rect.position.y);

	// TODO Figure out how to hide scrollbars without screwing up the layout because of GridContainer
	// _h_scroll_bar->set_visible(!is_range_full(*_h_scroll_bar));
	// _v_scroll_bar->set_visible(!is_range_full(*_v_scroll_bar));
}

void ZN_PanZoomContainer::on_h_scroll_bar_value_changed(float new_value) {
	_view_pos.x = new_value;
	update_content_root_transform();
}

void ZN_PanZoomContainer::on_v_scroll_bar_value_changed(float new_value) {
	_view_pos.y = new_value;
	update_content_root_transform();
}

void ZN_PanZoomContainer::update_content_root_transform() {
	Control *content_root = get_content_root();
	if (content_root == nullptr) {
		return;
	}
	const float zoom_scale = _zoom_scales[_zoom_level];
	const Vector2 pos = (_content_rect.position - _view_pos) * zoom_scale;
	content_root->set_position(pos);
	content_root->set_scale(Vector2(zoom_scale, zoom_scale));
}

void ZN_PanZoomContainer::clamp_view_rect() {
	const Rect2 boundary_rect = get_boundary_rect();
	const Rect2 view_rect = get_view_rect();
	_view_pos = _view_pos.min(boundary_rect.position + boundary_rect.size - view_rect.size);
	_view_pos = _view_pos.max(boundary_rect.position);
}

void ZN_PanZoomContainer::set_zoom_level(const int level, const Vector2 pointed_position) {
	const Rect2 prev_rect = get_view_rect();

	const uint8_t checked_level =
			static_cast<uint8_t>(math::clamp(level, 0, static_cast<int>(_zoom_scales.size()) - 1));
	if (checked_level == _zoom_level) {
		return;
	}
	_zoom_level = checked_level;

	const Rect2 new_rect = get_view_rect();
	const Vector2 size = get_size();
	_view_pos += (prev_rect.size - new_rect.size) * (pointed_position / size);

	clamp_view_rect();
	update_scrollbars();
	update_content_root_transform();
}

void ZN_PanZoomContainer::_bind_methods() {
	//
}

} // namespace zylann
