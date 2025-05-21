#ifndef ZN_HERMITE_H
#define ZN_HERMITE_H

#include "vector2f.h"
#include "vector3f.h"

namespace zylann::math {

// Calculates the cubic 2D Hermite interpolation between 2 points for which their derivative is also known.
inline float interpolate_hermite(const float p0, const float d0, const float p1, const float d1, const float x) {
	const float x2 = x * x;
	const float x3 = x2 * x;

	const float h0 = (2.f * x3 - 3.f * x2 + 1.f) * p0;
	const float h1 = (x3 - 2.f * x2 + x) * d0;
	const float h2 = (-2.f * x3 + 3.f * x2) * p1;
	const float h3 = (x3 - x2) * d1;

	return h0 + h1 + h2 + h3;
}

#if 1

// Calculates the cubic 2D Hermite interpolation within a quad of 4 points with known derivatives
inline float interpolate_hermite_2d(
		const Vector2f r, // Relative position within the 2x2 quad
		const Vector2f d, // Distance between the points (may default to (1,1))
		// Point values and their derivatives
		// x: value, y: x-derivative, z: y-derivative
		const Vector3f t00,
		const Vector3f t10,
		const Vector3f t01,
		const Vector3f t11
) {
	const float rx2 = r.x * r.x;
	const float ry2 = r.y * r.y;
	const float rx3 = rx2 * r.x;
	const float ry3 = ry2 * r.y;

	const float a0x = 2.f * rx3 - 3.f * rx2 + 1.f;
	const float a0y = 2.f * ry3 - 3.f * ry2 + 1.f;
	const float a1x = -2.f * rx3 + 3.f * rx2;
	const float a1y = -2.f * ry3 + 3.f * ry2;
	const float b0x = d.x * (rx3 - 2.f * rx2 + r.x);
	const float b0y = d.y * (ry3 - 2.f * ry2 + r.y);
	const float b1x = d.x * (rx3 - rx2);
	const float b1y = d.y * (ry3 - ry2);

	const float a00 = a0x * a0y;
	const float a10 = a1x * a0y;
	const float a01 = a0x * a1y;
	const float a11 = a1x * a1y;

	const Vector2f b00 = Vector2f(b0x * a0y, a0x * b0y);
	const Vector2f b10 = Vector2f(b1x * a0y, a1x * b0y);
	const Vector2f b01 = Vector2f(b0x * a1y, a0x * b1y);
	const Vector2f b11 = Vector2f(b1x * a1y, a1x * b1y);

	return //
			t00.x * a00 + //
			t10.x * a10 + //
			t01.x * a01 + //
			t11.x * a11 + //
			math::dot(t00.yz(), b00) + //
			math::dot(t10.yz(), b10) + //
			math::dot(t01.yz(), b01) + //
			math::dot(t11.yz(), b11);
}

#else

// Calculates the cubic 2D Hermite interpolation within a quad of 4 points with known derivatives
inline float interpolate_hermite_2d(
		const Vector2f r, // Relative position within the 2x2 quad
		const Vector2f d, // Distance between the points (may default to (1,1))
		// Point values and their derivatives
		// x: value, y: x-derivative, z: y-derivative
		const Vector3f t00,
		const Vector3f t10,
		const Vector3f t01,
		const Vector3f t11
) {
	struct L {
		// h0
		static inline float a0(const float x) {
			return 2.f * x * x * x - 3.f * x * x + 1.f;
		}
		// h1
		static inline float b0(const float x) {
			return x * x * x - 2.f * x * x + x;
		}
		// h2
		static inline float a1(const float x) {
			return -2.f * x * x * x + 3.f * x * x;
		}
		// h3
		static inline float b1(const float x) {
			return x * x * x - x * x;
		}
	};

	const float a0x = L::a0(r.x);
	const float a0y = L::a0(r.y);
	const float a1x = L::a1(r.x);
	const float a1y = L::a1(r.y);
	const float b0x = d.x * L::b0(r.x);
	const float b0y = d.y * L::b0(r.y);
	const float b1x = d.x * L::b1(r.x);
	const float b1y = d.y * L::b1(r.y);

	const float a00 = a0x * a0y;
	const float a10 = a1x * a0y;
	const float a01 = a0x * a1y;
	const float a11 = a1x * a1y;

	const Vector2f b00 = Vector2f(b0x * a0y, a0x * b0y);
	const Vector2f b10 = Vector2f(b1x * a0y, a1x * b0y);
	const Vector2f b01 = Vector2f(b0x * a1y, a0x * b1y);
	const Vector2f b11 = Vector2f(b1x * a1y, a1x * b1y);

	return //
			t00.x * a00 + //
			t10.x * a10 + //
			t01.x * a01 + //
			t11.x * a11 + //
			math::dot(t00.yz(), b00) + //
			math::dot(t10.yz(), b10) + //
			math::dot(t01.yz(), b01) + //
			math::dot(t11.yz(), b11);
}

#endif

} // namespace zylann::math

#endif // ZN_HERMITE_H
