#include "chart_view.h"
#include "../../util/math/funcs.h"

#include <editor/editor_scale.h>
#include <scene/2d/line_2d.h>

namespace zylann {

ChartView::ChartView() {
	_line_renderer = memnew(Line2D);
	add_child(_line_renderer);

	set_clip_contents(true);

	_view_min = Vector2(-1, -1);
	_view_max = Vector2(1, 1);
}

void ChartView::set_points(Span<const Vector2> points) {
	_points.resize(points.size());
	for (size_t i = 0; i < points.size(); ++i) {
		_points.write[i] = points[i];
	}

	queue_redraw();
}

void ChartView::auto_fit_view(Vector2 margin_ratios) {
	if (_points.size() > 0) {
		Vector2 min_point = _points[0];
		Vector2 max_point = min_point;
		for (int i = 1; i < _points.size(); ++i) {
			const Vector2 p = _points[i];
			min_point.x = math::min(min_point.x, p.x);
			min_point.y = math::min(min_point.y, p.y);
			max_point.x = math::max(max_point.x, p.x);
			max_point.y = math::max(max_point.y, p.y);
		}
		_view_min = min_point;
		_view_max = max_point;
	}

	const Vector2 view_size = _view_max - _view_min;
	_view_min -= view_size * margin_ratios;
	_view_max += view_size * margin_ratios;

	queue_redraw();
}

void ChartView::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW:
			_draw();
			break;

			// case NOTIFICATION_RESIZED:
			// 	update();
			// 	break;

		default:
			break;
	}
}

void ChartView::_draw() {
	const Color bg_color(Color(0.15, 0.15, 0.15));
	const Color line_color(Color(0.8, 0.8, 0.8, 1.0));
	const Color x_axis_color(Color(1.0, 1.0, 1.0, 0.5));
	const Color y_axis_color(Color(1.0, 1.0, 1.0, 0.5));

	const Vector2 view_size_pixels = get_rect().size;

	// Background

	draw_style_box(get_theme_stylebox(SNAME("bg"), SNAME("Tree")), Rect2(Point2(), view_size_pixels));

	if (_view_min.is_equal_approx(_view_max) || _points.size() == 0) {
		return;
	}

	const Vector2 view_size_units = _view_max - _view_min;
	const Vector2 unit_to_pixels = view_size_pixels / view_size_units;

	const Transform2D m = Transform2D( //
			Vector2(unit_to_pixels.x, 0.0), // X axis
			Vector2(0.0, -unit_to_pixels.y), // Y axis
			Vector2(-_view_min.x * unit_to_pixels.x,
					view_size_pixels.y + _view_min.y * unit_to_pixels.y) // Pixel offset
	);

	//draw_set_transform_matrix();
	// Can't use this, because contrary to Godot 3, the thickness of lines drawn with `draw_polyline` will "properly"
	// be zoomed and stretched by the view transformation. It is also not possible to specify a different width to
	// account for that, because the scale factor is not uniform. So instead, we apply the view transform manually.

	// Lines

	if (_visual_points.size() != _points.size()) {
		_visual_points.resize(_points.size());
	}
	for (int i = 0; i < _points.size(); ++i) {
		_visual_points.write[i] = m.xform(_points[i]);
	}

	//draw_polyline(_visual_points, line_color, 2.0, true);
	// Even with thickness 2 and antialiasing, `draw_polyline` looks bad (dotted, jagged, inconsistent width).
	// Line2D has always been better.
	_line_renderer->set_width(2.f);
	_line_renderer->set_begin_cap_mode(Line2D::LINE_CAP_NONE);
	_line_renderer->set_end_cap_mode(Line2D::LINE_CAP_NONE);
	_line_renderer->set_antialiased(true);
	_line_renderer->set_joint_mode(Line2D::LINE_JOINT_BEVEL);
	_line_renderer->set_default_color(line_color);
	_line_renderer->set_points(_visual_points);

	// Axes

	draw_line(m.xform(Vector2(_view_min.x, 0.0)), m.xform(Vector2(_view_max.x, 0.0)), x_axis_color);
	draw_line(m.xform(Vector2(0.0, _view_min.y)), m.xform(Vector2(0.0, _view_max.y)), y_axis_color);

	// Markings

	Ref<Font> font = get_theme_font(SNAME("font"), SNAME("Label"));
	const int font_size = get_theme_font_size(SNAME("font_size"), SNAME("Label"));
	const Color text_color = get_theme_color(SNAME("font_color"), SNAME("Editor"));

	const int font_height = font->get_height(font_size);
	const Vector2 text_offset(2.f * EDSCALE, -2.f * EDSCALE);
	draw_string(font, Vector2(0, font_height) + text_offset, String::num_real(_view_max.y), HORIZONTAL_ALIGNMENT_LEFT,
			-1.f, font_size, text_color);
	draw_string(font, Vector2(0, view_size_pixels.y) + text_offset, String::num_real(_view_min.y),
			HORIZONTAL_ALIGNMENT_LEFT, -1.f, font_size, text_color);

	// TODO Draw hovered value
}

} // namespace zylann
