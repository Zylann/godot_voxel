#include "heightmap_sdf.h"

const char *HeightmapSdf::MODE_HINT_STRING = "Vertical,VerticalAverage,Segment";

float HeightmapSdf::get_constrained_segment_sdf(float p_yp, float p_ya, float p_yb, float p_xb) {

	//  P
	//  .   B
	//  .  /
	//  . /      y
	//  ./       |
	//  A        o--x

	float s = p_yp >= p_ya ? 1 : -1;

	if (Math::absf(p_yp - p_ya) > 1.f && Math::absf(p_yp - p_yb) > 1.f) {
		return s;
	}

	Vector2 p(0, p_yp);
	Vector2 a(0, p_ya);
	Vector2 b(p_xb, p_yb);
	Vector2 closest_point;

	// TODO Optimize given the particular case we are in
	Vector2 n = b - a;
	real_t l2 = n.length_squared();
	if (l2 < 1e-20) {
		closest_point = a; // Both points are the same, just give any.
	} else {
		real_t d = n.dot(p - a) / l2;
		if (d <= 0.0) {
			closest_point = a; // Before first point.
		} else if (d >= 1.0) {
			closest_point = b; // After first point.
		} else {
			closest_point = a + n * d; // Inside.
		}
	}

	return s * closest_point.distance_to(p);
}
