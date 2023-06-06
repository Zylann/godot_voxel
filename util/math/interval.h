#ifndef ZN_INTERVAL_H
#define ZN_INTERVAL_H

#include "funcs.h"
#include <iosfwd>
#include <limits>

namespace zylann {
namespace math {

// TODO Optimization: make template, I don't always need `real_t`, sometimes it uses doubles unnecessarily

// For interval arithmetic
struct Interval {
	// Both inclusive
	real_t min;
	real_t max;

	inline Interval() : min(0), max(0) {}

	inline Interval(real_t p_min, real_t p_max) : min(p_min), max(p_max) {
#if DEV_ENABLED
		ZN_ASSERT(p_min <= p_max);
#elif DEBUG_ENABLED
		// Don't crash but keep signaling
		_check_range_once();
#endif
	}

	void _check_range_once();

	inline static Interval from_single_value(real_t p_val) {
		return Interval(p_val, p_val);
	}

	inline static Interval from_unordered_values(real_t a, real_t b) {
		return Interval(math::min(a, b), math::max(a, b));
	}

	inline static Interval from_infinity() {
		return Interval(-std::numeric_limits<real_t>::infinity(), std::numeric_limits<real_t>::infinity());
	}

	inline static Interval from_union(const Interval a, const Interval b) {
		return Interval(math::min(a.min, b.min), math::max(a.max, b.max));
	}

	inline bool contains(real_t v) const {
		return v >= min && v <= max;
	}

	inline bool is_single_value() const {
		return min == max;
	}

	inline void add_point(real_t x) {
		if (x < min) {
			min = x;
		} else if (x > max) {
			max = x;
		}
	}

	inline Interval padded(real_t e) const {
		return Interval(min - e, max + e);
	}

	inline void add_interval(Interval other) {
		add_point(other.min);
		add_point(other.max);
	}

	inline real_t length() const {
		return max - min;
	}

	inline bool operator==(const Interval &other) const {
		return min == other.min && max == other.max;
	}

	inline bool operator!=(const Interval &other) const {
		return min != other.min || max != other.max;
	}

	inline Interval operator+(real_t x) const {
		return Interval{ min + x, max + x };
	}

	inline Interval operator+(const Interval &other) const {
		return Interval{ min + other.min, max + other.max };
	}

	inline void operator+=(const Interval &other) {
		*this = *this + other;
	}

	inline Interval operator-(real_t x) const {
		return Interval{ min - x, max - x };
	}

	inline Interval operator-(const Interval &other) const {
		return Interval{ min - other.max, max - other.min };
	}

	inline Interval operator-() const {
		return Interval{ -max, -min };
	}

	inline Interval operator*(real_t x) const {
		const real_t a = min * x;
		const real_t b = max * x;
		if (a < b) {
			return Interval(a, b);
		} else {
			return Interval(b, a);
		}
	}

	inline Interval operator*(const Interval &other) const {
		// Note, if the two operands have the same source (i.e you are doing x^2), this may lead to suboptimal results.
		// You may then prefer using a more dedicated function.
		const real_t a = min * other.min;
		const real_t b = min * other.max;
		const real_t c = max * other.min;
		const real_t d = max * other.max;
		return Interval{ math::min(a, b, c, d), math::max(a, b, c, d) };
	}

	inline void operator*=(real_t x) {
		*this = *this * x;
	}

	inline void operator*=(Interval x) {
		*this = *this * x;
	}

	inline Interval operator/(const Interval &other) const {
		if (other.is_single_value() && other.min == 0.f) {
			// Division by zero. In Voxel graph, we return 0.
			return Interval::from_single_value(0.f);
		}
		if (other.contains(0.f)) {
			// TODO May need something more precise
			return Interval::from_infinity();
		}
		const real_t a = min / other.min;
		const real_t b = min / other.max;
		const real_t c = max / other.min;
		const real_t d = max / other.max;
		return Interval{ math::min(a, b, c, d), math::max(a, b, c, d) };
	}

	inline Interval operator/(real_t x) const {
		// TODO Implement proper division by interval
		return *this * (1.0 / x);
	}

	inline void operator/=(real_t x) {
		*this = *this / x;
	}
};

struct Interval2 {
	Interval x;
	Interval y;
};

struct Interval3 {
	Interval x;
	Interval y;
	Interval z;
};

inline Interval operator*(real_t b, const Interval &a) {
	return a * b;
}

// Functions declared outside, so using intervals or numbers can be the same code (templatable)

inline Interval min_interval(const Interval &a, const Interval &b) {
	return Interval(min(a.min, b.min), min(a.max, b.max));
}

inline Interval max_interval(const Interval &a, const Interval &b) {
	return Interval(max(a.min, b.min), max(a.max, b.max));
}

inline Interval min_interval(const Interval &a, const real_t b) {
	return Interval(min(a.min, b), min(a.max, b));
}

inline Interval max_interval(const Interval &a, const real_t b) {
	return Interval(max(a.min, b), max(a.max, b));
}

inline Interval sqrt(const Interval &i) {
	// Avoiding negative numbers because they are undefined, also because VoxelGeneratorGraph defines its SQRT node this
	// way.
	// TODO Rename function `sqrt_or_zero` to be explicit about this?
	return Interval{ Math::sqrt(maxf(0, i.min)), Math::sqrt(maxf(0, i.max)) };
}

inline Interval abs(const Interval &i) {
	return Interval{ i.contains(0) ? 0 : min(Math::abs(i.min), Math::abs(i.max)),
		max(Math::abs(i.min), Math::abs(i.max)) };
}

inline Interval clamp(const Interval &i, const Interval &p_min, const Interval &p_max) {
	if (p_min.is_single_value() && p_max.is_single_value()) {
		return { clamp(i.min, p_min.min, p_max.min), clamp(i.max, p_min.min, p_max.min) };
	}
	if (i.min >= p_min.max && i.max <= p_max.min) {
		return i;
	}
	if (i.min >= p_max.max) {
		return Interval::from_single_value(p_max.max);
	}
	if (i.max <= p_min.min) {
		return Interval::from_single_value(p_min.min);
	}
	return Interval(p_min.min, p_max.max);
}

inline Interval lerp(const Interval &a, const Interval &b, const Interval &t) {
	if (t.is_single_value()) {
		return Interval(Math::lerp(a.min, b.min, t.min), Math::lerp(a.max, b.max, t.min));
	}

	const real_t v0 = a.min + t.min * (b.min - a.min);
	const real_t v1 = a.max + t.min * (b.min - a.max);
	const real_t v2 = a.min + t.max * (b.min - a.min);
	const real_t v3 = a.max + t.max * (b.min - a.max);
	const real_t v4 = a.min + t.min * (b.max - a.min);
	const real_t v5 = a.max + t.min * (b.max - a.max);
	const real_t v6 = a.min + t.max * (b.max - a.min);
	const real_t v7 = a.max + t.max * (b.max - a.max);

	return Interval(min(v0, v1, v2, v3, v4, v5, v6, v7), max(v0, v1, v2, v3, v4, v5, v6, v7));
}

inline Interval sin(const Interval &i) {
	if (i.is_single_value()) {
		return Interval::from_single_value(Math::sin(i.min));
	} else {
		// TODO more precision
		// Simplified
		return Interval(-1.f, 1.f);
	}
}

inline Interval atan(const Interval &t) {
	if (t.is_single_value()) {
		return Interval::from_single_value(Math::atan(t.min));
	}
	// arctan is monotonic
	return Interval{ Math::atan(t.min), Math::atan(t.max) };
}

struct OptionalInterval {
	Interval value;
	bool valid = false;
};

inline Interval atan2(const Interval &y, const Interval &x, OptionalInterval *secondary_output) {
	if (y.is_single_value() && x.is_single_value()) {
		return Interval::from_single_value(Math::atan2(y.min, x.max));
	}

	//          y
	//      1   |    0
	//          |
	//    ------o------x
	//          |
	//      2   |    3

	const bool in_nx = x.min <= 0.f;
	const bool in_px = x.max >= 0.f;
	const bool in_ny = y.min <= 0.f;
	const bool in_py = y.max >= 0.f;

	if (secondary_output != nullptr) {
		secondary_output->valid = false;
	}

	if (in_nx && in_px && in_ny && in_py) {
		// All quadrants
		return Interval{ -Math_PI, Math_PI };
	}

	const bool in_q0 = in_px && in_py;
	const bool in_q1 = in_nx && in_py;
	const bool in_q2 = in_nx && in_ny;
	const bool in_q3 = in_px && in_ny;

	// Double-quadrants

	if (in_q0 && in_q1) {
		return Interval(Math::atan2(y.min, x.max), Math::atan2(y.min, x.min));
	}
	if (in_q1 && in_q2) {
		if (secondary_output == nullptr) {
			// When crossing those two quadrants, the angle wraps from PI to -PI.
			// We would be forced to split the interval in two, but we have to return only one.
			// For correctness, we have to return the full range...
			return Interval{ -Math_PI, Math_PI };
		} else {
			// But, sometimes we can afford splitting the interval,
			// especially if our use case joins it back to one.
			// Q1
			secondary_output->value = Interval(Math::atan2(y.max, x.max), Math_PI);
			secondary_output->valid = true;
			// Q2
			return Interval(-Math_PI, Math::atan2(y.min, x.max));
		}
	}
	if (in_q2 && in_q3) {
		return Interval(Math::atan2(y.max, x.min), Math::atan2(y.max, x.max));
	}
	if (in_q3 && in_q0) {
		return Interval(Math::atan2(y.min, x.min), Math::atan2(y.max, x.min));
	}

	// Single quadrants

	if (in_q0) {
		return Interval(Math::atan2(y.min, x.max), Math::atan2(y.max, x.min));
	}
	if (in_q1) {
		return Interval(Math::atan2(y.max, x.max), Math::atan2(y.min, x.min));
	}
	if (in_q2) {
		return Interval(Math::atan2(y.max, x.min), Math::atan2(y.min, x.max));
	}
	if (in_q3) {
		return Interval(Math::atan2(y.min, x.min), Math::atan2(y.max, x.max));
	}

	// Bwarf.
	return Interval{ -Math_PI, Math_PI };
}

inline Interval floor(const Interval &i) {
	// Floor is monotonic so I guess we can just do that?
	return Interval(Math::floor(i.min), Math::floor(i.max));
}

inline Interval round(const Interval &i) {
	// Floor is monotonic so I guess we can just do that?
	return Interval(Math::floor(i.min + 0.5f), Math::floor(i.max + 0.5f));
}

inline Interval snapped(const Interval &p_value, const Interval &p_step) {
	// TODO Division by zero returns 0, which is different from Godot's stepify. May have to change that
	return floor(p_value / p_step + Interval::from_single_value(0.5f)) * p_step;
}

inline Interval wrapf(const Interval &x, const Interval &d) {
	return x - (d * floor(x / d));
}

inline Interval smoothstep(real_t p_from, real_t p_to, Interval p_weight) {
	if (Math::is_equal_approx(p_from, p_to)) {
		return Interval::from_single_value(p_from);
	}
	// Smoothstep is monotonic
	real_t v0 = smoothstep(p_from, p_to, p_weight.min);
	real_t v1 = smoothstep(p_from, p_to, p_weight.max);
	if (v0 <= v1) {
		return Interval(v0, v1);
	} else {
		return Interval(v1, v0);
	}
}

// Prefer this over x*x, this will provide a more optimal result
inline Interval squared(const Interval &x) {
	if (x.min < 0.f && x.max > 0.f) {
		// The interval includes 0
		return Interval{ 0.f, max(x.min * x.min, x.max * x.max) };
	}
	// The interval is only on one side of the parabola
	if (x.max <= 0.f) {
		// Negative side: monotonic descending
		return Interval{ x.max * x.max, x.min * x.min };
	} else {
		// Positive side: monotonic ascending
		return Interval{ x.min * x.min, x.max * x.max };
	}
}

// Prefer this instead of doing polynomials with a single interval, this will provide a more optimal result
inline Interval polynomial_second_degree(const Interval x, real_t a, real_t b, real_t c) {
	// a*x*x + b*x + c

	if (a == 0.f) {
		if (b == 0.f) {
			return Interval::from_single_value(c);
		} else {
			return b * x + c;
		}
	}

	const real_t parabola_x = -b / (2.f * a);

	const real_t y0 = a * x.min * x.min + b * x.min + c;
	const real_t y1 = a * x.max * x.max + b * x.max + c;

	if (x.min < parabola_x && x.max > parabola_x) {
		// The interval includes the tip
		const real_t parabola_y = a * parabola_x * parabola_x + b * parabola_x + c;
		if (a < 0) {
			return Interval(min(y0, y1), parabola_y);
		} else {
			return Interval(parabola_y, max(y0, y1));
		}
	}
	// The interval is only on one side of the parabola
	if ((a >= 0 && x.min >= parabola_x) || (a < 0 && x.max < parabola_x)) {
		// Monotonic increasing
		return Interval(y0, y1);
	} else {
		// Monotonic decreasing
		return Interval(y1, y0);
	}
}

// Prefer this over x*x*x, this will provide a more optimal result
inline Interval cubed(const Interval &x) {
	// x^3 is monotonic ascending
	const real_t minv = x.min * x.min * x.min;
	const real_t maxv = x.max * x.max * x.max;
	return Interval{ minv, maxv };
}

inline Interval get_length(const Interval &x, const Interval &y) {
	return sqrt(squared(x) + squared(y));
}

inline Interval get_length(const Interval &x, const Interval &y, const Interval &z) {
	return sqrt(squared(x) + squared(y) + squared(z));
}

inline Interval powi(Interval x, int pi) {
	const real_t pf = pi;
	if (pi >= 0) {
		if (pi % 2 == 1) {
			// Positive odd powers: ascending
			return Interval{ Math::pow(x.min, pf), Math::pow(x.max, pf) };
		} else {
			// Positive even powers: parabola
			if (x.min < 0.f && x.max > 0.f) {
				// The interval includes 0
				return Interval{ 0.f, max(Math::pow(x.min, pf), Math::pow(x.max, pf)) };
			}
			// The interval is only on one side of the parabola
			if (x.max <= 0.f) {
				// Negative side: monotonic descending
				return Interval{ Math::pow(x.max, pf), Math::pow(x.min, pf) };
			} else {
				// Positive side: monotonic ascending
				return Interval{ Math::pow(x.min, pf), Math::pow(x.max, pf) };
			}
		}
	} else {
		// TODO Negative integer powers
		return Interval::from_infinity();
	}
}

inline Interval pow(Interval x, float pf) {
	const int pi = pf;
	if (Math::is_equal_approx(pi, pf)) {
		return powi(x, pi);
	} else {
		// TODO Decimal powers
		return Interval::from_infinity();
	}
}

inline Interval pow(Interval x, Interval p) {
	if (p.is_single_value()) {
		return pow(x, p.min);
	} else {
		// TODO Varying powers
		return Interval::from_infinity();
	}
}

} // namespace math

std::stringstream &operator<<(std::stringstream &ss, const math::Interval &v);

} // namespace zylann

#endif // ZN_INTERVAL_H
