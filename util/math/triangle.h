#ifndef ZN_TRIANGLE_H
#define ZN_TRIANGLE_H

#include "vector2f.h"
#include "vector3d.h"
#include "vector3f.h"

namespace zylann::math {

// Float version of Geometry::is_point_in_triangle()
inline bool is_point_in_triangle(const Vector2f &s, const Vector2f &a, const Vector2f &b, const Vector2f &c) {
	const Vector2f an = a - s;
	const Vector2f bn = b - s;
	const Vector2f cn = c - s;

	const bool orientation = cross(an, bn) > 0;

	if ((cross(bn, cn) > 0) != orientation) {
		return false;
	}

	return (cross(cn, an) > 0) == orientation;
}

struct TriangleIntersectionResult {
	enum Case { //
		INTERSECTION,
		PARALLEL,
		NO_INTERSECTION
	};

	Case case_id;
	double distance;
};

// TODO Templatize?

// Initially from Godot Engine, tweaked to suits needs
inline TriangleIntersectionResult ray_intersects_triangle(const Vector3f &p_from, const Vector3f &p_dir,
		const Vector3f &p_v0, const Vector3f &p_v1, const Vector3f &p_v2) {
	const Vector3f e1 = p_v1 - p_v0;
	const Vector3f e2 = p_v2 - p_v0;
	const Vector3f h = math::cross(p_dir, e2);
	const float a = math::dot(e1, h);

	if (Math::abs(a) < 0.00001f) {
		return { TriangleIntersectionResult::PARALLEL, -1 };
	}

	const float f = 1.0f / a;

	const Vector3f s = p_from - p_v0;
	const float u = f * math::dot(s, h);

	if ((u < 0.0f) || (u > 1.0f)) {
		return { TriangleIntersectionResult::NO_INTERSECTION, -1 };
	}

	const Vector3f q = math::cross(s, e1);

	const float v = f * math::dot(p_dir, q);

	if ((v < 0.0f) || (u + v > 1.0f)) {
		return { TriangleIntersectionResult::NO_INTERSECTION, -1 };
	}

	// At this stage we can compute t to find out where
	// the intersection point is on the line.
	const float t = f * math::dot(e2, q);

	if (t > 0.00001f) { // ray intersection
		//r_res = p_from + p_dir * t;
		return { TriangleIntersectionResult::INTERSECTION, t };

	} else { // This means that there is a line intersection but not a ray intersection.
		return { TriangleIntersectionResult::NO_INTERSECTION, -1 };
	}
}

inline TriangleIntersectionResult ray_intersects_triangle(const Vector3d &p_from, const Vector3d &p_dir,
		const Vector3d &p_v0, const Vector3d &p_v1, const Vector3d &p_v2) {
	const Vector3d e1 = p_v1 - p_v0;
	const Vector3d e2 = p_v2 - p_v0;
	const Vector3d h = math::cross(p_dir, e2);
	const double a = math::dot(e1, h);

	if (Math::abs(a) < 0.000000001) { // Parallel test.
		return { TriangleIntersectionResult::PARALLEL, -1 };
	}

	const double f = 1.0 / a;

	const Vector3d s = p_from - p_v0;
	const double u = f * math::dot(s, h);

	if ((u < 0.0) || (u > 1.0)) {
		return { TriangleIntersectionResult::NO_INTERSECTION, -1 };
	}

	const Vector3d q = math::cross(s, e1);
	const double v = f * math::dot(p_dir, q);

	if ((v < 0.0) || (u + v > 1.0)) {
		return { TriangleIntersectionResult::NO_INTERSECTION, -1 };
	}

	// At this stage we can compute t to find out where
	// the intersection point is on the line.
	const double t = f * math::dot(e2, q);

	if (t > 0.000000001) { // ray intersection
		//r_res = p_from + p_dir * t;
		return { TriangleIntersectionResult::INTERSECTION, t };

	} else { // This means that there is a line intersection but not a ray intersection.
		return { TriangleIntersectionResult::NO_INTERSECTION, -1 };
	}
}

// If you need to do a lot of raycasts on a triangle using the same direction every time
struct BakedIntersectionTriangleForFixedDirection {
	Vector3f v0;
	Vector3f e1; // v1 - v0
	Vector3f e2; // v2 - v0
	Vector3f h;
	float f;

	bool bake(Vector3f p_v0, Vector3f p_v1, Vector3f p_v2, Vector3f p_dir) {
		v0 = p_v0;

		e1 = p_v1 - v0;
		e2 = p_v2 - v0;
		h = math::cross(p_dir, e2);
		const float a = math::dot(e1, h);
		if (Math::abs(a) < 0.00001f) {
			// Parallel, will never hit
			return false;
		}
		f = 1.0f / a;
		return true;
	}

	// Note, `p_dir` must be the same value as used in `bake`
	inline TriangleIntersectionResult intersect(const Vector3f &p_from, const Vector3f &p_dir) {
		const Vector3f s = p_from - v0;
		const float u = f * math::dot(s, h);

		if ((u < 0.0f) || (u > 1.0f)) {
			return { TriangleIntersectionResult::NO_INTERSECTION, -1 };
		}

		const Vector3f q = math::cross(s, e1);

		const float v = f * math::dot(p_dir, q);

		if ((v < 0.0f) || (u + v > 1.0f)) {
			return { TriangleIntersectionResult::NO_INTERSECTION, -1 };
		}

		// At this stage we can compute t to find out where
		// the intersection point is on the line.
		const float t = f * math::dot(e2, q);

		if (t > 0.00001f) { // ray intersection
			//r_res = p_from + p_dir * t;
			return { TriangleIntersectionResult::INTERSECTION, t };

		} else { // This means that there is a line intersection but not a ray intersection.
			return { TriangleIntersectionResult::NO_INTERSECTION, -1 };
		}
	}
};

} // namespace zylann::math

#endif // ZN_TRIANGLE_H
