#include "axes_3d_control.h"
#include "../../util/math/funcs.h"

namespace zylann {

void ZN_Axes3DControl::set_basis_3d(Basis basis) {
	if (basis != _basis) {
		_basis = basis;
		queue_redraw();
	}
}

void ZN_Axes3DControl::_notification(int p_what) {
	if (p_what == NOTIFICATION_DRAW) {
		draw();
	}
}

void ZN_Axes3DControl::draw() {
	const Vector3 x_axis_3d = _basis.get_column(Vector3::AXIS_X);
	const Vector3 y_axis_3d = _basis.get_column(Vector3::AXIS_Y);
	const Vector3 z_axis_3d = _basis.get_column(Vector3::AXIS_Z);

	const Vector2 x_axis_2d(x_axis_3d.x, -x_axis_3d.y);
	const Vector2 y_axis_2d(y_axis_3d.x, -y_axis_3d.y);
	const Vector2 z_axis_2d(z_axis_3d.x, -z_axis_3d.y);

	const Vector2 center = get_size() / 2;
	const float len = math::min(get_size().x, get_size().y) / 2;

	const Vector2 xp = center + x_axis_2d * len;
	const Vector2 yp = center + y_axis_2d * len;
	const Vector2 zp = center + z_axis_2d * len;

	const Color outline_color(0, 0, 0.5);
	const float outline_width = 4.f;
	const float line_width = 2.f;

	draw_line(center, xp, outline_color, outline_width);
	draw_line(center, yp, outline_color, outline_width);
	draw_line(center, zp, outline_color, outline_width);

	draw_line(center, xp, Color(1, 0, 0), line_width);
	draw_line(center, yp, Color(0, 1, 0), line_width);
	draw_line(center, zp, Color(0, 0, 1), line_width);
}

} // namespace zylann
