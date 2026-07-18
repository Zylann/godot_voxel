#ifndef ZN_CHART_VIEW_H
#define ZN_CHART_VIEW_H

#include "../../util/containers/span.h"
#include "../../util/godot/classes/control.h"

class Line2D;

namespace zylann {

class ZN_ChartView : public Control {
	GDCLASS(ZN_ChartView, Control)
public:
	ZN_ChartView();

	void set_points(Span<const Vector2> points);
	void auto_fit_view(Vector2 margin_ratios);

private:
	void _notification(int p_what);

	void _draw();

private:
	PackedVector2Array _points;
	PackedVector2Array _visual_points;
	Vector2 _view_min;
	Vector2 _view_max;
	Line2D *_line_renderer = nullptr;
};

} // namespace zylann

#endif // ZN_CHART_VIEW_H
