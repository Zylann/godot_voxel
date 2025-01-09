#ifndef ZN_GODOT_PAN_ZOOM_CONTAINER_H
#define ZN_GODOT_PAN_ZOOM_CONTAINER_H

#include "../containers/std_vector.h"
#include "classes/control.h"
#include "macros.h"

ZN_GODOT_FORWARD_DECLARE(class HScrollBar);
ZN_GODOT_FORWARD_DECLARE(class VScrollBar);

namespace zylann {

// Container allowing to move and zoom around its content, using the mouse or scrollbars.
// The first child will be scaled according to zoom, and moved around when scrolling.
// I originally tried to use ScrollView, but counter-intuitively, it sucked for this use case.
class ZN_PanZoomContainer : public Control {
	GDCLASS(ZN_PanZoomContainer, Control)
public:
	ZN_PanZoomContainer();

	void set_content_rect(const Rect2 rect);

private:
	void _notification(int p_what);
#if defined(ZN_GODOT)
	void gui_input(const Ref<InputEvent> &p_event) override;
#elif defined(ZN_GODOT_EXTENSION)
	void _gui_input(const Ref<InputEvent> &p_event) override;
#endif

	void on_h_scroll_bar_value_changed(float new_value);
	void on_v_scroll_bar_value_changed(float new_value);

	Control *get_content_root() const;
	void clamp_view_rect();
	Rect2 get_view_rect() const;
	Rect2 get_boundary_rect() const;
	void update_scrollbars();
	void update_content_root_transform();
	void set_zoom_level(const int level, const Vector2 pointed_position);

	static void _bind_methods();

	StdVector<float> _zoom_scales;
	uint8_t _zoom_level = 0;
	// Margin around the content to allow scrolling past
	float _content_margin = 1.f;
	// Area occupied by the content in world coordinates (not affected by zoom)
	Rect2 _content_rect;
	// Position of the top-left corner of the viewer in world coordinates
	Vector2 _view_pos;
	// Scrollbar values are `_view_pos`
	HScrollBar *_h_scroll_bar = nullptr;
	VScrollBar *_v_scroll_bar = nullptr;
	Ref<StyleBox> _background_style;
};

} // namespace zylann

#endif ZN_GODOT_PAN_ZOOM_CONTAINER_H
