#include "test_curve_range.h"
#include "../../generators/graph/range_utility.h"
#include "../../util/containers/std_vector.h"
#include "../../util/godot/classes/curve.h"
#include "../testing.h"

namespace zylann::voxel::tests {

void test_get_curve_monotonic_sections() {
	// This one is a bit annoying to test because Curve has float precision issues stemming from the bake() function
	struct L {
		static bool is_equal_approx(float a, float b) {
			return Math::is_equal_approx(a, b, 2.f * CURVE_RANGE_MARGIN);
		}
	};
	{
		// One segment going up
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(1, 1));
		StdVector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZN_TEST_ASSERT(sections.size() == 1);
		ZN_TEST_ASSERT(sections[0].x_min == 0.f);
		ZN_TEST_ASSERT(sections[0].x_max == 1.f);
		ZN_TEST_ASSERT(sections[0].y_min == 0.f);
		ZN_TEST_ASSERT(sections[0].y_max == 1.f);
		{
			math::Interval yi = get_curve_range(**curve, sections, math::Interval(0.f, 1.f));
			ZN_TEST_ASSERT(L::is_equal_approx(yi.min, 0.f));
			ZN_TEST_ASSERT(L::is_equal_approx(yi.max, 1.f));
		}
		{
			math::Interval yi = get_curve_range(**curve, sections, math::Interval(-2.f, 2.f));
			ZN_TEST_ASSERT(L::is_equal_approx(yi.min, 0.f));
			ZN_TEST_ASSERT(L::is_equal_approx(yi.max, 1.f));
		}
		{
			math::Interval xi(0.2f, 0.8f);
			math::Interval yi = get_curve_range(**curve, sections, xi);
			math::Interval yi_expected(curve->sample_baked(xi.min), curve->sample_baked(xi.max));
			ZN_TEST_ASSERT(L::is_equal_approx(yi.min, yi_expected.min));
			ZN_TEST_ASSERT(L::is_equal_approx(yi.max, yi_expected.max));
		}
	}
	{
		// One flat segment
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(1, 0));
		StdVector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZN_TEST_ASSERT(sections.size() == 1);
		ZN_TEST_ASSERT(sections[0].x_min == 0.f);
		ZN_TEST_ASSERT(sections[0].x_max == 1.f);
		ZN_TEST_ASSERT(sections[0].y_min == 0.f);
		ZN_TEST_ASSERT(sections[0].y_max == 0.f);
	}
	{
		// Two segments: going up, then flat
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.5, 1));
		curve->add_point(Vector2(1, 1));
		StdVector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZN_TEST_ASSERT(sections.size() == 1);
	}
	{
		// Two segments: flat, then up
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.5, 0));
		curve->add_point(Vector2(1, 1));
		StdVector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZN_TEST_ASSERT(sections.size() == 1);
	}
	{
		// Three segments: flat, then up, then flat
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.3, 0));
		curve->add_point(Vector2(0.6, 1));
		curve->add_point(Vector2(1, 1));
		StdVector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZN_TEST_ASSERT(sections.size() == 1);
	}
	{
		// Three segments: up, down, up
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.3, 1));
		curve->add_point(Vector2(0.6, 0));
		curve->add_point(Vector2(1, 1));
		StdVector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZN_TEST_ASSERT(sections.size() == 3);
		ZN_TEST_ASSERT(sections[0].x_min == 0.f);
		ZN_TEST_ASSERT(sections[2].x_max == 1.f);
	}
	{
		// Two segments: going up, then down
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.5, 1));
		curve->add_point(Vector2(1, 0));
		StdVector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZN_TEST_ASSERT(sections.size() == 2);
	}
	{
		// One segment, curved as a parabola going up then down
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0), 0.f, 1.f);
		curve->add_point(Vector2(1, 0));
		StdVector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZN_TEST_ASSERT(sections.size() == 2);
		ZN_TEST_ASSERT(sections[0].x_min == 0.f);
		ZN_TEST_ASSERT(sections[0].y_max >= 0.1f);
		ZN_TEST_ASSERT(sections[1].x_max == 1.f);
	}
}

} // namespace zylann::voxel::tests
