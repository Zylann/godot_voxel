#ifndef CHART_VIEW_H
#define CHART_VIEW_H

#include "../../util/span.h"

#include "scene/gui/control.h"
#include <vector>

class Line2D;

namespace zylann {

class ChartView : public Control {
	GDCLASS(ChartView, Control)
public:
	ChartView();

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

#endif // CHART_VIEW_H
